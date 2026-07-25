#pragma once

#include <optional>
#include <string>

namespace automation_core {

enum class ModuleState {
    Booting,
    Initializing,
    Ready,
    Busy,
    Degraded,
    Faulted,
    Updating
};

[[nodiscard]] const char* to_string(ModuleState state) noexcept;

[[nodiscard]] std::optional<ModuleState> parse_module_state(
    const std::string& value
);

} // namespace automation_core
