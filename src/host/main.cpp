#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <nlohmann/json.hpp>
#include <shellapi.h>
#include <shlobj.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include "toast_manager.h"
#include "../shared/config_watcher.h"
#include "../shared/payload.h"
#include "../shared/tray_icon.h"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "shell32.lib")

using Microsoft::WRL::ComPtr;

ComPtr<ID2D1Factory> g_d2dFactory;
ComPtr<IDWriteFactory> g_dwFactory;
std::unique_ptr<notiman::ToastManager> g_toastManager;
NOTIFYICONDATAW g_nid = {};
constexpr UINT IDM_OPEN_SETTINGS = 1001;
constexpr UINT IDM_EXIT = 1002;
constexpr UINT WM_CONFIG_CHANGED = WM_APP + 2;

std::filesystem::path g_config_path;
std::thread g_watcher_thread;
HANDLE g_watcher_dir_handle = INVALID_HANDLE_VALUE;

std::filesystem::path ensure_config_path()
{
    WCHAR appdata_buf[MAX_PATH];
    if (SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appdata_buf) == S_OK)
    {
        std::filesystem::path dir(appdata_buf);
        dir /= "notiman";
        std::filesystem::path config_path = dir / "config.ini";

        if (std::filesystem::exists(config_path))
        {
            return config_path;
        }

        std::filesystem::create_directories(dir);

        std::ofstream out(config_path);
        if (out)
        {
            out << "[notiman]\n"
                << "corner=BottomRight\n"
                << "monitor=0\n"
                << "max_visible=5\n"
                << "duration=4000\n"
                << "width=400\n"
                << "accent_color=#7C3AED\n"
                << "opacity=0.85\n";
        }

        return config_path;
    }

    return notiman::NotimanConfig::default_config_path();
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_COPYDATA:
    {
        auto *cds = reinterpret_cast<COPYDATASTRUCT *>(lParam);
        if (cds && cds->dwData == 1)
        { // Check identifier
            // Validate cbData to prevent integer underflow
            if (cds->cbData == 0)
            {
                return 0;
            }

            try
            {
                // Parse JSON from lpData
                std::string json_str(
                    static_cast<const char *>(cds->lpData),
                    cds->cbData - 1 // Exclude null terminator
                );

                // Deserialize payload
                auto j = nlohmann::json::parse(json_str);
                auto payload = notiman::NotificationPayload::from_json(j);

                // Show toast
                if (g_toastManager)
                {
                    g_toastManager->Show(std::move(payload));
                }

                return 1; // Success
            }
            catch (...)
            {
                return 0; // Parse error
            }
        }
        return 0;
    }

    case WM_APP + 1: // Tray icon message
        if (lParam == WM_RBUTTONUP)
        {
            notiman::show_open_settings_exit_menu(hwnd, IDM_OPEN_SETTINGS, IDM_EXIT);
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDM_OPEN_SETTINGS:
        {
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

    case WM_CONFIG_CHANGED:
    {
        auto new_config = notiman::NotimanConfig::load_from_file(g_config_path);
        g_toastManager = std::make_unique<notiman::ToastManager>(
            new_config, g_d2dFactory.Get(), g_dwFactory.Get());
        notiman::NotificationPayload payload;
        payload.title = L"Config reloaded";
        payload.icon = notiman::NotificationIcon::Info;
        g_toastManager->Show(std::move(payload));
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    // Enable per-monitor DPI awareness V2 (Windows 10 1703+)
    // Prevents bitmap scaling blur at non-100% display scales
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Single instance check
    HANDLE mutex = CreateMutexW(nullptr, FALSE, L"Global\\NotimanHostMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        MessageBoxW(nullptr,
                    L"Notiman Host is already running.",
                    L"Notiman",
                    MB_OK | MB_ICONINFORMATION);
        return 1;
    }

    // Initialize COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
    {
        return 1;
    }

    // Create D2D factory
    hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        g_d2dFactory.GetAddressOf());
    if (FAILED(hr))
    {
        CoUninitialize();
        return 1;
    }

    // Create DirectWrite factory
    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown **>(g_dwFactory.GetAddressOf()));
    if (FAILED(hr))
    {
        CoUninitialize();
        return 1;
    }

    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"NotimanHostClass"; // For WM_COPYDATA discovery

    if (!RegisterClassExW(&wc))
    {
        return 1;
    }

    // Create hidden message-only window
    HWND hwnd = CreateWindowExW(
        0,
        L"NotimanHostClass",
        L"Notiman Host",
        0, // No WS_VISIBLE
        0, 0, 0, 0,
        HWND_MESSAGE, // Message-only window (invisible)
        nullptr,
        hInstance,
        nullptr);

    if (!hwnd)
    {
        return 1;
    }

    // Load config
    g_config_path = ensure_config_path();
    auto config = notiman::NotimanConfig::load_from_file(g_config_path);

    // Start config file watcher
    g_watcher_dir_handle = CreateFileW(
        g_config_path.parent_path().wstring().c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (g_watcher_dir_handle != INVALID_HANDLE_VALUE)
    {
        g_watcher_thread = std::thread(
            notiman::run_config_watcher,
            g_watcher_dir_handle,
            hwnd,
            WM_CONFIG_CHANGED,
            g_config_path.filename().wstring());
    }

    // Create toast manager
    g_toastManager = std::make_unique<notiman::ToastManager>(
        config,
        g_d2dFactory.Get(),
        g_dwFactory.Get());

    // Create tray icon
    notiman::init_tray_icon(g_nid, hwnd, 1, WM_APP + 1, L"Notiman");
    notiman::add_tray_icon(g_nid);

    // Test notification (temporary)
    notiman::NotificationPayload test_payload;
    test_payload.title = L"Test Toast";
    test_payload.body = L"Notiman Host is running";
    test_payload.icon = notiman::NotificationIcon::Info;
    g_toastManager->Show(test_payload);

    // Message pump
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Stop config watcher
    if (g_watcher_dir_handle != INVALID_HANDLE_VALUE)
    {
        if (g_watcher_thread.joinable())
        {
            CancelSynchronousIo(reinterpret_cast<HANDLE>(g_watcher_thread.native_handle()));
        }
        CloseHandle(g_watcher_dir_handle);
        g_watcher_dir_handle = INVALID_HANDLE_VALUE;
    }
    if (g_watcher_thread.joinable())
    {
        g_watcher_thread.join();
    }

    // Remove tray icon
    notiman::remove_tray_icon(g_nid);

    // Cleanup COM
    g_toastManager.reset();
    g_d2dFactory.Reset();
    g_dwFactory.Reset();
    CoUninitialize();

    if (mutex)
    {
        CloseHandle(mutex);
    }

    return static_cast<int>(msg.wParam);
}
