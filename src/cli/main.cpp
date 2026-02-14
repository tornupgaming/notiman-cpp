#include <CLI11/CLI11.hpp>
#include <iostream>
#include <string>
#include <Windows.h>
#include <io.h>      // _isatty
#include "../shared/payload.h"
#include "../shared/icon.h"
#include "../shared/hook_parser.h"

// Convert UTF-8 string to UTF-16 wstring
static std::wstring utf8_to_utf16(const std::string& utf8) {
    if (utf8.empty()) return L"";

    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (len <= 0) return L"";

    std::wstring result(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &result[0], len);

    return result;
}

static std::string read_piped_stdin() {
    if (_isatty(_fileno(stdin))) {
        return "";
    }

    std::string stdin_content;
    std::string line;
    while (std::getline(std::cin, line)) {
        stdin_content += line + "\n";
    }

    while (!stdin_content.empty() &&
           (stdin_content.back() == '\n' || stdin_content.back() == '\r')) {
        stdin_content.pop_back();
    }

    return stdin_content;
}

static notiman::NotificationPayload build_manual_payload(
    const std::string& title,
    const std::string& body,
    const std::string& code,
    const std::string& icon_str,
    int duration
) {
    notiman::NotificationPayload payload;
    payload.title = utf8_to_utf16(title);

    if (!body.empty()) {
        payload.body = utf8_to_utf16(body);
    }

    if (!code.empty()) {
        payload.code = utf8_to_utf16(code);
    }

    payload.icon = notiman::icon_from_string(icon_str);

    if (duration > 0) {
        payload.duration = duration;
    }

    return payload;
}

static int send_payload_to_host(const notiman::NotificationPayload& payload) {
    HWND hostWindow = FindWindowW(L"NotimanHostClass", nullptr);
    if (!hostWindow) {
        std::cerr << "Error: notiman host is not running\n";
        return 1;
    }

    auto json_obj = payload.to_json();
    std::string json_str = json_obj.dump();

    COPYDATASTRUCT cds;
    cds.dwData = 1;
    cds.cbData = static_cast<DWORD>(json_str.size() + 1);
    cds.lpData = const_cast<char*>(json_str.c_str());

    LRESULT result = SendMessageW(
        hostWindow,
        WM_COPYDATA,
        0,
        reinterpret_cast<LPARAM>(&cds)
    );

    if (result == 1) {
        return 0;
    }

    std::cerr << "Error: host did not accept notification\n";
    return 1;
}

int main(int argc, char** argv) {
    CLI::App app{"Notiman CLI - send notifications"};

    std::string title;
    std::string body;
    std::string code;
    std::string icon_str = "info";
    int duration = 0;
    int port = 9123;

    app.add_option("-t,--title", title, "Notification title");
    app.add_option("-b,--body", body, "Notification body");
    app.add_option("-c,--code", code, "Code snippet (monospace block)");
    app.add_option("-i,--icon", icon_str, "Icon type (info/success/warning/error)")
        ->default_str("info");
    app.add_option("-d,--duration", duration, "Auto-dismiss duration in ms");
    app.add_option("-p,--port", port, "Host port")
        ->default_str("9123");

    CLI11_PARSE(app, argc, argv);

    notiman::NotificationPayload payload;
    bool payload_from_hook = false;
    std::string stdin_content = read_piped_stdin();

    if (!stdin_content.empty() && title.empty() && body.empty()) {
        auto hook_payload = notiman::HookParser::try_parse(stdin_content);
        if (hook_payload.has_value()) {
            payload = std::move(hook_payload.value());
            payload_from_hook = true;
        }
    }

    if (!payload_from_hook) {
        if (body.empty() && !stdin_content.empty()) {
            body = stdin_content;
        }

        if (title.empty()) {
            std::cerr << "Error: --title is required\n";
            return 1;
        }

        payload = build_manual_payload(title, body, code, icon_str, duration);
    }

    return send_payload_to_host(payload);
}
