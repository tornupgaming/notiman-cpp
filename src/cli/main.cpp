#include <CLI11/CLI11.hpp>
#include <cstdlib>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <vector>
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

static std::string get_log_file_path() {
    char* env_path = nullptr;
    size_t env_len = 0;
    if (_dupenv_s(&env_path, &env_len, "NOTIMAN_LOG_FILE") == 0 &&
        env_path != nullptr &&
        env_path[0] != '\0') {
        std::string value(env_path);
        free(env_path);
        return value;
    }

    if (env_path != nullptr) {
        free(env_path);
    }

    // Default to current working directory. For user-level Cursor hooks this is
    // typically C:\Users\<user>\.cursor\logs.txt.
    return "logs.txt";
}

static std::string build_timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto now_time = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm {};
    localtime_s(&local_tm, &now_time);

    std::ostringstream oss;
    oss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

static void append_log(const std::string& message) {
    std::ofstream log_file(get_log_file_path(), std::ios::app);
    if (!log_file.is_open()) {
        return;
    }

    log_file << "[" << build_timestamp() << "] " << message << "\n";
}

static std::string extract_hook_event_name(const std::string& stdin_content) {
    try {
        auto root = nlohmann::json::parse(stdin_content);
        if (root.contains("hook_event_name") && root["hook_event_name"].is_string()) {
            return root["hook_event_name"].get<std::string>();
        }
    } catch (const nlohmann::json::exception&) {
    }

    return "";
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
        append_log("Host window not found (NotimanHostClass).");
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
        append_log("Notification payload accepted by host.");
        return 0;
    }

    append_log("Host rejected notification payload.");
    std::cerr << "Error: host did not accept notification\n";
    return 1;
}

int main(int argc, char** argv) {
    CLI::App app{"Notiman CLI - send notifications"};

    std::string title;
    std::string body;
    std::string code;
    std::string icon_str = "info";
    std::vector<std::string> ignored_tools = {"Glob", "Grep", "Read", "ReadFile"};
    int duration = 0;
    int port = 9123;

    app.add_option("-t,--title", title, "Notification title");
    app.add_option("-b,--body", body, "Notification body");
    app.add_option("-c,--code", code, "Code snippet (monospace block)");
    app.add_option("-i,--icon", icon_str, "Icon type (info/success/warning/error)")
        ->default_str("info");
    app.add_option("-d,--duration", duration, "Auto-dismiss duration in ms");
    app.add_option("--ignore-tool", ignored_tools, "Tool name to ignore for hook-based notifications (repeatable or comma-separated)")
        ->delimiter(',')
        ->default_str("Glob,Grep,Read,ReadFile");
    app.add_option("-p,--port", port, "Host port")
        ->default_str("9123");

    CLI11_PARSE(app, argc, argv);
    append_log("CLI invoked.");

    notiman::NotificationPayload payload;
    bool payload_from_hook = false;
    std::string stdin_content = read_piped_stdin();
    append_log("stdin bytes=" + std::to_string(stdin_content.size()));

    if (!stdin_content.empty() && title.empty() && body.empty()) {
        std::string hook_event_name = extract_hook_event_name(stdin_content);
        if (!hook_event_name.empty()) {
            append_log("Hook payload detected: hook_event_name=" + hook_event_name);
        } else {
            append_log("stdin present but missing/invalid hook_event_name.");
        }

        auto hook_payload = notiman::HookParser::try_parse(stdin_content, ignored_tools);
        if (hook_payload.has_value()) {
            payload = std::move(hook_payload.value());
            payload_from_hook = true;
            append_log("Hook payload mapped to notification.");
        } else {
            // In hook mode, ignore events that do not map to a notification.
            append_log("Hook payload ignored (unmapped/filtered/invalid).");
            return 0;
        }
    }

    if (!payload_from_hook) {
        if (body.empty() && !stdin_content.empty()) {
            body = stdin_content;
        }

        if (title.empty()) {
            append_log("Manual mode failed: --title missing.");
            std::cerr << "Error: --title is required\n";
            return 1;
        }

        append_log("Manual payload built from CLI arguments.");
        payload = build_manual_payload(title, body, code, icon_str, duration);
    }

    append_log("Sending payload to host.");
    return send_payload_to_host(payload);
}
