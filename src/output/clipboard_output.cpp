#include "src/output/clipboard_output.hpp"
#include <cstdio>
#include <thread>
#include <spdlog/spdlog.h>

#ifdef _WIN32
  #include <windows.h>
  #define VIM_HAS_CLIPBOARD 1
#else
  #define VIM_HAS_CLIPBOARD 0
#endif

namespace vim {

ClipboardOutput::ClipboardOutput() = default;
ClipboardOutput::~ClipboardOutput() = default;

bool ClipboardOutput::type_text(const std::string& utf8_text) {
    if (utf8_text.empty()) return true;

#if VIM_HAS_CLIPBOARD
    // Convert UTF-8 to wide string
    int wide_len = MultiByteToWideChar(CP_UTF8, 0, utf8_text.c_str(), -1, nullptr, 0);
    if (wide_len <= 0) {
        spdlog::error("Clipboard: UTF-8 → UTF-16 conversion failed");
        return false;
    }

    std::wstring wtext(static_cast<std::size_t>(wide_len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8_text.c_str(), -1, &wtext[0], wide_len);

    // Open clipboard
    if (!OpenClipboard(nullptr)) {
        spdlog::error("Clipboard: OpenClipboard failed");
        return false;
    }

    EmptyClipboard();

    // Allocate global memory
    std::size_t byte_size = wtext.size() * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, byte_size);
    if (!hMem) {
        CloseClipboard();
        return false;
    }

    void* dst = GlobalLock(hMem);
    std::memcpy(dst, wtext.c_str(), byte_size);
    GlobalUnlock(hMem);

    SetClipboardData(CF_UNICODETEXT, hMem);
    CloseClipboard();

    // Simulate Ctrl+V to paste
    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms_));

    INPUT inputs[4]{};
    // Ctrl down
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;
    // V down
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 'V';
    // V up
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = 'V';
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
    // Ctrl up
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(4, inputs, sizeof(INPUT));

    spdlog::debug("Clipboard: pasted {} chars via Ctrl+V", utf8_text.size());
    return true;
#else
    (void)utf8_text;
    return false;
#endif
}

bool ClipboardOutput::type_unicode_codepoint(uint32_t /*codepoint*/) {
    // Single codepoint not supported — use type_text instead
    return false;
}

bool ClipboardOutput::type_special_key(const std::string& key_name) {
#if VIM_HAS_CLIPBOARD
    WORD vk = 0;
    if (key_name == "Enter")       vk = VK_RETURN;
    else if (key_name == "Tab")     vk = VK_TAB;
    else if (key_name == "Backspace") vk = VK_BACK;
    else if (key_name == "Escape")  vk = VK_ESCAPE;
    else return false;

    INPUT inputs[2]{};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = vk;
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
