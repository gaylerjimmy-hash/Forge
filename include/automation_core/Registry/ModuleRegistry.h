#pragma once

#include "automation_core/Registry/Module.h"
#include "automation_core/Registry/HeartbeatResult.h"
#include "automation_core/Registry/CapabilityResult.h"
#include "automation_core/Registry/RegistrationResult.h"
#include "automation_core/Protocol/Message.h"

#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace automation_core {

class ModuleRegistry {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    RegistrationResult register_module(
        const Module& module,
        TimePoint now = Clock::now()
    );

    HeartbeatResult update_heartbeat(
        const std::string& connection_id,
        const HeartbeatMessage& heartbeat,
        TimePoint now = Clock::now()
    );

    CapabilityPublishResult publish_capabilities(
        const std::string& connection_id,
        const CapabilitiesMessage& document,
        TimePoint now = Clock::now()
    );

    std::vector<std::string> expire_heartbeats(
        std::chrono::milliseconds timeout,
        TimePoint now = Clock::now()
    );

    bool mark_offline_by_connection(const std::string& connection_id);
    bool quarantine(const std::string& module_id);

    [[nodiscard]] std::optional<Module> find(
        const std::string& module_id
    ) const;

    [[nodiscard]] std::optional<Module> find_by_connection(
        const std::string& connection_id
    ) const;

    [[nodiscard]] std::vector<Module> list() const;

private:
    std::unordered_map<std::string, Module> modules_;
};

} // namespace automation_core
