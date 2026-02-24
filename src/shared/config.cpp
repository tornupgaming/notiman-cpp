#include "config.h"
#include <windows.h>
#include <shlobj.h>

namespace notiman {

static std::string narrow_utf8(const wchar_t* value) {
    if (value == nullptr || value[0] == L'\0') return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) return {};
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, -1, result.data(), size, nullptr, nullptr);
    result.pop_back();
    return result;
}

static uint32_t parse_hex_color(const std::string& hex) {
    // Parse "#7C3AED" -> 0xFF7C3AED (ARGB with full alpha)
    if (hex.empty() || hex[0] != '#') return 0xFF7C3AED;

    std::string digits = hex.substr(1);
    if (digits.length() != 6) return 0xFF7C3AED;

    uint32_t rgb = std::stoul(digits, nullptr, 16);
    return 0xFF000000 | rgb;  // Add full alpha
}

NotimanConfig NotimanConfig::load_from_file(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return NotimanConfig();
    }
    NotimanConfig config;
    const std::wstring ini_path = path.wstring();
    constexpr const wchar_t* section = L"notiman";

    wchar_t corner_buf[64] = {};
    GetPrivateProfileStringW(
        section, L"corner", L"BottomRight", corner_buf, static_cast<DWORD>(_countof(corner_buf)), ini_path.c_str());
    config.corner = corner_from_string(narrow_utf8(corner_buf));

    config.monitor = static_cast<int>(GetPrivateProfileIntW(section, L"monitor", config.monitor, ini_path.c_str()));
    config.max_visible = static_cast<int>(GetPrivateProfileIntW(section, L"max_visible", config.max_visible, ini_path.c_str()));
    config.duration = static_cast<int>(GetPrivateProfileIntW(section, L"duration", config.duration, ini_path.c_str()));
    config.width = static_cast<int>(GetPrivateProfileIntW(section, L"width", config.width, ini_path.c_str()));

    wchar_t color_buf[64] = {};
    GetPrivateProfileStringW(
        section, L"accent_color", L"#7C3AED", color_buf, static_cast<DWORD>(_countof(color_buf)), ini_path.c_str());
    try {
        config.accent_color = parse_hex_color(narrow_utf8(color_buf));
    } catch (...) {
        // Keep default color if parse fails.
    }

    wchar_t opacity_buf[64] = {};
    GetPrivateProfileStringW(
        section, L"opacity", L"0.85", opacity_buf, static_cast<DWORD>(_countof(opacity_buf)), ini_path.c_str());
    try {
        config.opacity = std::stod(narrow_utf8(opacity_buf));
    } catch (...) {
        // Keep default opacity if parse fails.
    }

    return config;
}

std::filesystem::path NotimanConfig::default_config_path() {
    // Primary: %APPDATA%/notiman/config.ini
    WCHAR appdata_path[MAX_PATH];
    if (SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appdata_path) == S_OK) {
        std::filesystem::path primary(appdata_path);
        primary /= "notiman";
        primary /= "config.ini";
        if (std::filesystem::exists(primary)) {
            return primary;
        }
    }

    // Fallback: <exe_dir>/config/default.ini
    WCHAR exe_path[MAX_PATH];
    if (GetModuleFileNameW(NULL, exe_path, MAX_PATH) > 0) {
        std::filesystem::path fallback(exe_path);
        fallback = fallback.parent_path() / "config" / "default.ini";
        return fallback;
    }

    return "config/default.ini";
}

} // namespace notiman
