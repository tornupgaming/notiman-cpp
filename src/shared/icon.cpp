#include "icon.h"
#include <stdexcept>

namespace notiman {

NotificationIcon icon_from_string(const std::string& s) {
    if (s == "info") return NotificationIcon::Info;
    if (s == "success") return NotificationIcon::Success;
    if (s == "warning") return NotificationIcon::Warning;
    if (s == "error") return NotificationIcon::Error;
    throw std::invalid_argument("Invalid icon: " + s);
}

std::string icon_to_string(NotificationIcon icon) {
    switch (icon) {
        case NotificationIcon::Info: return "info";
        case NotificationIcon::Success: return "success";
        case NotificationIcon::Warning: return "warning";
        case NotificationIcon::Error: return "error";
    }
    throw std::logic_error("Unknown icon value");
}

} // namespace notiman
