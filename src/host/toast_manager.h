#pragma once
#include <vector>
#include <queue>
#include <memory>
#include "../shared/payload.h"
#include "../shared/config.h"
#include "../shared/positioning.h"
#include "toast_window.h"

namespace notiman {

class ToastManager {
public:
    ToastManager(const NotimanConfig& config,
                 ID2D1Factory* d2dFactory,
                 IDWriteFactory* dwFactory);

    void Show(NotificationPayload payload);

private:
    void ShowOnUiThread(NotificationPayload payload);
    void DismissToast(ToastWindow* toast);
    void PositionAllToasts();
    void RepositionAfterRemoval();
    void AnimateToPosition(ToastWindow* toast, ToastPosition target);

    std::vector<std::unique_ptr<ToastWindow>> toasts_;
    std::queue<NotificationPayload> queue_;
    bool is_animating_ = false;

    NotimanConfig config_;
    ID2D1Factory* d2d_factory_;      // Non-owning
    IDWriteFactory* dw_factory_;     // Non-owning
};

} // namespace notiman
