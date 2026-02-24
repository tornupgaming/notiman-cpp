#include "hook_parser.h"
#include <windows.h>
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace notiman
{

    namespace
    {
        std::wstring utf8_to_utf16(const std::string &utf8)
        {
            if (utf8.empty())
                return L"";
            int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
            if (len <= 0)
                return L"";
            std::wstring result(len - 1, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &result[0], len);
            return result;
        }

        bool starts_with_ignore_case(const std::string &str, const std::string &prefix)
        {
            if (str.length() < prefix.length())
                return false;
            return std::equal(prefix.begin(), prefix.end(), str.begin(),
                              [](char a, char b)
                              { return std::tolower(a) == std::tolower(b); });
        }

        bool equals_ignore_case(const std::string &a, const std::string &b)
        {
            if (a.size() != b.size())
                return false;
            return std::equal(a.begin(), a.end(), b.begin(),
                              [](char c1, char c2)
                              { return std::tolower(c1) == std::tolower(c2); });
        }

        bool is_tool_ignored(const std::string &tool_name, const std::vector<std::string> &ignored_tools)
        {
            if (tool_name.empty() || ignored_tools.empty())
                return false;

            return std::any_of(ignored_tools.begin(), ignored_tools.end(),
                               [&tool_name](const std::string &ignored)
                               { return equals_ignore_case(tool_name, ignored); });
        }

        bool is_lower_camel_case_event(const std::string &event_name)
        {
            return !event_name.empty() &&
                   std::islower(static_cast<unsigned char>(event_name.front())) != 0;
        }
    }

    std::optional<NotificationPayload> HookParser::try_parse(
        const std::string &json_str,
        const std::vector<std::string> &ignored_tools)
    {
        nlohmann::json root;
        try
        {
            root = nlohmann::json::parse(json_str);
        }
        catch (const nlohmann::json::exception &)
        {
            return std::nullopt;
        }

        if (!root.contains("hook_event_name") || !root["hook_event_name"].is_string())
        {
            return std::nullopt;
        }

        std::string cwd = get_string(root, "cwd");
        std::string project = extract_project_name(cwd);

        std::string event_name = root["hook_event_name"].get<std::string>();
        std::string normalized_event_name = normalize_event_name(event_name);
        std::optional<NotificationPayload> payload;

        if (normalized_event_name == "notification")
        {
            payload = map_notification(root);
        }
        else if (normalized_event_name == "posttooluse")
        {
            payload = map_post_tool_use(root, cwd, ignored_tools);
        }
        else if (normalized_event_name == "posttoolusefailure")
        {
            payload = map_post_tool_use_failure(root, ignored_tools);
        }
        else if (normalized_event_name == "aftershellexecution")
        {
            payload = map_after_shell_execution(root);
        }
        else if (normalized_event_name == "aftermcpexecution")
        {
            payload = map_after_mcp_execution(root, ignored_tools);
        }
        else if (normalized_event_name == "afterfileedit")
        {
            payload = map_after_file_edit(root, cwd);
        }
        else if (normalized_event_name == "stop")
        {
            payload = map_stop(root);
        }
        else if (normalized_event_name == "subagentstop")
        {
            payload = map_subagent_stop(root);
        }
        else if (normalized_event_name == "sessionstart")
        {
            payload = map_session_start(root);
        }

        if (payload.has_value() && !project.empty())
        {
            payload->project = utf8_to_utf16(project);
        }

        return payload;
    }

    std::string HookParser::normalize_event_name(const std::string &event_name)
    {
        std::string normalized;
        normalized.reserve(event_name.size());

        for (char c : event_name)
        {
            if (std::isalnum(static_cast<unsigned char>(c)))
            {
                normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            }
        }

        return normalized;
    }

    std::optional<NotificationPayload> HookParser::map_notification(const nlohmann::json &root)
    {
        std::string notification_type = get_string(root, "notification_type");

        std::string title = "Notification";
        NotificationIcon icon = NotificationIcon::Info;

        if (notification_type == "permission_prompt")
        {
            title = "Permission Needed";
            icon = NotificationIcon::Warning;
        }
        else if (notification_type == "idle_prompt")
        {
            title = "Claude is Idle";
            icon = NotificationIcon::Info;
        }
        else if (notification_type == "auth_success")
        {
            title = "Auth Success";
            icon = NotificationIcon::Success;
        }
        else if (notification_type == "elicitation_dialog")
        {
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

    std::optional<NotificationPayload> HookParser::map_post_tool_use(
        const nlohmann::json &root,
        const std::string &cwd,
        const std::vector<std::string> &ignored_tools)
    {
        std::string tool_name = get_first_string(root, {"tool_name", "tool"});
        if (is_tool_ignored(tool_name, ignored_tools))
        {
            return std::nullopt;
        }
        if (tool_name.empty())
        {
            tool_name = "Unknown";
        }

        std::string code = extract_tool_summary(root, tool_name, cwd);

        NotificationPayload payload;
        payload.title = utf8_to_utf16("Tool Complete: " + tool_name);
        payload.code = utf8_to_utf16(code);
        payload.icon = NotificationIcon::Success;

        return payload;
    }

    std::optional<NotificationPayload> HookParser::map_post_tool_use_failure(
        const nlohmann::json &root,
        const std::vector<std::string> &ignored_tools)
    {
        std::string tool_name = get_first_string(root, {"tool_name", "tool"});
        if (is_tool_ignored(tool_name, ignored_tools))
        {
            return std::nullopt;
        }
        if (tool_name.empty())
        {
            tool_name = "Unknown";
        }

        std::string error = get_first_string(root, {"error", "error_message"});

        NotificationPayload payload;
        payload.title = utf8_to_utf16("Tool Failed: " + tool_name);
        payload.code = utf8_to_utf16(error);
        payload.icon = NotificationIcon::Error;

        return payload;
    }

    std::optional<NotificationPayload> HookParser::map_after_shell_execution(const nlohmann::json &root)
    {
        std::string command = get_string(root, "command");
        if (command.empty())
        {
            return std::nullopt;
        }

        NotificationPayload payload;
        payload.title = L"Shell Complete";
        payload.code = utf8_to_utf16(command);
        payload.icon = NotificationIcon::Success;
        return payload;
    }

    std::optional<NotificationPayload> HookParser::map_after_mcp_execution(
        const nlohmann::json &root,
        const std::vector<std::string> &ignored_tools)
    {
        std::string tool_name = get_string(root, "tool_name");
        if (is_tool_ignored(tool_name, ignored_tools))
        {
            return std::nullopt;
        }

        if (tool_name.empty())
        {
            tool_name = "Unknown";
        }

        std::string summary = get_string(root, "tool_input");
        if (summary.empty() && root.contains("tool_input"))
        {
            const auto &tool_input = root["tool_input"];
            if (tool_input.is_object())
            {
                summary = get_first_string(tool_input, {"command", "file_path", "path"});
            }
        }

        NotificationPayload payload;
        payload.title = utf8_to_utf16("MCP Complete: " + tool_name);
        payload.code = utf8_to_utf16(summary);
        payload.icon = NotificationIcon::Success;
        return payload;
    }

    std::optional<NotificationPayload> HookParser::map_after_file_edit(
        const nlohmann::json &root,
        const std::string &cwd)
    {
        std::string file_path = get_string(root, "file_path");
        if (file_path.empty())
        {
            return std::nullopt;
        }

        NotificationPayload payload;
        payload.title = L"File Edited";
        payload.code = utf8_to_utf16(strip_cwd(file_path, cwd));
        payload.icon = NotificationIcon::Info;
        return payload;
    }

    std::optional<NotificationPayload> HookParser::map_stop(const nlohmann::json &root)
    {
        std::string event_name = get_string(root, "hook_event_name");
        bool cursor_style_name = is_lower_camel_case_event(event_name);

        NotificationPayload payload;
        payload.title = cursor_style_name ? L"Cursor Finished" : L"Claude Finished";
        payload.icon = NotificationIcon::Success;
        return payload;
    }

    std::optional<NotificationPayload> HookParser::map_subagent_stop(const nlohmann::json &root)
    {
        std::string agent_type = get_first_string(root, {"agent_type", "subagent_type"});

        std::string title;
        if (!agent_type.empty())
        {
            title = "Agent Done: " + agent_type;
        }
        else
        {
            title = "Agent Done";
        }

        NotificationPayload payload;
        payload.title = utf8_to_utf16(title);
        payload.icon = NotificationIcon::Success;

        return payload;
    }

    std::optional<NotificationPayload> HookParser::map_session_start(const nlohmann::json &root)
    {
        std::string source = get_first_string(root, {"source", "composer_mode"});

        NotificationPayload payload;
        payload.title = source.empty() ? L"Session Started" : L"Session Resumed";
        payload.body = utf8_to_utf16(source);
        payload.icon = NotificationIcon::Info;

        return payload;
    }

    std::string HookParser::extract_tool_summary(const nlohmann::json &root, const std::string &tool_name, const std::string &cwd)
    {
        if (!root.contains("tool_input") || !root["tool_input"].is_object())
        {
            return "";
        }

        const auto &tool_input = root["tool_input"];

        if (equals_ignore_case(tool_name, "Bash") || equals_ignore_case(tool_name, "Shell"))
        {
            return get_string(tool_input, "command");
        }
        else if (equals_ignore_case(tool_name, "Write") ||
                 equals_ignore_case(tool_name, "Edit") ||
                 equals_ignore_case(tool_name, "Read") ||
                 equals_ignore_case(tool_name, "ReadFile") ||
                 equals_ignore_case(tool_name, "Delete"))
        {
            std::string file_path = get_first_string(tool_input, {"file_path", "path"});
            return strip_cwd(file_path, cwd);
        }
        else if (equals_ignore_case(tool_name, "EditNotebook"))
        {
            std::string file_path = get_string(tool_input, "target_notebook");
            return strip_cwd(file_path, cwd);
        }

        return "";
    }

    std::string HookParser::get_string(const nlohmann::json &element, const std::string &property)
    {
        if (element.contains(property) && element[property].is_string())
        {
            return element[property].get<std::string>();
        }
        return "";
    }

    std::string HookParser::get_first_string(
        const nlohmann::json &element,
        std::initializer_list<std::string> properties)
    {
        for (const auto &property : properties)
        {
            std::string value = get_string(element, property);
            if (!value.empty())
            {
                return value;
            }
        }
        return "";
    }

    std::string HookParser::extract_project_name(const std::string &cwd)
    {
        if (cwd.empty())
            return "";

        std::string trimmed = cwd;
        // Trim trailing separators
        while (!trimmed.empty() && (trimmed.back() == '/' || trimmed.back() == '\\'))
        {
            trimmed.pop_back();
        }

        if (trimmed.empty())
            return "";

        // Find last separator
        size_t pos = trimmed.find_last_of("/\\");
        if (pos == std::string::npos)
        {
            return trimmed;
        }

        return trimmed.substr(pos + 1);
    }

    std::string HookParser::strip_cwd(const std::string &file_path, const std::string &cwd)
    {
        if (file_path.empty() || cwd.empty())
            return file_path;

        // Normalize cwd with trailing separator
        std::string normalized_cwd = cwd;
        while (!normalized_cwd.empty() && (normalized_cwd.back() == '/' || normalized_cwd.back() == '\\'))
        {
            normalized_cwd.pop_back();
        }

        // Try with backslash separator
        std::string cwd_with_sep = normalized_cwd + "\\";
        if (starts_with_ignore_case(file_path, cwd_with_sep))
        {
            return file_path.substr(cwd_with_sep.length());
        }

        // Try with forward slash separator
        cwd_with_sep = normalized_cwd + "/";
        if (starts_with_ignore_case(file_path, cwd_with_sep))
        {
            return file_path.substr(cwd_with_sep.length());
        }

        return file_path;
    }

} // namespace notiman
