#pragma once
#include <span>
#include "corner.h"

namespace notiman {

struct ToastPosition {
    double x;
    double y;
};

ToastPosition calculate_position(
    Corner corner,
    double screen_width,
    double screen_height,
    double toast_width,
    double toast_height,
    int index,
    std::span<const double> preceding_heights,
    double margin = 16.0,
    double gap = 8.0
);

} // namespace notiman
