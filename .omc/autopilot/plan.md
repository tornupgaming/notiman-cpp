# Implementation Plan: System Tray Context Menu

**Target**: `C:\Users\terro\projects\notiman-cpp\src\host\main.cpp`

---

## Step 1: Add includes

**Action**: INSERT after line 6 (`#include <shellapi.h>`)
**Lines**: after line 6

```cpp
#include <shlobj.h>
#include <filesystem>
#include <fstream>
```

Result: lines 7-9 are the new includes; old line 7 (`#include "toast_manager.h"`) shifts to line 10.

---

## Step 2: Add menu command IDs

**Action**: INSERT after line 19 (`NOTIFYICONDATAW g_nid = {};`) — adjusted to line 22 after Step 1 shift
**Lines**: after original line 19 (post-shift line 22)

```cpp
constexpr UINT IDM_OPEN_SETTINGS = 1001;
constexpr UINT IDM_EXIT          = 1002;
```

---

## Step 3: Add ensure_config_path() function

**Action**: INSERT before WndProc — between the new constants (Step 2) and the `LRESULT CALLBACK WndProc` line (original line 21, post-shift line 26)
**Lines**: after the constants from Step 2, before WndProc

```cpp

std::filesystem::path ensure_config_path() {
    WCHAR appdata_buf[MAX_PATH];
    if (SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appdata_buf) == S_OK) {
        std::filesystem::path dir(appdata_buf);
        dir /= "notiman";
        std::filesystem::path config_path = dir / "config.toml";

        if (std::filesystem::exists(config_path)) {
            return config_path;
        }

        std::filesystem::create_directories(dir);

        std::ofstream out(config_path);
        if (out) {
            out << "port = 9123\n"
                << "corner = \"BottomRight\"\n"
                << "monitor = 0\n"
                << "max_visible = 5\n"
                << "duration = 4000\n"
                << "width = 400\n"
                << "accent_color = \"#7C3AED\"\n"
                << "opacity = 0.85\n";
        }

        return config_path;
    }

    return notiman::NotimanConfig::default_config_path();
}
```

---

## Step 4: Replace WM_RBUTTONUP handler

**Action**: REPLACE original lines 51-57 (the entire `case WM_APP + 1:` block including `return 0;`)
**Original code** (to find/match):

```cpp
        case WM_APP + 1:  // Tray icon message
            if (lParam == WM_RBUTTONUP) {
                // Right-click menu (will be added in next task)
                // For now, just show a test message
                MessageBoxW(hwnd, L"Tray icon clicked", L"Notiman", MB_OK);
            }
            return 0;
```

**Replace with**:

```cpp
        case WM_APP + 1:  // Tray icon message
            if (lParam == WM_RBUTTONUP) {
                HMENU hMenu = CreatePopupMenu();
                if (hMenu) {
                    AppendMenuW(hMenu, MF_STRING, IDM_OPEN_SETTINGS, L"Open Settings");
                    AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"Exit");

                    POINT pt;
                    GetCursorPos(&pt);
                    SetForegroundWindow(hwnd);
                    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
                    PostMessage(hwnd, WM_NULL, 0, 0);
                    DestroyMenu(hMenu);
                }
            }
            return 0;
```

---

## Step 5: Insert WM_COMMAND handler

**Action**: INSERT after the `return 0;` of the WM_APP+1 case (Step 4), before the `case WM_DESTROY:` line
**Lines**: between the WM_APP+1 return and WM_DESTROY case

```cpp

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDM_OPEN_SETTINGS: {
                    auto config_path = ensure_config_path();
                    ShellExecuteW(NULL, L"open",
                        config_path.wstring().c_str(),
                        NULL, NULL, SW_SHOWNORMAL);
                    break;
                }
                case IDM_EXIT:
                    DestroyWindow(hwnd);
                    break;
            }
            return 0;
```

---

## Execution Order

1. Step 1 — includes (top of file, no deps)
2. Step 2 — constants (after globals)
3. Step 3 — ensure_config_path function (uses constants implicitly, before WndProc)
4. Step 4 — replace WM_RBUTTONUP (uses constants)
5. Step 5 — insert WM_COMMAND (uses constants + ensure_config_path)

**Apply edits bottom-up (Steps 5→4→3→2→1) to preserve line numbers**, or use string-match replacement which is line-number independent.

---

## Expected Final Structure (main.cpp)

```
Lines 1-6:   Original includes
Lines 7-9:   New includes (shlobj, filesystem, fstream)
Lines 10-11: Original includes (toast_manager, payload)
Lines 13-15: pragma comments
Line 17:     using directive
Lines 19-22: Globals (g_d2dFactory, g_dwFactory, g_toastManager, g_nid)
Lines 24-25: Menu constants (IDM_OPEN_SETTINGS, IDM_EXIT)
Lines 27-57: ensure_config_path() function
Lines 59+:   WndProc with updated WM_APP+1 and new WM_COMMAND
```

## Verification

After applying, compile and confirm:
- Right-click tray icon shows "Open Settings" / "Exit"
- "Open Settings" creates+opens %APPDATA%/notiman/config.toml
- "Exit" cleanly shuts down (tray icon removed, COM cleaned)
- Existing WM_COPYDATA toast delivery unaffected
