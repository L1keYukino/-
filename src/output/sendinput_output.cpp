#include "src/output/sendinput_output.hpp"
#include <algorithm>
#include <cstdio>
#include <thread>
#include <spdlog/spdlog.h>

#ifdef _WIN32
  #include <windows.h>
  #define VIM_HAS_SENDINPUT 1
#else
  #define VIM_HAS_SENDINPUT 0
#endif

namespace vim {

// ─── UTF-8 decoder (simple, no library needed) ──────────

static char32_t decode_utf8(const char*& p, const char* end) {
    if (p >= end) return 0;
    unsigned char c = static_cast<unsigned char>(*p);

    if (c < 0x80) { p++; return c; }

    char32_t cp = 0;
    int extra = 0;

    if ((c & 0xE0) == 0xC0)      { cp = c & 0x1F; extra = 1; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
    else { p++; return 0xFFFD; } // invalid

    p++;
    for (int i = 0; i < extra && p < end; ++i, ++p) {
        unsigned char follow = static_cast<unsigned char>(*p);
        if ((follow & 0xC0) != 0x80) return 0xFFFD;
        cp = (cp << 6) | (follow & 0x3F);
    }
    return cp;
}

SendInputOutput::SendInputOutput() = default;
SendInputOutput::~SendInputOutput() = default;

bool SendInputOutput::type_text(const std::string& utf8_text) {
    if (utf8_text.empty()) return true;

#if VIM_HAS_SENDINPUT
    const char* p = utf8_text.c_str();
    const char* end = p + utf8_text.size();
    int typed = 0;

    while (p < end) {
        char32_t ch = decode_utf8(p, end);
        if (ch == 0) break;
        if (type_char(ch)) {
            ++typed;
            if (delay_ms_ > 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms_));
        }
    }

    spdlog::debug("SendInput: typed {} characters", typed);
    return typed > 0;
#else
    spdlog::warn("SendInput: not available (non-Windows)");
    (void)utf8_text;
    return false;
#endif
}

bool SendInputOutput::type_char(char32_t ch) {
#if VIM_HAS_SENDINPUT
    INPUT inputs[2]{};
    int input_count = 0;

    if (ch <= 0x7F) {
        // ASCII: direct key event
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = 0;
        inputs[0].ki.wScan = static_cast<WORD>(ch);
        inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;
        inputs[0].ki.time = 0;
        inputs[0].ki.dwExtraInfo = 0;

        inputs[1] = inputs[0];
        inputs[1].ki.dwFlags |= KEYEVENTF_KEYUP;
        input_count = 2;
    } else if (ch <= 0xFFFF) {
        // BMP Unicode: KEYEVENTF_UNICODE
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = 0;
        inputs[0].ki.wScan = static_cast<WORD>(ch);
        inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;
        inputs[0].ki.time = 0;
        inputs[0].ki.dwExtraInfo = 0;

        inputs[1] = inputs[0];
        inputs[1].ki.dwFlags |= KEYEVENTF_KEYUP;
        input_count = 2;
    } else {
        // Supplementary plane: use Alt+Numpad decimal
        // First type Alt down
        INPUT alt_down{};
        alt_down.type = INPUT_KEYBOARD;
        alt_down.ki.wVk = VK_MENU;
        alt_down.ki.dwFlags = 0;
        SendInput(1, &alt_down, sizeof(INPUT));

        // Type each digit
        char buf[16];
        snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(ch));
        for (const char* d = buf; *d; ++d) {
            INPUT digit{};
            digit.type = INPUT_KEYBOARD;
            digit.ki.wVk = 0;
            digit.ki.wScan = static_cast<WORD>(*d);
            digit.ki.dwFlags = KEYEVENTF_UNICODE;
            SendInput(1, &digit, sizeof(INPUT));

            INPUT digit_up = digit;
            digit_up.ki.dwFlags |= KEYEVENTF_KEYUP;
            SendInput(1, &digit_up, sizeof(INPUT));
        }

        // Alt up
        INPUT alt_up = alt_down;
        alt_up.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &alt_up, sizeof(INPUT));
        return true;
    }

    if (input_count > 0) {
        UINT sent = SendInput(static_cast<UINT>(input_count), inputs, sizeof(INPUT));
        if (sent != static_cast<UINT>(input_count)) {
            spdlog::debug("SendInput failed for U+{:04X}: sent {}/{}",
                          static_cast<unsigned>(ch), sent, input_count);
            return false;
        }
    }
    return true;
#else
    (void)ch;
    return false;
#endif
}

bool SendInputOutput::type_unicode_codepoint(uint32_t codepoint) {
    return type_char(static_cast<char32_t>(codepoint));
}

bool SendInputOutput::type_special_key(const std::string& key_name) {
#if VIM_HAS_SENDINPUT
    WORD vk = 0;

    if (key_name == "Enter")       vk = VK_RETURN;
    else if (key_name == "Tab")     vk = VK_TAB;
    else if (key_name == "Backspace") vk = VK_BACK;
    else if (key_name == "Escape")  vk = VK_ESCAPE;
    else if (key_name == "Space")   vk = VK_SPACE;
    else if (key_name == "Left")    vk = VK_LEFT;
    else if (key_name == "Right")   vk = VK_RIGHT;
    else if (key_name == "Up")      vk = VK_UP;
    else if (key_name == "Down")    vk = VK_DOWN;
    else if (key_name == "Home")    vk = VK_HOME;
    else if (key_name == "End")     vk = VK_END;
    else {
        spdlog::warn("SendInput: unknown special key '{}'", key_name);
        return false;
    }

    INPUT inputs[2]{};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = vk;
    inputs[0].ki.dwFlags = 0;

    inputs[1] = inputs[0];
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(2, inputs, sizeof(INPUT));
    return true;
#else
    (void)key_name;
    return false;
#endif
}

} // namespace vim
