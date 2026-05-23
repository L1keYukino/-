#pragma once
#include "src/output/i_text_output.hpp"
#include <string>

namespace vim {

// Windows SendInput-based keyboard simulation.
// Types UTF-8 text as keystrokes into the focused window.
// Falls back to Alt+Numpad for characters outside the current keyboard layout.
class SendInputOutput : public ITextOutput {
public:
    SendInputOutput();
    ~SendInputOutput() override;

    bool type_text(const std::string& utf8_text) override;
    bool type_unicode_codepoint(uint32_t codepoint) override;
    bool type_special_key(const std::string& key_name) override;
    void set_typing_delay(int delay_ms) override { delay_ms_ = delay_ms; }
    const char* backend_name() const override { return "SendInput"; }

private:
    bool type_char(char32_t ch);
    int delay_ms_ = 10;
};

} // namespace vim
