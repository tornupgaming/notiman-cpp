# Phase 5 & 6 Implementation Plan

**Project**: Notiman Toast Notification System
**Features**: Animations + Auto-Dismiss Timers
**Estimated Complexity**: Medium (Win32 timers, easing functions, layered window updates)

---

## Overview

Complete the notification system by adding:
- **Phase 5**: Smooth animations for toast appearance, dismissal, and repositioning
- **Phase 6**: Auto-dismiss timers based on `config.duration`

---

## Current State

### ✅ Already Implemented
- Core toast rendering with Direct2D
- Multi-toast positioning system
- Queue management for overflow
- `is_animating_` flag infrastructure (lines 22, 64, 71 in toast_manager.cpp)
- `config.duration` field (4000ms default)

### ❌ Not Implemented (TODOs in code)
- `AnimateIn()` - Line 53 of toast_manager.cpp
- `AnimateOut()` - Line 66 of toast_manager.cpp
- `AnimateCollapse()` - Line 66 of toast_manager.cpp
- Auto-dismiss timer - Line 54 of toast_manager.cpp
- Animated repositioning - Line 120 of toast_manager.cpp

---

## Phase 5: Animations

### Requirements

1. **AnimateIn**: Toast slides in from edge + fades in (0% → 100% opacity)
2. **AnimateOut**: Toast fades out (100% → 0% opacity) before removal
3. **AnimateCollapse**: Remaining toasts smoothly reposition when one closes
4. **Duration**: ~200-300ms for in/out, ~150ms for collapse
5. **Easing**: Ease-out for natural feel

### Technical Approach

#### 5.1: Add Animation Infrastructure

**File**: `src/host/toast_window.h`

Add to `ToastWindow` class:
```cpp
private:
    // Animation state
    enum class AnimState { None, FadingIn, FadingOut };
    AnimState anim_state_ = AnimState::None;
    UINT_PTR anim_timer_id_ = 0;
    double anim_progress_ = 0.0;  // 0.0 to 1.0

    // Animation callbacks
    std::function<void(ToastWindow*)> anim_complete_callback_;

    // Methods
    void StartAnimation(AnimState state, std::function<void(ToastWindow*)> on_complete);
    void UpdateAnimation();
    void StopAnimation();
    double EaseOutCubic(double t);  // Easing function
```

#### 5.2: Implement Animation Timer

**File**: `src/host/toast_window.cpp`

Add timer handling to `WndProc`:
```cpp
case WM_TIMER:
    if (wParam == anim_timer_id_) {
        UpdateAnimation();
        return 0;
    }
    break;
```

Implement animation update:
```cpp
void ToastWindow::UpdateAnimation() {
    constexpr double ANIM_DURATION_MS = 250.0;
    constexpr double FRAME_MS = 16.0;  // ~60 FPS

    anim_progress_ += FRAME_MS / ANIM_DURATION_MS;

    if (anim_progress_ >= 1.0) {
        anim_progress_ = 1.0;
        StopAnimation();
        if (anim_complete_callback_) {
            anim_complete_callback_(this);
        }
        return;
    }

    double eased = EaseOutCubic(anim_progress_);

    // Update layered window attributes based on anim_state_
    switch (anim_state_) {
        case AnimState::FadingIn:
            SetLayeredWindowOpacity(static_cast<BYTE>(eased * 255));
            break;
        case AnimState::FadingOut:
            SetLayeredWindowOpacity(static_cast<BYTE>((1.0 - eased) * 255));
            break;
    }
}

double ToastWindow::EaseOutCubic(double t) {
    double f = t - 1.0;
    return f * f * f + 1.0;
}
```

#### 5.3: Make Window Layered

**File**: `src/host/toast_window.cpp`

Modify window creation to support alpha blending:
```cpp
// In CreateWindowExW call, add WS_EX_LAYERED
CreateWindowExW(
    WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED,
    // ... rest of params
);

// After window creation, enable alpha blending
SetLayeredWindowAttributes(hwnd_, 0, 255, LWA_ALPHA);
```

#### 5.4: Update ToastManager to Use Animations

**File**: `src/host/toast_manager.cpp`

