#include "positioning.h"

namespace notiman {

ToastPosition calculate_position(
    Corner corner,
    double screen_width,
    double screen_height,
    double toast_width,
    double toast_height,
    [[maybe_unused]] int index,
    std::span<const double> preceding_heights,
    double margin,
    double gap)
{
    // Calculate X position based on corner
    double x;
    if (corner == Corner::TopLeft || corner == Corner::BottomLeft) {
        x = margin;
    } else {
        x = screen_width - toast_width - margin;
    }

    // Calculate stack offset from preceding toasts
    double stack_offset = 0;
    for (const double height : preceding_heights) {
        stack_offset += height + gap;
    }

    // Calculate Y position based on corner
    double y;
    if (corner == Corner::TopLeft || corner == Corner::TopRight) {
        y = margin + stack_offset;
    } else {
        y = screen_height - toast_height - margin - stack_offset;
    }

    return ToastPosition{x, y};
}

} // namespace notiman
