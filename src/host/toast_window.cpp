#include "toast_window.h"
#include "icon_paths.h"
#include "render_utils.h"
#include <cmath>

namespace notiman
{

    ToastWindow::ToastWindow(const NotificationPayload &payload,
                             const NotimanConfig &config,
                             ID2D1Factory *d2dFactory,
                             IDWriteFactory *dwFactory)
        : payload_(payload), config_(config), d2d_factory_(d2dFactory), dw_factory_(dwFactory)
    {
        // Register window class (only once)
        static bool class_registered = false;
        if (!class_registered)
        {
            WNDCLASSEXW wc = {};
            wc.cbSize = sizeof(WNDCLASSEXW);
            wc.lpfnWndProc = StaticWndProc;
            wc.hInstance = GetModuleHandle(nullptr);
            wc.lpszClassName = L"NotimanToastClass";
            RegisterClassExW(&wc);
            class_registered = true;
        }

        // Create window with toast styles
        hwnd_ = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            L"NotimanToastClass",
            L"Toast",
            WS_POPUP,
            0, 0,
            config_.width, 100, // Height will be adjusted in SizeToContent
            nullptr,
            nullptr,
            GetModuleHandle(nullptr),
            this // Pass this pointer for retrieval in WndProc
        );

        // Set layered window opacity
        SetLayeredWindowAttributes(hwnd_, 0,
                                   static_cast<BYTE>(config_.opacity * 255), LWA_ALPHA);

        // Layered popup windows and system backdrops (Mica/Acrylic) do not mix reliably.
        // Enabling backdrop here often causes a solid black surface on some systems.
        // Keep the toast purely custom-rendered instead.

        // -- Text formats --

        // Title: Segoe UI SemiBold 13
        dw_factory_->CreateTextFormat(
            L"Segoe UI", nullptr,
            DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            13.0f, L"en-us",
            title_format_.GetAddressOf());

        // Body: Segoe UI 12, word-wrapping
        dw_factory_->CreateTextFormat(
            L"Segoe UI", nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            12.0f, L"en-us",
            body_format_.GetAddressOf());
        if (body_format_)
        {
            body_format_->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
        }

        // Code: Consolas 11
        dw_factory_->CreateTextFormat(
            L"Consolas", nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            11.0f, L"en-us",
            code_format_.GetAddressOf());

        // Project: Segoe UI 12, right-aligned
        dw_factory_->CreateTextFormat(
            L"Segoe UI", nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            12.0f, L"en-us",
            project_format_.GetAddressOf());
        if (project_format_)
        {
            project_format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
        }

        // -- Text layouts --
        float layout_width = static_cast<float>(config_.width - 40 - 12 - 12); // icon+margin+padding

