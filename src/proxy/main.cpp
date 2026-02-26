#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

#include <atomic>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include <httplib/httplib.h>

#include "../shared/payload.h"
#include "proxy_config.h"

#pragma comment(lib, "shell32.lib")

namespace {

constexpr UINT IDM_OPEN_SETTINGS = 1001;
constexpr UINT IDM_EXIT = 1002;
constexpr UINT WM_TRAYICON = WM_APP + 1;

NOTIFYICONDATAW g_nid = {};
HWND g_hwnd = nullptr;
std::unique_ptr<httplib::Server> g_server;
std::thread g_server_thread;
std::atomic_bool g_server_running = false;
notiman::ProxyConfig g_proxy_config;

std::wstring utf8_to_utf16(const std::string& utf8) {
    if (utf8.empty()) {
        return L"";
    }

    const int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (len <= 0) {
        return L"";
    }

    std::wstring result(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &result[0], len);
    return result;
}

std::filesystem::path ensure_proxy_config_path() {
    WCHAR appdata_buf[MAX_PATH];
    if (SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appdata_buf) == S_OK) {
        std::filesystem::path dir(appdata_buf);
        dir /= "notiman";
        std::filesystem::path config_path = dir / "proxy.ini";

        if (std::filesystem::exists(config_path)) {
            return config_path;
        }

        std::filesystem::create_directories(dir);
        std::ofstream out(config_path);
        if (out) {
            out << "[proxy]\n";
            out << "host=127.0.0.1\n";
            out << "port=8080\n\n";
            out << "[routes]\n";
            out << "; /api = http://localhost:3000/api\n";
        }
        return config_path;
    }

    return notiman::ProxyConfig::default_config_path();
}

int send_payload_to_host(const notiman::NotificationPayload& payload) {
    HWND host_window = FindWindowW(L"NotimanHostClass", nullptr);
    if (!host_window) {
        return 1;
    }

    const std::string json_str = payload.to_json().dump();
    COPYDATASTRUCT cds = {};
    cds.dwData = 1;
    cds.cbData = static_cast<DWORD>(json_str.size() + 1);
    cds.lpData = const_cast<char*>(json_str.c_str());

    const LRESULT result = SendMessageW(host_window, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds));
    return (result == 1) ? 0 : 1;
}

void notify_host(notiman::NotificationIcon icon,
                 const std::wstring& title,
                 const std::wstring& body = L"",
                 const std::wstring& code = L"",
                 const std::wstring& project = L"") {
    notiman::NotificationPayload payload;
    payload.icon = icon;
    payload.title = title;
    payload.body = body;
    payload.code = code;
    payload.project = project;
    payload.duration = 10000;
    send_payload_to_host(payload);
}

struct TargetEndpoint {
    std::string scheme;
    std::string host;
    int port = 80;
    std::string base_path = "/";
};

std::optional<TargetEndpoint> parse_target_endpoint(const std::string& url) {
    const size_t scheme_pos = url.find("://");
    if (scheme_pos == std::string::npos) {
        return std::nullopt;
    }

    TargetEndpoint endpoint;
    endpoint.scheme = url.substr(0, scheme_pos);
    if (endpoint.scheme != "http") {
        return std::nullopt;
    }

    const size_t authority_start = scheme_pos + 3;
    const size_t path_start = url.find('/', authority_start);
    const std::string authority = (path_start == std::string::npos)
        ? url.substr(authority_start)
        : url.substr(authority_start, path_start - authority_start);
    endpoint.base_path = (path_start == std::string::npos) ? "/" : url.substr(path_start);

    if (authority.empty()) {
        return std::nullopt;
    }

    const size_t colon_pos = authority.rfind(':');
    if (colon_pos != std::string::npos) {
        endpoint.host = authority.substr(0, colon_pos);
        const std::string port_text = authority.substr(colon_pos + 1);
        if (endpoint.host.empty() || port_text.empty()) {
            return std::nullopt;
        }
        try {
            endpoint.port = std::stoi(port_text);
        } catch (...) {
            return std::nullopt;
        }
    } else {
        endpoint.host = authority;
        endpoint.port = 80;
    }

    if (endpoint.base_path.empty()) {
        endpoint.base_path = "/";
    }
    return endpoint;
}

