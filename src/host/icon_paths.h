#pragma once
#include <string_view>
#include "../shared/icon.h"

namespace notiman {

constexpr std::wstring_view GetIconPath(NotificationIcon icon) {
    switch (icon) {
        case NotificationIcon::Info:
            return L"M10,0 C4.48,0 0,4.48 0,10 C0,15.52 4.48,20 10,20 C15.52,20 20,15.52 20,10 C20,4.48 15.52,0 10,0 Z M11,15 L9,15 L9,9 L11,9 Z M11,7 L9,7 L9,5 L11,5 Z";
        case NotificationIcon::Success:
            return L"M10,0 C4.48,0 0,4.48 0,10 C0,15.52 4.48,20 10,20 C15.52,20 20,15.52 20,10 C20,4.48 15.52,0 10,0 Z M8,15 L4,11 L5.41,9.59 L8,12.17 L14.59,5.58 L16,7 Z";
        case NotificationIcon::Warning:
            return L"M1,18 L10,2 L19,18 Z M11,15 L9,15 L9,13 L11,13 Z M11,12 L9,12 L9,8 L11,8 Z";
        case NotificationIcon::Error:
            return L"M10,0 C4.48,0 0,4.48 0,10 C0,15.52 4.48,20 10,20 C15.52,20 20,15.52 20,10 C20,4.48 15.52,0 10,0 Z M14.59,12.17 L12.17,14.59 L10,12.41 L7.83,14.59 L5.41,12.17 L7.59,10 L5.41,7.83 L7.83,5.41 L10,7.59 L12.17,5.41 L14.59,7.83 L12.41,10 Z";
    }
    return L"";
}

constexpr uint32_t GetIconColor(NotificationIcon icon) {
    switch (icon) {
        case NotificationIcon::Info:    return 0xFF60A5FA;  // Blue
        case NotificationIcon::Success: return 0xFF34D399;  // Green
        case NotificationIcon::Warning: return 0xFFFBBF24;  // Yellow
        case NotificationIcon::Error:   return 0xFFF87171;  // Red
    }
    return 0xFFFFFFFF;
}

} // namespace notiman
