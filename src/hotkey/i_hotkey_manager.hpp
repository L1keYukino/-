#pragma once
#include <functional>
#include "src/core/types.hpp"

namespace vim {

using HotkeyCallback = std::function<void(bool key_down)>;

class IHotkeyManager {
public:
    virtual ~IHotkeyManager() = default;

    virtual bool register_hotkey(HotkeyMod modifiers, uint32_t virtual_key,
                                 HotkeyCallback callback) = 0;
    virtual void unregister_hotkey() = 0;
    virtual bool pump_messages() = 0;
    virtual void run_message_loop() = 0;
    virtual void quit() = 0;
};

} // namespace vim
