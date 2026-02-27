#pragma once

#include <string>

#include "payload.h"

namespace notiman {

constexpr const wchar_t* kHostWindowClassName = L"NotimanHostClass";

// Sends a notification payload to the running host via WM_COPYDATA.
// Returns true on success. If provided, error_message receives a short failure reason.
bool send_payload_to_host(const NotificationPayload& payload, std::string* error_message = nullptr);

}  // namespace notiman
