#pragma once
#include "src/hotkey/i_hotkey_manager.hpp"
#include <atomic>
#include <memory>
#include <string>

namespace vim {

// Win32 RegisterHotKey global hotkey manager.
// Creates a message-only window to receive WM_HOTKEY messages.
class Win32HotkeyManager : public IHotkeyManager {
public:
    Win32HotkeyManager();
    ~Win32HotkeyManager() override;

    bool register_hotkey(HotkeyMod modifiers, uint32_t virtual_key,
                         HotkeyCallback callback) override;
    void unregister_hotkey() override;
    bool pump_messages() override;
    void run_message_loop() override;
    void quit() override;

    // Internal access for window proc (defined in .cpp)
    HotkeyCallback* get_callback();
    unsigned get_hotkey_vk() const;
    void toggle_ptt();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::atomic<bool> running_{false};
    std::atomic<bool> ptt_active_{false};
    int hotkey_id_ = 1;
};

} // namespace vim
