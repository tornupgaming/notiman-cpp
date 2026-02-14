# Notification Opacity Flicker - Fix Specification

## Problem Statement

Two opacity bugs cause visible flash when notifications appear:
1. **Initial flash**: Window appears at config opacity (~85%), drops to 0, then animates up
2. **Target overshoot**: Animation goes to 100% opacity instead of respecting config opacity

## Root Cause Analysis

### Bug 1: Initial Opacity Flash

Timeline:
1. Constructor sets `SetLayeredWindowAttributes(hwnd_, 0, config_.opacity * 255, LWA_ALPHA)` (~217 for 0.85)
2. `StartAnimation(FadingIn)` sets `anim_progress_ = 0.0` but **doesn't set window opacity to 0**
3. `Show()` makes window visible at constructor opacity (217)
4. First timer tick (16ms later) sets opacity to ~15 (near 0)
5. Subsequent ticks animate from 15 to 255

**Result**: Flash of toast at 85% opacity → snap to near-zero → fade up

### Bug 2: Fade Target Overshoot

Code at `toast_window.cpp:476`:
```cpp
SetLayeredWindowAttributes(hwnd_, 0, static_cast<BYTE>(eased * 255), LWA_ALPHA);
```

When `eased = 1.0`, opacity = 255 (fully opaque). Should be `config_.opacity * 255` (~217).

Same issue in fade-out at line 479 - starts from 255 instead of `config_.opacity * 255`.

## Technical Specification

### Fix 1: Set Opacity to 0 Before Showing

**File**: `src/host/toast_window.cpp`
**Location**: Line 448, inside `StartAnimation()` after `anim_progress_ = 0.0`

Add when `state == FadingIn`:
```cpp
if (state == AnimState::FadingIn) {
    SetLayeredWindowAttributes(hwnd_, 0, 0, LWA_ALPHA);
}
```

**Rationale**: Animation owns its start state. Window is invisible when `Show()` is called.

### Fix 2: Use config_.opacity as Animation Target

**File**: `src/host/toast_window.cpp`
**Location**: Lines 474-483 in `UpdateAnimation()`

Replace hardcoded `255` with `config_.opacity * 255`:
```cpp
BYTE target_opacity = static_cast<BYTE>(config_.opacity * 255);
switch (anim_state_) {
    case AnimState::FadingIn:
        SetLayeredWindowAttributes(hwnd_, 0, static_cast<BYTE>(eased * target_opacity), LWA_ALPHA);
        break;
    case AnimState::FadingOut:
        SetLayeredWindowAttributes(hwnd_, 0, static_cast<BYTE>((1.0 - eased) * target_opacity), LWA_ALPHA);
        break;
    case AnimState::None:
        break;
}
```

**Rationale**: Animation respects user-configured opacity.

### Files to Modify

| File | Lines | Change |
|------|-------|--------|
| `src/host/toast_window.cpp` | 448 | Add opacity=0 when FadingIn |
| `src/host/toast_window.cpp` | 474-483 | Use `config_.opacity * 255` instead of `255` |

## Testing Strategy

### Manual Integration Test
1. Set `opacity = 0.5` in config (makes overshoot obvious)
2. Trigger notification
3. **PASS**: Toast fades smoothly from invisible to 50% opacity, no flash
4. **FAIL**: Toast flashes or reaches full opacity

### Edge Cases

| Case | Expected Behavior |
|------|-------------------|
| `opacity = 1.0` | Fade to 255, no regression |
| `opacity = 0.0` | Fade to 0, toast invisible |
| `opacity = 0.5` | Fade to 127 (best test for overshoot) |
| Rapid show/dismiss | No stale state, each starts at 0 |
| FadingOut | Start at `config_.opacity * 255`, reach 0 |

## Implementation Files

- `C:\Users\terro\projects\notiman-cpp\src\host\toast_window.cpp`

## Acceptance Criteria

1. No visible flash when notification appears
2. Fade-in goes from 0 to `config_.opacity * 255`, not 255
3. Fade-out starts from `config_.opacity * 255`, not 255
4. Smooth animation at all configured opacity values
