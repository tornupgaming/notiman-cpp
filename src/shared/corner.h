#pragma once
#include <string>

namespace notiman {

enum class Corner : uint8_t {
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight
};

Corner corner_from_string(const std::string& s);
std::string corner_to_string(Corner c);

} // namespace notiman
