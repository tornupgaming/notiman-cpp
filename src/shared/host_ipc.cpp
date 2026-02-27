#include "host_ipc.h"

#include <windows.h>

namespace notiman {

bool send_payload_to_host(const NotificationPayload& payload, std::string* error_message) {
    HWND host_window = FindWindowW(kHostWindowClassName, nullptr);
    if (!host_window) {
        if (error_message != nullptr) {
            *error_message = "Host window not found.";
        }
        return false;
    }

    const std::string json_str = payload.to_json().dump();
    COPYDATASTRUCT cds = {};
    cds.dwData = 1;
    cds.cbData = static_cast<DWORD>(json_str.size() + 1);
    cds.lpData = const_cast<char*>(json_str.c_str());

    const LRESULT result = SendMessageW(host_window, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds));
    if (result == 1) {
        return true;
    }

    if (error_message != nullptr) {
        *error_message = "Host rejected notification payload.";
    }
    return false;
}

}  // namespace notiman
