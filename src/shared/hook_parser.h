#pragma once
#include <string>
#include <optional>
#include "payload.h"

namespace notiman {

class HookParser {
public:
    static std::optional<NotificationPayload> try_parse(const std::string& json_str);

private:
    static std::optional<NotificationPayload> map_notification(const nlohmann::json& root);
    static std::optional<NotificationPayload> map_post_tool_use(const nlohmann::json& root, const std::string& cwd);
    static std::optional<NotificationPayload> map_post_tool_use_failure(const nlohmann::json& root);
    static std::optional<NotificationPayload> map_stop();
    static std::optional<NotificationPayload> map_subagent_stop(const nlohmann::json& root);
    static std::optional<NotificationPayload> map_session_start(const nlohmann::json& root);

    static std::string get_string(const nlohmann::json& element, const std::string& property);
    static std::string extract_project_name(const std::string& cwd);
    static std::string strip_cwd(const std::string& file_path, const std::string& cwd);
    static std::string extract_tool_summary(const nlohmann::json& root, const std::string& tool_name, const std::string& cwd);
};

} // namespace notiman
