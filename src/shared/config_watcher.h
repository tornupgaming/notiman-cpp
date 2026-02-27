#pragma once

#include <string>

#include <windows.h>

namespace notiman {

// Blocks while watching a directory handle and posts a message when filename changes.
void run_config_watcher(HANDLE dir_handle, HWND hwnd, UINT changed_message, const std::wstring& filename);

}  // namespace notiman
