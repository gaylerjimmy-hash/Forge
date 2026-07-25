#pragma once

#include "automation_core/ModuleState.h"
#include "automation_core/Protocol/Capability.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

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
    bool has_capabilities{false};
    bool capabilities_available{false};
    std::uint32_t capability_revision{0};
    std::vector<Capability> capabilities;
    std::chrono::steady_clock::time_point capabilities_published_at{};
};

} // namespace automation_core
