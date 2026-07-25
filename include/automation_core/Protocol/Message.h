#pragma once

#include "automation_core/ModuleState.h"
#include "automation_core/Protocol/Capability.h"
#include "automation_core/Protocol/Measurement.h"

#include <cstdint>
#include <string>
#include <variant>

namespace automation_core {

struct HelloMessage {
    std::string message_id;
    std::string module_type;
    std::string module_id;
    std::string firmware_version;
    std::string protocol_version;
    std::string session_id;
};

struct HelloAckMessage {
    std::string message_id;
    std::string connection_id;
    std::string status;
};

struct HeartbeatMessage {
    std::string message_id;
    std::string module_id;
    std::string session_id;
    std::uint32_t sequence{0};
    std::uint64_t uptime_ms{0};
    ModuleState state{ModuleState::Booting};
    std::uint32_t active_fault_count{0};
};

struct ErrorMessage {
    std::string message_id;
    std::string code;
    std::string detail;
};

using MessagePayload = std::variant<
    HelloMessage,
    HelloAckMessage,
    HeartbeatMessage,
    MeasurementMessage,
    CapabilitiesMessage,
    CapabilitiesAckMessage,
    ErrorMessage
>;

struct Message {
    MessagePayload payload;
};

} // namespace automation_core