bool path_matches_prefix(const std::string& path, const std::string& prefix) {
    if (prefix.empty() || prefix[0] != '/') {
        return false;
    }
    if (path.rfind(prefix, 0) != 0) {
        return false;
    }
    if (path.size() == prefix.size()) {
        return true;
    }
    if (!prefix.empty() && prefix.back() == '/') {
        return true;
    }
    return path[prefix.size()] == '/';
}

std::optional<notiman::ProxyRoute> find_route_for_path(const std::string& path) {
    for (const auto& route : g_proxy_config.routes) {
        if (path_matches_prefix(path, route.path_prefix)) {
            return route;
        }
    }
    return std::nullopt;
}

std::string build_forward_path(const std::string& request_path,
                               const std::string& request_query,
                               const std::string& route_prefix,
                               const std::string& target_base_path) {
    std::string remainder;
    if (request_path.size() > route_prefix.size()) {
        remainder = request_path.substr(route_prefix.size());
    }

    std::string joined = target_base_path;
    if (joined.empty()) {
        joined = "/";
    }
    if (joined.back() == '/' && !remainder.empty() && remainder.front() == '/') {
        joined.pop_back();
    } else if (joined.back() != '/' && !remainder.empty() && remainder.front() != '/') {
        joined.push_back('/');
    }
    joined += remainder;

    if (!request_query.empty()) {
        joined += "?";
        joined += request_query;
    }
    return joined;
}

std::string extract_query_from_target(const std::string& target) {
    const size_t qpos = target.find('?');
    if (qpos == std::string::npos || qpos + 1 >= target.size()) {
        return {};
    }
    return target.substr(qpos + 1);
}

std::string lowercase(const std::string& input) {
    std::string result = input;
    for (char& c : result) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return result;
}

std::wstring build_request_title(const httplib::Request& req, long long elapsed_ms) {
    return utf8_to_utf16(req.method + " " + std::to_string(elapsed_ms) + "ms");
}

void proxy_request(const httplib::Request& req, httplib::Response& res) {
    const auto started_at = std::chrono::steady_clock::now();

    auto route = find_route_for_path(req.path);
    if (!route.has_value()) {
        res.status = 500;
        res.set_content("No route configured for path", "text/plain");
        notify_host(
            notiman::NotificationIcon::Error,
            L"Proxy error",
            utf8_to_utf16("No route configured for " + req.path),
            L"route-match",
            L"");
        return;
    }

    auto endpoint = parse_target_endpoint(route->target_base_url);
    if (!endpoint.has_value()) {
        res.status = 500;
        res.set_content("Invalid route target URL", "text/plain");
        notify_host(
            notiman::NotificationIcon::Error,
            L"Proxy error",
            utf8_to_utf16("Invalid target URL for route " + route->path_prefix),
            L"target-url",
            utf8_to_utf16(route->path_prefix));
        return;
    }

    httplib::Client client(endpoint->host, endpoint->port);
    client.set_connection_timeout(3, 0);
    client.set_read_timeout(15, 0);
    client.set_write_timeout(15, 0);

    httplib::Headers headers;
    const std::unordered_set<std::string> excluded_headers = {
        "host", "content-length", "transfer-encoding", "connection"
    };
    for (const auto& [key, value] : req.headers) {
        if (excluded_headers.find(lowercase(key)) != excluded_headers.end()) {
            continue;
        }
        headers.emplace(key, value);
    }

    httplib::Request outgoing;
    outgoing.method = req.method;
    outgoing.path = build_forward_path(
        req.path,
        extract_query_from_target(req.target),
        route->path_prefix,
        endpoint->base_path);
    outgoing.headers = std::move(headers);
    outgoing.body = req.body;

    auto result = client.send(outgoing);
    const auto ended_at = std::chrono::steady_clock::now();
    const auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(ended_at - started_at).count();

    if (!result) {
        res.status = 502;
        res.set_content("Failed to reach upstream target", "text/plain");
        notify_host(
            notiman::NotificationIcon::Error,
            L"Proxy error",
            utf8_to_utf16("Failed to connect to " + route->target_base_url),
            L"upstream",
            utf8_to_utf16(route->path_prefix));
        return;
    }

    res.status = result->status;
    res.body = result->body;
    for (const auto& [key, value] : result->headers) {
        if (excluded_headers.find(lowercase(key)) != excluded_headers.end()) {
            continue;
        }
        res.set_header(key.c_str(), value.c_str());
    }

    notiman::NotificationIcon icon = notiman::NotificationIcon::Info;
    if (result->status >= 500) {
        icon = notiman::NotificationIcon::Error;
    } else if (result->status >= 400) {
        icon = notiman::NotificationIcon::Warning;
    }

    const std::string remainder = req.path.size() > route->path_prefix.size()
        ? req.path.substr(route->path_prefix.size())
        : "/";

    notify_host(
        icon,
        build_request_title(req, elapsed_ms),
        L"",
        utf8_to_utf16(remainder),
        utf8_to_utf16(route->path_prefix));
}

