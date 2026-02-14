#include "render_utils.h"
#include <vector>
#include <sstream>

namespace notiman {

using std::wstringstream;

D2D1::ColorF ColorFromHex(uint32_t argb) {
    float a = ((argb >> 24) & 0xFF) / 255.0f;
    float r = ((argb >> 16) & 0xFF) / 255.0f;
    float g = ((argb >> 8) & 0xFF) / 255.0f;
    float b = (argb & 0xFF) / 255.0f;
    return D2D1::ColorF(r, g, b, a);
}

ComPtr<ID2D1PathGeometry> ParseSvgPath(ID2D1Factory* factory, std::wstring_view path_data) {
    ComPtr<ID2D1PathGeometry> geometry;
    if (FAILED(factory->CreatePathGeometry(&geometry))) {
        return nullptr;
    }

    ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(geometry->Open(&sink))) {
        return nullptr;
    }

    // Parse SVG path commands
    std::wstringstream ss{std::wstring(path_data)};
    wchar_t cmd = 0;
    bool figure_open = false;

    while (ss >> cmd) {
        if (cmd == L'M') {
            // MoveTo
            float x, y;
            wchar_t comma;
            if (ss >> x >> comma >> y) {
                if (figure_open) {
                    sink->EndFigure(D2D1_FIGURE_END_OPEN);
                }
                sink->BeginFigure(D2D1::Point2F(x, y), D2D1_FIGURE_BEGIN_FILLED);
                figure_open = true;
            }
        } else if (cmd == L'L') {
            // LineTo
            float x, y;
            wchar_t comma;
            if (ss >> x >> comma >> y) {
                sink->AddLine(D2D1::Point2F(x, y));
            }
        } else if (cmd == L'C') {
            // CubicBezier
            float x1, y1, x2, y2, x3, y3;
            wchar_t c1, c2, c3;
            if (ss >> x1 >> c1 >> y1 >> x2 >> c2 >> y2 >> x3 >> c3 >> y3) {
                D2D1_BEZIER_SEGMENT bezier;
                bezier.point1 = D2D1::Point2F(x1, y1);
                bezier.point2 = D2D1::Point2F(x2, y2);
                bezier.point3 = D2D1::Point2F(x3, y3);
                sink->AddBezier(bezier);
            }
        } else if (cmd == L'Z') {
            // CloseFigure
            if (figure_open) {
                sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                figure_open = false;
            }
        }
    }

    if (figure_open) {
        sink->EndFigure(D2D1_FIGURE_END_OPEN);
    }

    if (FAILED(sink->Close())) {
        return nullptr;
    }

    return geometry;
}

} // namespace notiman
