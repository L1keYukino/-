#include "src/llm/openai_engine.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <future>
#include <sstream>
#include <spdlog/spdlog.h>

#ifdef _WIN32
  #include <windows.h>
  #include <winhttp.h>
  #pragma comment(lib, "winhttp.lib")
  #define VIM_HAS_HTTP 1
#else
  #define VIM_HAS_HTTP 0
#endif

namespace vim {

static std::string utf8_to_json_escaped(const std::string& s) {
    std::ostringstream out;
    for (char c : s) {
        switch (c) {
            case '"':  out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\n': out << "\\n";  break;
            case '\r': out << "\\r";  break;
            case '\t': out << "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20)
                    out << "\\u" << std::hex << static_cast<int>(c);
                else
                    out << c;
        }
    }
    return out.str();
}

static std::string build_chat_json(const LLMRequest& req, const std::string& model) {
    std::ostringstream body;
    body << "{\"model\":\"" << model << "\",\"messages\":[";
    for (std::size_t i = 0; i < req.messages.size(); ++i) {
        if (i > 0) body << ",";
        body << "{\"role\":\"" << req.messages[i].role
             << "\",\"content\":\"" << utf8_to_json_escaped(req.messages[i].content) << "\"}";
    }
    body << "],\"temperature\":" << req.temperature
         << ",\"max_tokens\":" << req.max_tokens;
    if (req.stream) body << ",\"stream\":true";
    body << "}";
    return body.str();
}

struct OpenAIEngine::Impl {
    // Nothing to store (WinHTTP is stateless per-request)
};

OpenAIEngine::OpenAIEngine()
    : impl_(std::make_unique<Impl>())
{
}

OpenAIEngine::~OpenAIEngine() = default;

bool OpenAIEngine::initialize(const std::string& api_key, const std::string& model,
                               const std::string& endpoint_url) {
    api_key_ = api_key;
    model_ = model;
    endpoint_url_ = endpoint_url;

    if (api_key.empty()) {
        spdlog::warn("OpenAI: no API key provided — engine disabled");
        return false;
    }

    spdlog::info("OpenAI: configured (model={}, endpoint={})", model, endpoint_url);
    ready_.store(true);
    return true;
}

std::future<LLMResponse> OpenAIEngine::process_async(const LLMRequest& request) {
    return std::async(std::launch::async, [this, request]() {
        LLMRequest req = request;
        req.stream = false;
        std::string body = build_chat_json(req, model_);
        return send_http_request(body, nullptr);
    });
}

// Provide a concrete definition for the overload the header declares
LLMResponse OpenAIEngine::process_streaming(const LLMRequest& request,
                                             const StreamingCallback& cb) {
    if (!ready_.load()) {
        return llm_error("OpenAI engine not ready");
    }

    LLMRequest req = request;
    req.stream = true;
    std::string body = build_chat_json(req, model_);
    return send_http_request(body, &cb);
}


LLMResponse OpenAIEngine::send_http_request(const std::string& body,
                                             const StreamingCallback* cb) {
    LLMResponse resp;

#if VIM_HAS_HTTP
    // Parse endpoint URL
    std::string host, path;
    bool use_https = false;
    const char* url = endpoint_url_.c_str();

    if (strncmp(url, "https://", 8) == 0) {
        use_https = true;
        url += 8;
    } else if (strncmp(url, "http://", 7) == 0) {
        url += 7;
    }

    const char* slash = strchr(url, '/');
    host.assign(url, slash ? static_cast<std::size_t>(slash - url) : strlen(url));
    path = slash ? slash : "/v1/chat/completions";

    DWORD port = use_https ? 443 : 80;

    HINTERNET hSession = WinHttpOpen(L"VoiceInputMethod/0.2",
                                      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME,
                                      WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        resp.success = false;
        resp.error_message = "WinHttpOpen failed";
        return resp;
    }

    std::wstring whost(host.begin(), host.end());
    HINTERNET hConnect = WinHttpConnect(hSession, whost.c_str(), port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        resp.success = false;
        resp.error_message = "WinHttpConnect failed";
        return resp;
    }

    std::wstring wpath(path.begin(), path.end());
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", wpath.c_str(),
                                             nullptr, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES,
                                             use_https ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        resp.success = false;
        resp.error_message = "WinHttpOpenRequest failed";
        return resp;
    }

    // Headers
    std::wstring headers = L"Content-Type: application/json\r\n";
    std::string auth = "Authorization: Bearer " + api_key_ + "\r\n";
    headers.append(auth.begin(), auth.end());

    WinHttpAddRequestHeaders(hRequest, headers.c_str(),
                              static_cast<DWORD>(headers.size()),
                              WINHTTP_ADDREQ_FLAG_ADD);

    WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                       const_cast<char*>(body.data()),
                       static_cast<DWORD>(body.size()),
                       static_cast<DWORD>(body.size()), 0);
    WinHttpReceiveResponse(hRequest, nullptr);

    // Read response
    std::ostringstream response_body;
    DWORD bytes_read = 0;
    char buffer[4096];
    while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytes_read) && bytes_read > 0) {
        response_body.write(buffer, static_cast<std::streamsize>(bytes_read));
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    // Minimal JSON parse: extract "content" from "choices"[0]."message"."content"
    std::string raw = response_body.str();
    auto pos = raw.find("\"content\":\"");
    if (pos != std::string::npos) {
        pos += 11; // skip "content":"
        auto end = raw.find("\"", pos);
        if (end != std::string::npos) {
            resp.text = raw.substr(pos, end - pos);
            resp.success = true;
            return resp;
        }
    }

    resp.success = false;
    resp.error_message = "Failed to parse OpenAI response";
    (void)cb;
    return resp;
#else
    (void)cb;
    (void)body;
    resp.success = false;
    resp.error_message = "HTTP not available (non-Windows platform)";
    return resp;
#endif
}

bool OpenAIEngine::is_ready() const {
    return ready_.load();
}

} // namespace vim
