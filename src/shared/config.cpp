#include "config.h"
#include <fstream>
#include <sstream>
#include <toml11/parser.hpp>
#include <windows.h>
#include <shlobj.h>

namespace notiman {

static uint32_t parse_hex_color(const std::string& hex) {
    // Parse "#7C3AED" -> 0xFF7C3AED (ARGB with full alpha)
    if (hex.empty() || hex[0] != '#') return 0xFF7C3AED;

    std::string digits = hex.substr(1);
    if (digits.length() != 6) return 0xFF7C3AED;

    uint32_t rgb = std::stoul(digits, nullptr, 16);
    return 0xFF000000 | rgb;  // Add full alpha
}

NotimanConfig NotimanConfig::parse(const std::string& toml_str) {
    NotimanConfig config;
    if (toml_str.empty()) return config;

    try {
        auto data = toml::parse_str(toml_str);

        if (data.contains("port")) {
            config.port = static_cast<int>(data["port"].as_integer());
        }
        if (data.contains("corner")) {
            config.corner = corner_from_string(data["corner"].as_string());
        }
        if (data.contains("monitor")) {
            config.monitor = static_cast<int>(data["monitor"].as_integer());
        }
        if (data.contains("max_visible")) {
            config.max_visible = static_cast<int>(data["max_visible"].as_integer());
        }
        if (data.contains("duration")) {
            config.duration = static_cast<int>(data["duration"].as_integer());
        }
        if (data.contains("width")) {
            config.width = static_cast<int>(data["width"].as_integer());
        }
        if (data.contains("accent_color")) {
            config.accent_color = parse_hex_color(data["accent_color"].as_string());
        }
        if (data.contains("opacity")) {
            config.opacity = data["opacity"].as_floating();
        }
    } catch (...) {
        // Return default config on parse failure
    }

    return config;
}

NotimanConfig NotimanConfig::load_from_file(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return NotimanConfig();
    }

    std::ifstream file(path);
    if (!file) return NotimanConfig();

    std::stringstream buffer;
    buffer << file.rdbuf();
    return parse(buffer.str());
}

std::filesystem::path NotimanConfig::default_config_path() {
    // Primary: %APPDATA%/notiman/config.toml
    WCHAR appdata_path[MAX_PATH];
    if (SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appdata_path) == S_OK) {
        std::filesystem::path primary(appdata_path);
        primary /= "notiman";
        primary /= "config.toml";
        if (std::filesystem::exists(primary)) {
            return primary;
        }
    }

    // Fallback: <exe_dir>/config/default.toml
    WCHAR exe_path[MAX_PATH];
    if (GetModuleFileNameW(NULL, exe_path, MAX_PATH) > 0) {
        std::filesystem::path fallback(exe_path);
        fallback = fallback.parent_path() / "config" / "default.toml";
        return fallback;
    }

    return "config/default.toml";
}

} // namespace notiman
