#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <functional>
#include "../shared/payload.h"
#include "../shared/config.h"

using Microsoft::WRL::ComPtr;

namespace notiman {

class ToastWindow {
public:
    enum class AnimState { None, FadingIn, FadingOut };

    ToastWindow(const NotificationPayload& payload,
                const NotimanConfig& config,
                ID2D1Factory* d2dFactory,
                IDWriteFactory* dwFactory);
    ~ToastWindow();

    void Show();
    double GetHeight() const;
    double GetWidth() const;
    HWND GetHwnd() const { return hwnd_; }

    using DismissCallback = std::function<void(ToastWindow*)>;
    void SetDismissCallback(DismissCallback cb);

    void StartAutoDismissTimer(int duration_ms);
    void StopAutoDismissTimer();
    void StartAnimation(AnimState state, std::function<void(ToastWindow*)> on_complete);

private:
    static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);

    void Render();

    HWND hwnd_ = nullptr;
    NotificationPayload payload_;
    NotimanConfig config_;
    ID2D1Factory* d2d_factory_;  // Non-owning
    IDWriteFactory* dw_factory_;  // Non-owning
    ComPtr<ID2D1HwndRenderTarget> render_target_;
    ComPtr<ID2D1SolidColorBrush> brush_;
    DismissCallback dismiss_callback_;

    // Text formats
    ComPtr<IDWriteTextFormat> title_format_;
    ComPtr<IDWriteTextFormat> body_format_;
    ComPtr<IDWriteTextFormat> code_format_;
    ComPtr<IDWriteTextFormat> project_format_;

    // Text layouts
    ComPtr<IDWriteTextLayout> title_layout_;
    ComPtr<IDWriteTextLayout> body_layout_;
    ComPtr<IDWriteTextLayout> code_layout_;
    ComPtr<IDWriteTextLayout> project_layout_;

    // Animation state
    AnimState anim_state_ = AnimState::None;
    UINT_PTR anim_timer_id_ = 0;
    double anim_progress_ = 0.0;  // 0.0 to 1.0

    // Animation callback
    std::function<void(ToastWindow*)> anim_complete_callback_;

    // Animation methods
    void UpdateAnimation();
    void StopAnimation();
    double EaseOutCubic(double t);

    UINT_PTR dismiss_timer_id_ = 0;
    bool auto_dismiss_enabled_ = true;
    bool mouse_tracked_ = false;
};

} // namespace notiman
