#include "toast_manager.h"
#include "../shared/positioning.h"
#include <algorithm>

namespace notiman {

ToastManager::ToastManager(const NotimanConfig& config,
                           ID2D1Factory* d2dFactory,
                           IDWriteFactory* dwFactory)
    : config_(config)
    , d2d_factory_(d2dFactory)
    , dw_factory_(dwFactory)
{
}

void ToastManager::Show(NotificationPayload payload) {
    ShowOnUiThread(std::move(payload));
}

void ToastManager::ShowOnUiThread(NotificationPayload payload) {
    // If animating, queue the notification
    if (is_animating_) {
        queue_.push(std::move(payload));
        return;
    }

    // If at max visible, queue and dismiss oldest
    if (toasts_.size() >= static_cast<size_t>(config_.max_visible)) {
        queue_.push(std::move(payload));
        DismissToast(toasts_[0].get());
        return;
    }

    // Create new toast
    auto toast = std::make_unique<ToastWindow>(
        payload,
        config_,
        d2d_factory_,
        dw_factory_
    );

    // Set dismiss callback
    toast->SetDismissCallback([this](ToastWindow* t) {
        DismissToast(t);
    });

    toasts_.push_back(std::move(toast));

    // Position and show
    PositionAllToasts();

    // Animate in and show
    auto* toast_ptr = toasts_.back().get();
    toast_ptr->StartAnimation(ToastWindow::AnimState::FadingIn,
        [this, toast_ptr](ToastWindow* t) {
            // Fade-in complete, start auto-dismiss timer
            t->StartAutoDismissTimer(config_.duration);
        });
    toast_ptr->Show();
}

void ToastManager::DismissToast(ToastWindow* toast) {
    // Find toast
    auto it = std::find_if(toasts_.begin(), toasts_.end(),
        [toast](const auto& t) { return t.get() == toast; });

    if (it == toasts_.end()) return;

    is_animating_ = true;

    // Animate out before removal
    auto* toast_ptr = it->get();
    toast_ptr->StartAnimation(ToastWindow::AnimState::FadingOut,
        [this](ToastWindow* t) {
            // Called when fade-out completes
            auto it = std::find_if(toasts_.begin(), toasts_.end(),
                [t](const auto& ptr) { return ptr.get() == t; });
            if (it != toasts_.end()) {
                toasts_.erase(it);
                RepositionAfterRemoval();
            }
            is_animating_ = false;

            // Dequeue next
            if (!queue_.empty()) {
                auto next = std::move(queue_.front());
                queue_.pop();
                ShowOnUiThread(std::move(next));
            }
        });
}

void ToastManager::PositionAllToasts() {
    // Get work area
    RECT work_area;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &work_area, 0);

    double screen_width = static_cast<double>(work_area.right - work_area.left);
    double screen_height = static_cast<double>(work_area.bottom - work_area.top);

    // Position each toast
    for (size_t i = 0; i < toasts_.size(); ++i) {
        auto& toast = toasts_[i];

        // Collect preceding heights
        std::vector<double> preceding_heights;
        for (size_t j = 0; j < i; ++j) {
            preceding_heights.push_back(toasts_[j]->GetHeight());
        }

        // Calculate position
        auto pos = calculate_position(
            config_.corner,
            screen_width,
            screen_height,
            toast->GetWidth(),
            toast->GetHeight(),
            static_cast<int>(i),
            preceding_heights
        );

        // Set position
        SetWindowPos(toast->GetHwnd(), nullptr,
            static_cast<int>(pos.x),
            static_cast<int>(pos.y),
            0, 0,
            SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void ToastManager::RepositionAfterRemoval() {
    // Smoothly animate toasts to new positions
    for (size_t i = 0; i < toasts_.size(); ++i) {
        auto& toast = toasts_[i];

        // Calculate target position
        std::vector<double> preceding_heights;
        for (size_t j = 0; j < i; ++j) {
            preceding_heights.push_back(toasts_[j]->GetHeight());
        }

        RECT work_area;
        SystemParametersInfo(SPI_GETWORKAREA, 0, &work_area, 0);
        auto target_pos = calculate_position(
            config_.corner,
            work_area.right - work_area.left,
            work_area.bottom - work_area.top,
            toast->GetWidth(),
            toast->GetHeight(),
            static_cast<int>(i),
            preceding_heights
        );

        // Animate to target position
        AnimateToPosition(toast.get(), target_pos);
    }
}

void ToastManager::AnimateToPosition(ToastWindow* toast, ToastPosition target) {
    // Get current position
    RECT current;
    GetWindowRect(toast->GetHwnd(), &current);

    // If already at target, skip
    if (current.left == target.x && current.top == target.y) return;

    // Use SetWindowPos with smooth movement
    SetWindowPos(toast->GetHwnd(), nullptr,
        static_cast<int>(target.x),
        static_cast<int>(target.y),
        0, 0,
        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

} // namespace notiman
