#include "automation_core/Response/ResponseBuilder.h"

namespace automation_core {

Message ResponseBuilder::hello_ack(
    const std::string& message_id,
    const std::string& connection_id
) const
{
    return Message{
        HelloAckMessage{
            message_id,
            connection_id,
            "accepted"
        }
    };
}

Message ResponseBuilder::error(
    const std::string& message_id,
    const std::string& code,
    const std::string& detail
) const
{
    return Message{
        ErrorMessage{
            message_id,
            code,
            detail
        }
    };
}

Message ResponseBuilder::capabilities_ack(
    const std::string& message_id,
    const std::string& module_id,
    const std::uint32_t revision
) const
{
    return Message{
        CapabilitiesAckMessage{
            message_id,
            module_id,
            revision,
            "accepted"
        }
    };
}


} // namespace automation_core

