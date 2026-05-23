#pragma once
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace vim {

struct EngineConfig;

// Win32 modal dialog for settings
bool show_settings_dialog(HINSTANCE hinst, HWND parent, EngineConfig& config);

} // namespace vim
