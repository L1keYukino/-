#pragma once
#include "src/output/i_text_output.hpp"
#include <string>

namespace vim {

// Clipboard-based fallback output.
// Copies text to clipboard, then simulates Ctrl+V to paste.
// Useful when SendInput Unicode is unreliable.
class ClipboardOutput : public ITextOutput {
public:
    ClipboardOutput();
    ~ClipboardOutput() override;

    bool type_text(const std::string& utf8_text) override;
    bool type_unicode_codepoint(uint32_t codepoint) override;
    bool type_special_key(const std::string& key_name) override;
    void set_typing_delay(int delay_ms) override { delay_ms_ = delay_ms; }
    const char* backend_name() const override { return "Clipboard"; }

private:
    int delay_ms_ = 50; // longer delay for clipboard operations
};

} // namespace vim
