#include "payload.h"
#include <windows.h>

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

    std::string utf16_to_utf8(const std::wstring& utf16) {
        if (utf16.empty()) return "";
        int len = WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (len <= 0) return "";
        std::string result(len - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), -1, &result[0], len, nullptr, nullptr);
        return result;
    }

    NotificationIcon string_to_icon(const std::string& s) {
        if (s == "success") return NotificationIcon::Success;
        if (s == "warning") return NotificationIcon::Warning;
        if (s == "error") return NotificationIcon::Error;
        return NotificationIcon::Info;
    }

}

NotificationPayload NotificationPayload::from_json(const nlohmann::json& j) {
    NotificationPayload payload;

    if (j.contains("title") && j["title"].is_string()) {
        payload.title = utf8_to_utf16(j["title"].get<std::string>());
    }

    if (j.contains("body") && j["body"].is_string()) {
        payload.body = utf8_to_utf16(j["body"].get<std::string>());
    }

    if (j.contains("code") && j["code"].is_string()) {
        payload.code = utf8_to_utf16(j["code"].get<std::string>());
    }

    if (j.contains("project") && j["project"].is_string()) {
        payload.project = utf8_to_utf16(j["project"].get<std::string>());
    }

    if (j.contains("icon") && j["icon"].is_string()) {
        payload.icon = string_to_icon(j["icon"].get<std::string>());
    }

    if (j.contains("duration") && j["duration"].is_number_integer()) {
        payload.duration = j["duration"].get<int>();
    }

    return payload;
}

nlohmann::json NotificationPayload::to_json() const {
    nlohmann::json j;

    if (!title.empty()) {
        j["title"] = utf16_to_utf8(title);
    }

    if (!body.empty()) {
        j["body"] = utf16_to_utf8(body);
    }

    if (!code.empty()) {
        j["code"] = utf16_to_utf8(code);
    }

    if (!project.empty()) {
        j["project"] = utf16_to_utf8(project);
    }

    j["icon"] = icon_to_string(icon);

    if (duration.has_value()) {
        j["duration"] = duration.value();
    }

    return j;
}

} // namespace notiman
