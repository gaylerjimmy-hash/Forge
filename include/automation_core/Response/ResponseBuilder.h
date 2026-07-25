#pragma once

#include "automation_core/Protocol/Message.h"

#include <string>

namespace automation_core {

class ResponseBuilder {
public:
    [[nodiscard]] Message hello_ack(
        const std::string& message_id,
        const std::string& connection_id
    ) const;

    [[nodiscard]] Message error(
        const std::string& message_id,
        const std::string& code,
        const std::string& detail
    ) const;

    [[nodiscard]] Message capabilities_ack(
        const std::string& message_id,
        const std::string& module_id,
        std::uint32_t revision
    ) const;
};

} // namespace automation_core
