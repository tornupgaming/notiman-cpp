#include "acrylic.h"
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

namespace notiman {

bool EnableAcrylic(HWND hwnd) {
    // DWMWA_SYSTEMBACKDROP_TYPE = 38
    // DWMSBT_TRANSIENTWINDOW = 3
    int backdrop = 3;
    HRESULT hr = DwmSetWindowAttribute(
        hwnd,
        static_cast<DWORD>(38),
        &backdrop,
        sizeof(backdrop)
    );
    return SUCCEEDED(hr);
}

} // namespace notiman
