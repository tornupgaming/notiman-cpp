#include "hook_parser.h"
#include <windows.h>
#include <algorithm>
#include <filesystem>

namespace notiman {

namespace {
    std::wstring utf8_to_utf16(const std::string& utf8) {
        if (utf8.empty()) return L"";
        int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
        if (len <= 0) return L"";
        std::wstring result(len - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &result[0], len);
        return result;
    }

    bool starts_with_ignore_case(const std::string& str, const std::string& prefix) {
        if (str.length() < prefix.length()) return false;
        return std::equal(prefix.begin(), prefix.end(), str.begin(),
            [](char a, char b) { return std::tolower(a) == std::tolower(b); });
    }
}

std::optional<NotificationPayload> HookParser::try_parse(const std::string& json_str) {
    nlohmann::json root;
    try {
        root = nlohmann::json::parse(json_str);
    } catch (const nlohmann::json::exception&) {
        return std::nullopt;
    }

    if (!root.contains("hook_event_name") || !root["hook_event_name"].is_string()) {
        return std::nullopt;
    }

    std::string cwd = get_string(root, "cwd");
    std::string project = extract_project_name(cwd);

    std::string event_name = root["hook_event_name"].get<std::string>();
    std::optional<NotificationPayload> payload;

    if (event_name == "Notification") {
        payload = map_notification(root);
    } else if (event_name == "PostToolUse") {
        payload = map_post_tool_use(root, cwd);
    } else if (event_name == "PostToolUseFailure") {
        payload = map_post_tool_use_failure(root);
    } else if (event_name == "Stop") {
        payload = map_stop();
    } else if (event_name == "SubagentStop") {
        payload = map_subagent_stop(root);
    } else if (event_name == "SessionStart") {
        payload = map_session_start(root);
    }

    if (payload.has_value() && !project.empty()) {
        payload->project = utf8_to_utf16(project);
    }

    return payload;
}

std::optional<NotificationPayload> HookParser::map_notification(const nlohmann::json& root) {
    std::string notification_type = get_string(root, "notification_type");

    std::string title = "Notification";
    NotificationIcon icon = NotificationIcon::Info;

    if (notification_type == "permission_prompt") {
        title = "Permission Needed";
        icon = NotificationIcon::Warning;
    } else if (notification_type == "idle_prompt") {
        title = "Claude is Idle";
        icon = NotificationIcon::Info;
    } else if (notification_type == "auth_success") {
        title = "Auth Success";
        icon = NotificationIcon::Success;
    } else if (notification_type == "elicitation_dialog") {
        title = "Input Needed";
        icon = NotificationIcon::Info;
    }

    std::string message = get_string(root, "message");

    NotificationPayload payload;
    payload.title = utf8_to_utf16(title);
    payload.body = utf8_to_utf16(message);
    payload.icon = icon;

    return payload;
}

std::optional<NotificationPayload> HookParser::map_post_tool_use(const nlohmann::json& root, const std::string& cwd) {
    std::string tool_name = get_string(root, "tool_name");
    if (tool_name.empty()) {
        tool_name = "Unknown";
    }

    // Filter out Glob and Read
    if (tool_name == "Glob" || tool_name == "Read") {
        return std::nullopt;
    }

    std::string code = extract_tool_summary(root, tool_name, cwd);

    NotificationPayload payload;
    payload.title = utf8_to_utf16("Tool Complete: " + tool_name);
    payload.code = utf8_to_utf16(code);
    payload.icon = NotificationIcon::Success;

    return payload;
}

std::optional<NotificationPayload> HookParser::map_post_tool_use_failure(const nlohmann::json& root) {
    std::string tool_name = get_string(root, "tool_name");
    if (tool_name.empty()) {
        tool_name = "Unknown";
    }

    std::string error = get_string(root, "error");

    NotificationPayload payload;
    payload.title = utf8_to_utf16("Tool Failed: " + tool_name);
    payload.code = utf8_to_utf16(error);
    payload.icon = NotificationIcon::Error;

    return payload;
}

std::optional<NotificationPayload> HookParser::map_stop() {
    NotificationPayload payload;
    payload.title = L"Claude Finished";
    payload.icon = NotificationIcon::Success;
    return payload;
}

std::optional<NotificationPayload> HookParser::map_subagent_stop(const nlohmann::json& root) {
    std::string agent_type = get_string(root, "agent_type");

    std::string title;
    if (!agent_type.empty()) {
        title = "Agent Done: " + agent_type;
    } else {
        title = "Agent Done";
    }

    NotificationPayload payload;
    payload.title = utf8_to_utf16(title);
    payload.icon = NotificationIcon::Success;

    return payload;
}

std::optional<NotificationPayload> HookParser::map_session_start(const nlohmann::json& root) {
    std::string source = get_string(root, "source");

    NotificationPayload payload;
    payload.title = L"Session Resumed";
    payload.body = utf8_to_utf16(source);
    payload.icon = NotificationIcon::Info;

    return payload;
}

std::string HookParser::extract_tool_summary(const nlohmann::json& root, const std::string& tool_name, const std::string& cwd) {
    if (!root.contains("tool_input") || !root["tool_input"].is_object()) {
        return "";
    }

    const auto& tool_input = root["tool_input"];

    if (tool_name == "Bash") {
        return get_string(tool_input, "command");
    } else if (tool_name == "Write" || tool_name == "Edit" || tool_name == "Read") {
        std::string file_path = get_string(tool_input, "file_path");
        return strip_cwd(file_path, cwd);
    }

    return "";
}

std::string HookParser::get_string(const nlohmann::json& element, const std::string& property) {
    if (element.contains(property) && element[property].is_string()) {
        return element[property].get<std::string>();
    }
    return "";
}

std::string HookParser::extract_project_name(const std::string& cwd) {
    if (cwd.empty()) return "";

    std::string trimmed = cwd;
    // Trim trailing separators
    while (!trimmed.empty() && (trimmed.back() == '/' || trimmed.back() == '\\')) {
        trimmed.pop_back();
    }

    if (trimmed.empty()) return "";

    // Find last separator
    size_t pos = trimmed.find_last_of("/\\");
    if (pos == std::string::npos) {
        return trimmed;
    }

    return trimmed.substr(pos + 1);
}

std::string HookParser::strip_cwd(const std::string& file_path, const std::string& cwd) {
    if (file_path.empty() || cwd.empty()) return file_path;

    // Normalize cwd with trailing separator
    std::string normalized_cwd = cwd;
    while (!normalized_cwd.empty() && (normalized_cwd.back() == '/' || normalized_cwd.back() == '\\')) {
        normalized_cwd.pop_back();
    }

    // Try with backslash separator
    std::string cwd_with_sep = normalized_cwd + "\\";
    if (starts_with_ignore_case(file_path, cwd_with_sep)) {
        return file_path.substr(cwd_with_sep.length());
    }

    // Try with forward slash separator
    cwd_with_sep = normalized_cwd + "/";
    if (starts_with_ignore_case(file_path, cwd_with_sep)) {
        return file_path.substr(cwd_with_sep.length());
    }

    return file_path;
}

} // namespace notiman
