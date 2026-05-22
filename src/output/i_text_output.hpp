#pragma once
#include <cstdint>
#include <string>

namespace vim {

class ITextOutput {
public:
    virtual ~ITextOutput() = default;

    virtual bool type_text(const std::string& utf8_text) = 0;
    virtual bool type_unicode_codepoint(uint32_t codepoint) = 0;
    virtual bool type_special_key(const std::string& key_name) = 0;
    virtual void set_typing_delay(int delay_ms) = 0;
    virtual const char* backend_name() const = 0;
};

} // namespace vim
