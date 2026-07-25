#include "automation_core/ModuleState.h"

namespace automation_core {

const char* to_string(const ModuleState state) noexcept {
    switch (state) {
        case ModuleState::Booting:
            return "booting";
        case ModuleState::Initializing:
            return "initializing";
        case ModuleState::Ready:
            return "ready";
        case ModuleState::Busy:
            return "busy";
        case ModuleState::Degraded:
            return "degraded";
        case ModuleState::Faulted:
            return "faulted";
        case ModuleState::Updating:
            return "updating";
    }

    return "booting";
}

std::optional<ModuleState> parse_module_state(
    const std::string& value
) {
    if (value == "booting") {
        return ModuleState::Booting;
    }
    if (value == "initializing") {
        return ModuleState::Initializing;
    }
    if (value == "ready") {
        return ModuleState::Ready;
    }
    if (value == "busy") {
        return ModuleState::Busy;
    }
    if (value == "degraded") {
        return ModuleState::Degraded;
    }
    if (value == "faulted") {
        return ModuleState::Faulted;
    }
    if (value == "updating") {
        return ModuleState::Updating;
    }

    return std::nullopt;
}

} // namespace automation_core