        // Title layout
        if (title_format_)
        {
            dw_factory_->CreateTextLayout(
                payload_.title.c_str(),
                static_cast<UINT32>(payload_.title.length()),
                title_format_.Get(),
                layout_width,
                1000.0f,
                title_layout_.GetAddressOf());
            if (title_layout_)
            {
                DWRITE_TRIMMING trimming = {DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
                title_layout_->SetTrimming(&trimming, nullptr);
            }
        }

        // Body layout
        if (!payload_.body.empty() && body_format_)
        {
            dw_factory_->CreateTextLayout(
                payload_.body.c_str(),
                static_cast<UINT32>(payload_.body.length()),
                body_format_.Get(),
                layout_width,
                1000.0f,
                body_layout_.GetAddressOf());
        }

        // Code layout (max 28px height, character trimming)
        if (!payload_.code.empty() && code_format_)
        {
            dw_factory_->CreateTextLayout(
                payload_.code.c_str(),
                static_cast<UINT32>(payload_.code.length()),
                code_format_.Get(),
                layout_width,
                28.0f,
                code_layout_.GetAddressOf());
            if (code_layout_)
            {
                code_layout_->SetMaxHeight(28.0f);
                DWRITE_TRIMMING trimming = {DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
                code_layout_->SetTrimming(&trimming, nullptr);
            }
        }

        // Project layout
        if (!payload_.project.empty() && project_format_)
        {
            dw_factory_->CreateTextLayout(
                payload_.project.c_str(),
                static_cast<UINT32>(payload_.project.length()),
                project_format_.Get(),
                layout_width,
                1000.0f,
                project_layout_.GetAddressOf());
        }

        // Calculate total content height (SizeToContent equivalent)
        float total_height = 12.0f; // Top padding

        DWRITE_TEXT_METRICS title_metrics;
        title_layout_->GetMetrics(&title_metrics);
        total_height += title_metrics.height + 4.0f; // Title + gap

        if (body_layout_)
        {
            DWRITE_TEXT_METRICS body_metrics;
            body_layout_->GetMetrics(&body_metrics);
            total_height += body_metrics.height + 4.0f; // Body + gap
        }

        if (code_layout_)
        {
            DWRITE_TEXT_METRICS code_metrics;
            code_layout_->GetMetrics(&code_metrics);
            total_height += code_metrics.height + 8.0f + 4.0f; // Code block (with padding) + gap
        }

        total_height += 12.0f; // Bottom padding

        // Get actual DPI and scale dimensions
        // DirectWrite returns DIPs (96 DPI base), need to convert to physical pixels
        UINT dpi = GetDpiForWindow(hwnd_);
        float dpi_scale = dpi / 96.0f;

        // Resize window to content height (scale to physical pixels and round up)
        const int window_width_px = static_cast<int>(std::ceil(config_.width * dpi_scale));
        const int window_height_px = static_cast<int>(std::ceil(total_height * dpi_scale));
        SetWindowPos(hwnd_, nullptr, 0, 0,
                     window_width_px,
                     window_height_px,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

        // Clip the window to a rounded region so corners stay transparent on layered windows.
        const int raw_corner_radius_px = static_cast<int>(std::ceil(8.0f * dpi_scale));
        const int corner_radius_px = (raw_corner_radius_px > 1) ? raw_corner_radius_px : 1;
        HRGN rounded_region = CreateRoundRectRgn(
            0, 0,
            window_width_px + 1, window_height_px + 1,
            corner_radius_px * 2, corner_radius_px * 2);
        if (rounded_region)
        {
            // The system owns rounded_region after successful SetWindowRgn.
            SetWindowRgn(hwnd_, rounded_region, TRUE);
        }
    }

    ToastWindow::~ToastWindow()
    {
        StopAutoDismissTimer();
        StopAnimation();
        StopMoveAnimation();
        anim_complete_callback_ = nullptr;
        dismiss_callback_ = nullptr;
        if (hwnd_)
        {
            DestroyWindow(hwnd_);
        }
    }

    void ToastWindow::Show()
    {
        ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    }

    double ToastWindow::GetHeight() const
    {
        RECT rc;
        GetWindowRect(hwnd_, &rc);
        return static_cast<double>(rc.bottom - rc.top);
    }

    double ToastWindow::GetWidth() const
    {
        RECT rc;
        GetWindowRect(hwnd_, &rc);
        return static_cast<double>(rc.right - rc.left);
    }

    void ToastWindow::SetDismissCallback(DismissCallback cb)
    {
        dismiss_callback_ = std::move(cb);
    }

    void ToastWindow::StartAutoDismissTimer(int duration_ms)
    {
        if (!auto_dismiss_enabled_)
            return;
        StopAutoDismissTimer(); // Clear existing
        dismiss_timer_id_ = SetTimer(hwnd_, 2, duration_ms, nullptr);
    }

    void ToastWindow::StopAutoDismissTimer()
    {
        if (dismiss_timer_id_)
        {
            KillTimer(hwnd_, dismiss_timer_id_);
            dismiss_timer_id_ = 0;
        }
    }

    LRESULT CALLBACK ToastWindow::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        ToastWindow *pThis = nullptr;

        if (msg == WM_CREATE)
        {
            auto pCreate = reinterpret_cast<CREATESTRUCT *>(lParam);
            pThis = static_cast<ToastWindow *>(pCreate->lpCreateParams);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        }
        else
        {
            pThis = reinterpret_cast<ToastWindow *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        }

        if (pThis)
        {
            return pThis->WndProc(msg, wParam, lParam);
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    LRESULT ToastWindow::WndProc(UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg)
        {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
            Render();
            ValidateRect(hwnd_, nullptr);
            return 0;

        case WM_TIMER:
            if (wParam == dismiss_timer_id_)
            {
                StopAutoDismissTimer();
                if (dismiss_callback_)
                {
                    dismiss_callback_(this);
                }
                return 0;
            }
            if (wParam == anim_timer_id_)
            {
                UpdateAnimation();
                return 0;
            }
            if (wParam == move_timer_id_)
            {
                UpdateMoveAnimation();
                return 0;
            }
            break;

        case WM_MOUSEMOVE:
            if (!mouse_tracked_)
            {
                TRACKMOUSEEVENT tme = {};
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE | TME_HOVER;
                tme.hwndTrack = hwnd_;
                tme.dwHoverTime = 100;
                TrackMouseEvent(&tme);
                mouse_tracked_ = true;
            }
            break;

        case WM_MOUSEHOVER:
            // Pause auto-dismiss when hovering
            StopAutoDismissTimer();
            break;

        case WM_MOUSELEAVE:
            // Resume auto-dismiss when leaving
            StartAutoDismissTimer(config_.duration);
            mouse_tracked_ = false;
            break;

        case WM_LBUTTONDOWN:
            StopAutoDismissTimer();
            if (dismiss_callback_)
            {
                dismiss_callback_(this);
            }
            return 0;
        }
        return DefWindowProc(hwnd_, msg, wParam, lParam);
    }

    void ToastWindow::Render()
    {
        // Lazy-init render target on first paint
        if (!render_target_)
        {
            RECT rc;
            GetClientRect(hwnd_, &rc);
            D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

            // Get actual DPI and set render target to match
            UINT dpi = GetDpiForWindow(hwnd_);

            HRESULT hr = d2d_factory_->CreateHwndRenderTarget(
                D2D1::RenderTargetProperties(
                    D2D1_RENDER_TARGET_TYPE_DEFAULT,
                    D2D1::PixelFormat(),
                    static_cast<float>(dpi),
                    static_cast<float>(dpi)),
                D2D1::HwndRenderTargetProperties(hwnd_, size),
                render_target_.GetAddressOf());
            if (FAILED(hr))
                return;

            // Create reusable brush alongside render target
            render_target_->CreateSolidColorBrush(
                D2D1::ColorF(0.078f, 0.078f, 0.125f, static_cast<float>(config_.opacity)),
                brush_.GetAddressOf());
        }

        render_target_->BeginDraw();

        // Clear full client to transparent first
        render_target_->Clear(D2D1::ColorF(0, 0.0f));

        // Draw rounded rectangle background
        RECT rc;
        GetClientRect(hwnd_, &rc);

        // Client rect is in physical pixels; convert to DIPs for a DPI-aware render target.
        const float dpi = static_cast<float>(GetDpiForWindow(hwnd_));
        const float px_to_dip = 96.0f / dpi;
        const float client_w_dip = static_cast<float>(rc.right - rc.left) * px_to_dip;
        const float client_h_dip = static_cast<float>(rc.bottom - rc.top) * px_to_dip;

        // Inset by 0.5 DIP to keep a 1px stroke fully inside bounds.
        D2D1_ROUNDED_RECT roundedRect = D2D1::RoundedRect(
            D2D1::RectF(0.5f, 0.5f, client_w_dip - 0.5f, client_h_dip - 0.5f),
            8.0f, 8.0f);

        if (brush_)
        {
            // Fill background
            brush_->SetColor(D2D1::ColorF(0.078f, 0.078f, 0.125f, 0.9f));
            render_target_->FillRoundedRectangle(roundedRect, brush_.Get());

            // Draw border (#26FFFFFF = white at 15% alpha)
            brush_->SetColor(D2D1::ColorF(D2D1::ColorF::White, 0.15f));
            render_target_->DrawRoundedRectangle(roundedRect, brush_.Get(), 1.0f);
        }

        // Draw icon
        auto iconPath = GetIconPath(payload_.icon);
        auto iconGeometry = ParseSvgPath(d2d_factory_, iconPath);

        if (iconGeometry)
        {
            // Create transform to scale 16x16 icon and position at (12, 12)
            D2D1_MATRIX_3X2_F transform = D2D1::Matrix3x2F::Scale(1.0f, 1.0f) *
                                          D2D1::Matrix3x2F::Translation(12.0f, 12.0f);
            render_target_->SetTransform(transform);

            // Set icon color
            uint32_t iconColor = GetIconColor(payload_.icon);
            brush_->SetColor(ColorFromHex(iconColor));

            // Fill icon geometry
            render_target_->FillGeometry(iconGeometry.Get(), brush_.Get());

            // Reset transform
            render_target_->SetTransform(D2D1::Matrix3x2F::Identity());
        }

        // -- Draw text --

        // Title (white)
        if (title_layout_)
        {
            brush_->SetColor(D2D1::ColorF(D2D1::ColorF::White, 1.0f));
            render_target_->DrawTextLayout(
                D2D1::Point2F(40.0f, 12.0f),
                title_layout_.Get(),
                brush_.Get());
        }

        float y_offset = 12.0f;
        if (title_layout_)
        {
            DWRITE_TEXT_METRICS title_metrics;
            title_layout_->GetMetrics(&title_metrics);
            y_offset += title_metrics.height + 4.0f;
        }

        // Body (80% white)
        if (body_layout_)
        {
            brush_->SetColor(D2D1::ColorF(D2D1::ColorF::White, 0.8f));
            render_target_->DrawTextLayout(
                D2D1::Point2F(40.0f, y_offset),
                body_layout_.Get(),
                brush_.Get());

            DWRITE_TEXT_METRICS body_metrics;
            body_layout_->GetMetrics(&body_metrics);
            y_offset += body_metrics.height + 4.0f;
        }

        // Code block (background + accent border + text)
        if (code_layout_)
        {
            DWRITE_TEXT_METRICS code_metrics;
            code_layout_->GetMetrics(&code_metrics);

            D2D1_ROUNDED_RECT code_rect = D2D1::RoundedRect(
                D2D1::RectF(40.0f, y_offset,
                            static_cast<float>(config_.width - 12),
                            y_offset + code_metrics.height + 8.0f),
                4.0f, 4.0f);

            // Background
            brush_->SetColor(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.4f));
            render_target_->FillRoundedRectangle(code_rect, brush_.Get());

            // Left accent border (2px)
            brush_->SetColor(ColorFromHex(config_.accent_color));
            render_target_->DrawLine(
                D2D1::Point2F(40.0f, y_offset),
                D2D1::Point2F(40.0f, y_offset + code_metrics.height + 8.0f),
                brush_.Get(),
                2.0f);

            // Code text
            brush_->SetColor(D2D1::ColorF(D2D1::ColorF::White, 0.93f));
            render_target_->DrawTextLayout(
                D2D1::Point2F(48.0f, y_offset + 4.0f),
                code_layout_.Get(),
                brush_.Get());
        }

        // Project (muted blue-purple, right-aligned, same row as title)
        if (project_layout_)
        {
            brush_->SetColor(D2D1::ColorF(0.667f, 0.8f, 1.0f, 0.67f));
            render_target_->DrawTextLayout(
                D2D1::Point2F(40.0f, 12.0f),
                project_layout_.Get(),
                brush_.Get());
        }

        HRESULT hr = render_target_->EndDraw();
        if (hr == D2DERR_RECREATE_TARGET)
        {
            render_target_.Reset();
            brush_.Reset();
        }
    }

    void ToastWindow::StartAnimation(AnimState state, std::function<void(ToastWindow *)> on_complete)
    {
        anim_state_ = state;
        anim_complete_callback_ = std::move(on_complete);
        anim_progress_ = 0.0;

        RECT current = {};
        GetWindowRect(hwnd_, &current);
        anim_target_pos_.x = current.left;
        anim_target_pos_.y = current.top;

        if (state == AnimState::FadingIn)
        {
            constexpr int kSlideX = 28;
            constexpr int kSlideY = 10;
            int offset_x = 0;
            int offset_y = 0;

            switch (config_.corner)
            {
            case Corner::TopLeft:
                offset_x = -kSlideX;
                offset_y = -kSlideY;
                break;
            case Corner::TopRight:
                offset_x = kSlideX;
                offset_y = -kSlideY;
                break;
            case Corner::BottomLeft:
                offset_x = -kSlideX;
                offset_y = kSlideY;
                break;
            case Corner::BottomRight:
                offset_x = kSlideX;
                offset_y = kSlideY;
                break;
            }

            anim_start_pos_.x = anim_target_pos_.x + offset_x;
            anim_start_pos_.y = anim_target_pos_.y + offset_y;

            SetWindowPos(hwnd_, nullptr,
                         anim_start_pos_.x,
                         anim_start_pos_.y,
                         0, 0,
                         SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            SetLayeredWindowAttributes(hwnd_, 0, 0, LWA_ALPHA);
        }
        else
        {
            anim_start_pos_ = anim_target_pos_;
        }

        // Start 16ms timer (~60 FPS)
        anim_timer_id_ = SetTimer(hwnd_, 1, 16, nullptr);
    }

    void ToastWindow::StartMoveAnimation(int target_x, int target_y, int duration_ms)
    {
        RECT current = {};
        GetWindowRect(hwnd_, &current);

        move_start_pos_.x = current.left;
        move_start_pos_.y = current.top;
        move_target_pos_.x = target_x;
        move_target_pos_.y = target_y;

        if (move_start_pos_.x == move_target_pos_.x && move_start_pos_.y == move_target_pos_.y)
        {
            return;
        }

        move_duration_ms_ = (duration_ms > 0) ? duration_ms : 170;
        move_progress_ = 0.0;

        StopMoveAnimation();
        move_timer_id_ = SetTimer(hwnd_, 3, 16, nullptr);
    }

    void ToastWindow::UpdateAnimation()
    {
        constexpr double ANIM_DURATION_MS = 125.0;
        constexpr double FRAME_MS = 16.0; // ~60 FPS

        anim_progress_ += FRAME_MS / ANIM_DURATION_MS;

        double eased = EaseOutCubic(anim_progress_);
        BYTE target_opacity = static_cast<BYTE>(config_.opacity * 255);

        if (anim_state_ == AnimState::FadingIn)
        {
            const int x = static_cast<int>(std::lround(
                anim_start_pos_.x + (anim_target_pos_.x - anim_start_pos_.x) * eased));
            const int y = static_cast<int>(std::lround(
                anim_start_pos_.y + (anim_target_pos_.y - anim_start_pos_.y) * eased));
            SetWindowPos(hwnd_, nullptr, x, y, 0, 0,
                         SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }

        if (anim_progress_ >= 1.0)
        {
            anim_progress_ = 1.0;

            if (anim_state_ == AnimState::FadingIn)
            {
                SetWindowPos(hwnd_, nullptr,
                             anim_target_pos_.x,
                             anim_target_pos_.y,
                             0, 0,
                             SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                SetLayeredWindowAttributes(hwnd_, 0, target_opacity, LWA_ALPHA);
            }
            else if (anim_state_ == AnimState::FadingOut)
            {
                SetLayeredWindowAttributes(hwnd_, 0, 0, LWA_ALPHA);
            }

            StopAnimation();
            if (anim_complete_callback_)
            {
                auto callback = std::move(anim_complete_callback_);
                anim_complete_callback_ = nullptr;
                callback(this);
            }
            return;
        }

        // Update layered window opacity based on animation state
        switch (anim_state_)
        {
        case AnimState::FadingIn:
            SetLayeredWindowAttributes(hwnd_, 0, static_cast<BYTE>(eased * target_opacity), LWA_ALPHA);
            break;
        case AnimState::FadingOut:
            SetLayeredWindowAttributes(hwnd_, 0, static_cast<BYTE>((1.0 - eased) * target_opacity), LWA_ALPHA);
            break;
        case AnimState::None:
            break;
        }
    }

    void ToastWindow::StopAnimation()
    {
        if (anim_timer_id_)
        {
            KillTimer(hwnd_, anim_timer_id_);
            anim_timer_id_ = 0;
        }
        anim_state_ = AnimState::None;
        anim_progress_ = 0.0;
    }

    void ToastWindow::UpdateMoveAnimation()
    {
        constexpr double FRAME_MS = 16.0;

        move_progress_ += FRAME_MS / static_cast<double>(move_duration_ms_);
        if (move_progress_ > 1.0)
        {
            move_progress_ = 1.0;
        }

        const double eased = EaseOutCubic(move_progress_);
        const int x = static_cast<int>(std::lround(
            move_start_pos_.x + (move_target_pos_.x - move_start_pos_.x) * eased));
        const int y = static_cast<int>(std::lround(
            move_start_pos_.y + (move_target_pos_.y - move_start_pos_.y) * eased));

        SetWindowPos(hwnd_, nullptr, x, y, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

        if (move_progress_ >= 1.0)
        {
            SetWindowPos(hwnd_, nullptr, move_target_pos_.x, move_target_pos_.y, 0, 0,
                         SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            StopMoveAnimation();
        }
    }

    void ToastWindow::StopMoveAnimation()
    {
        if (move_timer_id_)
        {
            KillTimer(hwnd_, move_timer_id_);
            move_timer_id_ = 0;
        }
        move_progress_ = 0.0;
    }

    double ToastWindow::EaseOutCubic(double t)
    {
        double f = t - 1.0;
        return f * f * f + 1.0;
    }

} // namespace notiman
