#include "config_watcher.h"

namespace notiman {

void run_config_watcher(HANDLE dir_handle, HWND hwnd, UINT changed_message, const std::wstring& filename) {
    char buf[4096];
    DWORD bytes_returned;
    while (ReadDirectoryChangesW(
        dir_handle,
        buf,
        sizeof(buf),
        FALSE,
        FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_FILE_NAME,
        &bytes_returned,
        nullptr,
        nullptr)) {
        const auto* info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(buf);
        for (;;) {
            std::wstring changed(info->FileName, info->FileNameLength / sizeof(wchar_t));
            if (_wcsicmp(changed.c_str(), filename.c_str()) == 0) {
                PostMessageW(hwnd, changed_message, 0, 0);
                break;
            }
            if (info->NextEntryOffset == 0) {
                break;
            }
            info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(
                reinterpret_cast<const char*>(info) + info->NextEntryOffset);
        }
    }
}

}  // namespace notiman
