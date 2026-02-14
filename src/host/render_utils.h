#pragma once
#include <d2d1.h>
#include <wrl/client.h>
#include <string_view>

using Microsoft::WRL::ComPtr;

namespace notiman {

// Parse SVG path string (M, L, C, Z commands) into D2D PathGeometry
ComPtr<ID2D1PathGeometry> ParseSvgPath(ID2D1Factory* factory, std::wstring_view path_data);

// Convert ARGB uint32 to D2D1::ColorF
D2D1::ColorF ColorFromHex(uint32_t argb);

} // namespace notiman