**Replace line 51** (`toasts_.back()->Show()`):
```cpp
// Animate in
auto* toast_ptr = toasts_.back().get();
toast_ptr->StartAnimation(ToastWindow::AnimState::FadingIn, nullptr);
toast_ptr->Show();
```

**Replace line 66-68** (immediate removal):
```cpp
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
```

#### 5.5: Animated Repositioning

**File**: `src/host/toast_manager.cpp`

**Replace `RepositionAfterRemoval()` (line 119-122)**:
```cpp
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
            config_.width,
            toast->GetHeight(),
            static_cast<int>(i),
            preceding_heights
        );

        // Animate to target position
        AnimateToPosition(toast.get(), target_pos);
    }
}
```

Add position animation helper:
```cpp
void ToastManager::AnimateToPosition(ToastWindow* toast, Position target) {
    // Get current position
    RECT current;
    GetWindowRect(toast->GetHwnd(), &current);

    // If already at target, skip
    if (current.left == target.x && current.top == target.y) return;

    // Use SetWindowPos with SWP_NOOWNERZORDER for smooth movement
    // Win32 will interpolate automatically on modern Windows
    SetWindowPos(toast->GetHwnd(), nullptr,
        static_cast<int>(target.x),
        static_cast<int>(target.y),
        0, 0,
        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}
```

---

## Phase 6: Auto-Dismiss Timers

### Requirements

1. Toast automatically dismisses after `config.duration` milliseconds
2. Timer starts when toast finishes fading in
3. Timer cancels if user manually dismisses
4. Hovering pauses timer (optional enhancement)

### Technical Approach

#### 6.1: Add Timer State

**File**: `src/host/toast_window.h`

Add to `ToastWindow` class:
```cpp
private:
    UINT_PTR dismiss_timer_id_ = 0;
    bool auto_dismiss_enabled_ = true;

public:
    void StartAutoDismissTimer(int duration_ms);
    void StopAutoDismissTimer();
```

#### 6.2: Implement Auto-Dismiss Timer

**File**: `src/host/toast_window.cpp`

```cpp
void ToastWindow::StartAutoDismissTimer(int duration_ms) {
    if (!auto_dismiss_enabled_) return;

    StopAutoDismissTimer();  // Clear any existing timer

    dismiss_timer_id_ = SetTimer(hwnd_, 2, duration_ms, nullptr);
}

void ToastWindow::StopAutoDismissTimer() {
    if (dismiss_timer_id_) {
        KillTimer(hwnd_, dismiss_timer_id_);
        dismiss_timer_id_ = 0;
    }
}
```

Add to `WndProc`:
```cpp
case WM_TIMER:
    if (wParam == dismiss_timer_id_) {
        // Auto-dismiss triggered
        StopAutoDismissTimer();
        if (dismiss_callback_) {
            dismiss_callback_(this);
        }
        return 0;
    }
    // ... handle anim_timer_id_ as before
```

#### 6.3: Start Timer After Fade-In

**File**: `src/host/toast_manager.cpp`

**Update AnimateIn callback (line 51)**:
```cpp
auto* toast_ptr = toasts_.back().get();
toast_ptr->StartAnimation(ToastWindow::AnimState::FadingIn,
    [this, toast_ptr](ToastWindow* t) {
        // Fade-in complete, start auto-dismiss timer
        t->StartAutoDismissTimer(config_.duration);
    });
toast_ptr->Show();
```

#### 6.4: Cancel Timer on Manual Dismiss

**File**: `src/host/toast_window.cpp`

In click handler (wherever user dismisses):
```cpp
case WM_LBUTTONDOWN:
    StopAutoDismissTimer();
    if (dismiss_callback_) {
        dismiss_callback_(this);
    }
    return 0;
```

#### 6.5: (Optional) Pause on Hover

**File**: `src/host/toast_window.cpp`

```cpp
case WM_MOUSEMOVE:
    if (!mouse_tracked_) {
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
```

---

## Implementation Order

### Step 1: Phase 5.1-5.3 (Animation Infrastructure)
- Add animation state to `ToastWindow`
- Implement timer + easing function
- Make window layered for alpha blending

