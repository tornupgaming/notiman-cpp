#pragma once
#include <filesystem>
#include "corner.h"

namespace notiman {

struct NotimanConfig {
    Corner corner = Corner::BottomRight;
    int monitor = 0;
    int max_visible = 5;
    int duration = 4000;        // ms
    int width = 400;            // px
    uint32_t accent_color = 0xFF7C3AED;  // ARGB
    double opacity = 0.85;

    static NotimanConfig load_from_file(const std::filesystem::path& path);
    static std::filesystem::path default_config_path();
};

} // namespace notiman
