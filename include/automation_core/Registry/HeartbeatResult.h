#pragma once

#include <string>

namespace automation_core {

enum class HeartbeatStatus {
    Accepted,
    Duplicate,
    UnknownModule,
    ConnectionMismatch,
    SessionMismatch,
    OutOfOrder,
    UptimeRegression,
    Offline,
    Quarantined
};

struct HeartbeatResult {
    HeartbeatStatus status{HeartbeatStatus::UnknownModule};
    std::string error_code;
    std::string detail;

    [[nodiscard]] bool accepted() const noexcept {
        return status == HeartbeatStatus::Accepted ||
            status == HeartbeatStatus::Duplicate;
    }

    [[nodiscard]] bool changed() const noexcept {
        return status == HeartbeatStatus::Accepted;
    }
};

} // namespace automation_core
