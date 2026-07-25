#include "automation_core/Router/MessageRouter.h"

#include "automation_core/Registry/Module.h"
#include "automation_core/Registry/RegistrationResult.h"

#include <variant>

namespace automation_core {

MessageRouter::MessageRouter(
    Validator& validator,
    ModuleRegistry& registry,
    ResponseBuilder& responses
)
    : validator_(validator),
      registry_(registry),
      responses_(responses) {}

RouteResult MessageRouter::route(
    const std::string& connection_id,
    const Message& message,
    const ModuleRegistry::TimePoint now
) {
    const auto* hello = std::get_if<HelloMessage>(&message.payload);
    const auto* heartbeat =
        std::get_if<HeartbeatMessage>(&message.payload);

    if (hello == nullptr && heartbeat == nullptr) {
        return {
            responses_.error(
                "",
                "UNSUPPORTED",
                "Only HELLO and HEARTBEAT messages are supported"
            ),
            "Unsupported message type"
        };
    }

    if (connection_id.empty()) {
        const std::string message_id = hello != nullptr
            ? hello->message_id
            : heartbeat->message_id;

        return {
            responses_.error(
                message_id,
                "CONNECTION",
                "Connection ID is required"
            ),
            "Rejected message without connection ID"
        };
    }

    const ValidationResult validation = validator_.validate(message);

    if (!validation.valid()) {
        const std::string message_id = hello != nullptr
            ? hello->message_id
            : heartbeat->message_id;

        return {
            responses_.error(
                message_id,
                validation.error_code,
                validation.detail
            ),
            validation.detail
        };
    }

    if (heartbeat != nullptr) {
        const HeartbeatResult result =
            registry_.update_heartbeat(connection_id, *heartbeat, now);

        if (result.accepted()) {
            return {
                std::nullopt,
                result.detail
            };
        }

        return {
            responses_.error(
                heartbeat->message_id,
                result.error_code,
                result.detail
            ),
            result.detail
        };
    }

    const Module module{
        hello->module_id,
        hello->module_type,
        hello->firmware_version,
        hello->protocol_version,
        hello->session_id,
        connection_id,
        ModuleStatus::Active
    };

    const RegistrationResult registration =
        registry_.register_module(module, now);

    switch (registration.status) {
        case RegistrationStatus::Added:
        case RegistrationStatus::Reconnected:
            return {
                responses_.hello_ack(
                    hello->message_id,
                    connection_id
                ),
                registration.detail
            };

        case RegistrationStatus::DuplicateIdentity:
            return {
                responses_.error(
                    hello->message_id,
                    "DUPLICATE_IDENTITY",
                    registration.detail
                ),
                registration.detail
            };

        case RegistrationStatus::Quarantined:
            return {
                responses_.error(
                    hello->message_id,
                    "QUARANTINED",
                    registration.detail
                ),
                registration.detail
            };

        case RegistrationStatus::Rejected:
            return {
                responses_.error(
                    hello->message_id,
                    "REGISTRATION_REJECTED",
                    registration.detail
                ),
                registration.detail
            };
    }

    return {
        responses_.error(
            hello->message_id,
            "INTERNAL",
            "Unhandled registration status"
        ),
        "Unhandled registration status"
    };
}

} // namespace automation_core
