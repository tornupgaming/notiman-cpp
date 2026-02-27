#pragma once

#include <windows.h>
#include <shellapi.h>

namespace notiman {

void init_tray_icon(
    NOTIFYICONDATAW& nid,
    HWND hwnd,
    UINT icon_id,
    UINT callback_message,
    const wchar_t* tooltip_text);

bool add_tray_icon(NOTIFYICONDATAW& nid);
void remove_tray_icon(NOTIFYICONDATAW& nid);

void show_open_settings_exit_menu(HWND hwnd, UINT open_settings_id, UINT exit_id);

}  // namespace notiman
