#pragma once
#include <string>
#include <initializer_list>
#include <optional>
#include <vector>
#include "payload.h"

namespace notiman {

class HookParser {
public:
    static std::optional<NotificationPayload> try_parse(
        const std::string& json_str,
        const std::vector<std::string>& ignored_tools = {});

private:
    static std::string normalize_event_name(const std::string& event_name);
    static std::optional<NotificationPayload> map_notification(const nlohmann::json& root);
    static std::optional<NotificationPayload> map_post_tool_use(
        const nlohmann::json& root,
        const std::string& cwd,
        const std::vector<std::string>& ignored_tools);
    static std::optional<NotificationPayload> map_post_tool_use_failure(
        const nlohmann::json& root,
        const std::vector<std::string>& ignored_tools);
    static std::optional<NotificationPayload> map_after_shell_execution(const nlohmann::json& root);
    static std::optional<NotificationPayload> map_after_mcp_execution(
        const nlohmann::json& root,
        const std::vector<std::string>& ignored_tools);
    static std::optional<NotificationPayload> map_after_file_edit(
        const nlohmann::json& root,
        const std::string& cwd);
    static std::optional<NotificationPayload> map_stop(const nlohmann::json& root);
    static std::optional<NotificationPayload> map_subagent_stop(const nlohmann::json& root);
    static std::optional<NotificationPayload> map_session_start(const nlohmann::json& root);

    static std::string get_string(const nlohmann::json& element, const std::string& property);
    static std::string get_first_string(
        const nlohmann::json& element,
        std::initializer_list<std::string> properties);
    static std::string extract_project_name(const std::string& cwd);
    static std::string strip_cwd(const std::string& file_path, const std::string& cwd);
    static std::string extract_tool_summary(const nlohmann::json& root, const std::string& tool_name, const std::string& cwd);
};

} // namespace notiman
