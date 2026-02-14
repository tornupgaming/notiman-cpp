#pragma once
#include <string>
#include <optional>
#include <nlohmann/json.hpp>
#include "icon.h"

namespace notiman {

struct NotificationPayload {
    std::wstring title;
    std::wstring body;
    std::wstring code;
    std::wstring project;
    NotificationIcon icon = NotificationIcon::Info;
    std::optional<int> duration;

    static NotificationPayload from_json(const nlohmann::json& j);
    nlohmann::json to_json() const;
};

} // namespace notiman