**Files**: `toast_window.h`, `toast_window.cpp`
**Complexity**: Medium
**Estimated time**: 2-3 hours

### Step 2: Phase 5.4 (Fade In/Out)
- Integrate animations into `ToastManager::ShowOnUiThread`
- Implement fade-out callback chain

**Files**: `toast_manager.cpp`
**Complexity**: Medium
**Estimated time**: 1-2 hours

### Step 3: Phase 5.5 (Animated Repositioning)
- Implement smooth collapse animations
- Test multi-toast scenarios

**Files**: `toast_manager.cpp`
**Complexity**: Low-Medium
**Estimated time**: 1 hour

### Step 4: Phase 6.1-6.4 (Auto-Dismiss Core)
- Add dismiss timer state
- Wire up timer start after fade-in
- Handle manual dismissal

**Files**: `toast_window.h`, `toast_window.cpp`, `toast_manager.cpp`
**Complexity**: Low
**Estimated time**: 1 hour

### Step 5: Phase 6.5 (Optional Hover Pause)
- Add mouse tracking
- Pause/resume on hover/leave

**Files**: `toast_window.cpp`
**Complexity**: Low
**Estimated time**: 30 min

### Step 6: Testing & Polish
- Test with rapid notifications
- Test queue overflow scenarios
- Verify animation smoothness
- Check edge cases (rapid dismiss, hover during fade)

**Complexity**: Medium
**Estimated time**: 1-2 hours

---

## Testing Checklist

- [ ] Single toast fades in smoothly
- [ ] Single toast auto-dismisses after `config.duration`
- [ ] Multiple toasts stack without overlap
- [ ] Removing middle toast causes smooth collapse
- [ ] Rapid notifications queue correctly during animation
- [ ] Max visible limit respected during animations
- [ ] Manual dismiss cancels auto-dismiss timer
- [ ] Hover pauses timer (if implemented)
- [ ] No flicker or visual artifacts
- [ ] No memory leaks (timers properly cleaned up)
- [ ] Works across all 4 corner positions

---

## Dependencies

**Required Win32 APIs**:
- `SetTimer` / `KillTimer` - For animations and auto-dismiss
- `SetLayeredWindowAttributes` - For opacity control
- `TrackMouseEvent` - For hover detection (optional)

**Required Math**:
- Easing functions (ease-out cubic recommended)

**No new external libraries needed** - all Win32 native APIs.

---

## Risks & Mitigations

| Risk | Mitigation |
|------|------------|
| Timer cleanup on early destruction | Call `StopAnimation()` and `StopAutoDismissTimer()` in `~ToastWindow()` |
| Animation jank on slow systems | Use `WM_TIMER` with conservative frame rate (16ms = 60fps max) |
| Layered window performance | Direct2D already renders to bitmap, minimal overhead |
| Queue starvation during long animations | `is_animating_` flag already prevents queued toasts from being blocked indefinitely |

---

## File Modification Summary

| File | Lines Changed (Est.) | Complexity |
|------|---------------------|------------|
| `toast_window.h` | +25 | Low |
| `toast_window.cpp` | +120 | Medium |
| `toast_manager.h` | +5 | Low |
| `toast_manager.cpp` | +80 | Medium |
| **Total** | **~230 lines** | **Medium** |

---

## Success Criteria

✅ All TODOs removed from `toast_manager.cpp`
✅ Toasts fade in over 250ms
✅ Toasts auto-dismiss after `config.duration`
✅ Smooth collapse animation when toast removed
✅ No visual glitches or flicker
✅ All test cases pass
✅ No memory leaks (verify with timer cleanup)

---

## Usage with Autopilot

To execute this plan with autopilot:

```bash
# Copy this file to the autopilot spec location
cp .omc/phase5-6-plan.md .omc/autopilot/spec.md

# Run autopilot
/oh-my-claudecode:autopilot
```

Or simply reference this document when running autopilot with the prompt:
```
Implement Phase 5 & 6 from the plan at .omc/phase5-6-plan.md
```

---

**Document Version**: 1.0
**Last Updated**: 2026-02-14
**Status**: Ready for implementation
