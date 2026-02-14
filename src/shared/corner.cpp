#include "corner.h"
#include <stdexcept>

namespace notiman {

Corner corner_from_string(const std::string& s) {
    if (s == "TopLeft") return Corner::TopLeft;
    if (s == "TopRight") return Corner::TopRight;
    if (s == "BottomLeft") return Corner::BottomLeft;
    if (s == "BottomRight") return Corner::BottomRight;
    throw std::invalid_argument("Invalid corner: " + s);
}

std::string corner_to_string(Corner c) {
    switch (c) {
        case Corner::TopLeft: return "TopLeft";
        case Corner::TopRight: return "TopRight";
        case Corner::BottomLeft: return "BottomLeft";
        case Corner::BottomRight: return "BottomRight";
    }
    throw std::logic_error("Unknown corner value");
}

} // namespace notiman
