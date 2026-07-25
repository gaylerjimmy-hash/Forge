#pragma once

#include "automation_core/ModuleState.h"

#include <chrono>
#include <cstdint>
#include <string>

namespace automation_core {

enum class ModuleStatus {
    Active,
    Offline,
    Quarantined
};

struct Module {
    std::string module_id;
    std::string module_type;
    std::string firmware_version;
    std::string protocol_version;
    std::string session_id;
    std::string connection_id;
    ModuleStatus status{ModuleStatus::Active};
    ModuleState state{ModuleState::Booting};
    bool has_heartbeat{false};
    std::uint32_t last_heartbeat_sequence{0};
    std::uint64_t uptime_ms{0};
    std::uint32_t active_fault_count{0};
    std::chrono::steady_clock::time_point last_heartbeat_at{};
    std::chrono::steady_clock::time_point last_transition_at{};
    std::string last_transition_reason{"registered"};
};

} // namespace automation_core
