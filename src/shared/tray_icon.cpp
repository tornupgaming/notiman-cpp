#include "tray_icon.h"

namespace notiman {

void init_tray_icon(
    NOTIFYICONDATAW& nid,
    HWND hwnd,
    UINT icon_id,
    UINT callback_message,
    const wchar_t* tooltip_text) {
    nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hwnd;
    nid.uID = icon_id;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = callback_message;
    nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    lstrcpynW(nid.szTip, tooltip_text, static_cast<int>(sizeof(nid.szTip) / sizeof(nid.szTip[0])));
}

bool add_tray_icon(NOTIFYICONDATAW& nid) {
    return Shell_NotifyIconW(NIM_ADD, &nid) != FALSE;
}

void remove_tray_icon(NOTIFYICONDATAW& nid) {
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

void show_open_settings_exit_menu(HWND hwnd, UINT open_settings_id, UINT exit_id) {
    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return;
    }

    AppendMenuW(menu, MF_STRING, open_settings_id, L"Open Settings");
    AppendMenuW(menu, MF_STRING, exit_id, L"Exit");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
    PostMessage(hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

}  // namespace notiman
