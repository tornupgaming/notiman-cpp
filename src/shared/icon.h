#pragma once
#include <cstdint>
#include <string>

namespace notiman {

enum class NotificationIcon : uint8_t {
    Info,
    Success,
    Warning,
    Error
};

NotificationIcon icon_from_string(const std::string& s);
std::string icon_to_string(NotificationIcon icon);

} // namespace notiman
