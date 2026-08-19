#pragma once

#include "automation_core/Protocol/Message.h"
#include "automation_core/Protocol/ValidationResult.h"

#include <string>
#include <vector>

namespace automation_core {

class Validator {
public:
    explicit Validator(std::string supported_protocol_version = "1");

    [[nodiscard]] ValidationResult validate(
        const Message& message
    ) const;

    [[nodiscard]] ValidationResult validate_command(
        const CommandMessage& command,
        const std::vector<Capability>& accepted_capabilities,
        std::uint32_t accepted_revision
    ) const;

private:
    std::string supported_protocol_version_;
};

} // namespace automation_core