bool start_proxy_server() {
    g_server = std::make_unique<httplib::Server>();
    if (!g_server) {
        return false;
    }

    auto handler = [](const httplib::Request& req, httplib::Response& res) {
        try {
            proxy_request(req, res);
        } catch (...) {
            res.status = 500;
            res.set_content("Internal proxy error", "text/plain");
            notify_host(notiman::NotificationIcon::Error, L"Proxy error", L"Unhandled exception.", L"internal");
        }
    };

    g_server->Get(R"(/.*)", handler);
    g_server->Post(R"(/.*)", handler);
    g_server->Put(R"(/.*)", handler);
    g_server->Delete(R"(/.*)", handler);
    g_server->Patch(R"(/.*)", handler);
    g_server->Options(R"(/.*)", handler);

    g_server_thread = std::thread([] {
        g_server_running = true;
        g_server->listen(g_proxy_config.host, g_proxy_config.port);
        g_server_running = false;
    });

    // Quick startup check. If binding fails immediately, the thread exits.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    return g_server_running.load();
}

void stop_proxy_server() {
    if (g_server) {
        g_server->stop();
    }
    if (g_server_thread.joinable()) {
        g_server_thread.join();
    }
    g_server.reset();
    g_server_running = false;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            HMENU menu = CreatePopupMenu();
            if (menu) {
                AppendMenuW(menu, MF_STRING, IDM_OPEN_SETTINGS, L"Open Settings");
                AppendMenuW(menu, MF_STRING, IDM_EXIT, L"Exit");

                POINT pt;
                GetCursorPos(&pt);
                SetForegroundWindow(hwnd);
                TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
                PostMessage(hwnd, WM_NULL, 0, 0);
                DestroyMenu(menu);
            }
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDM_OPEN_SETTINGS: {
            auto config_path = ensure_proxy_config_path();
            ShellExecuteW(NULL, L"open", config_path.wstring().c_str(), NULL, NULL, SW_SHOWNORMAL);
            break;
        }
        case IDM_EXIT:
            DestroyWindow(hwnd);
            break;
        default:
            break;
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

}  // namespace

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    HANDLE mutex = CreateMutexW(nullptr, FALSE, L"Global\\NotimanProxyMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, L"Notiman Proxy is already running.", L"Notiman Proxy", MB_OK | MB_ICONINFORMATION);
        return 1;
    }

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"NotimanProxyClass";

    if (!RegisterClassExW(&wc)) {
        if (mutex) {
            CloseHandle(mutex);
        }
        return 1;
    }

    g_hwnd = CreateWindowExW(
        0,
        L"NotimanProxyClass",
        L"Notiman Proxy",
        0,
        0,
        0,
        0,
        0,
        HWND_MESSAGE,
        nullptr,
        hInstance,
        nullptr);
    if (!g_hwnd) {
        if (mutex) {
            CloseHandle(mutex);
        }
        return 1;
    }

    const auto config_path = ensure_proxy_config_path();
    g_proxy_config = notiman::ProxyConfig::load_from_file(config_path);

    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = g_hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    swprintf_s(g_nid.szTip, L"Notiman Proxy");
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    if (!start_proxy_server()) {
        notify_host(
            notiman::NotificationIcon::Error,
            L"notiman-proxy startup error",
            utf8_to_utf16("Failed to bind " + g_proxy_config.host + ":" + std::to_string(g_proxy_config.port)));
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        if (mutex) {
            CloseHandle(mutex);
        }
        return 1;
    }

    notify_host(
        notiman::NotificationIcon::Info,
        L"notiman-proxy started",
        utf8_to_utf16("Listening on " + g_proxy_config.host + ":" + std::to_string(g_proxy_config.port)));

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    stop_proxy_server();
    Shell_NotifyIconW(NIM_DELETE, &g_nid);

    if (mutex) {
        CloseHandle(mutex);
    }
    return static_cast<int>(msg.wParam);
}
