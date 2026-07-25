#include "automation_core/Protocol/MessageSerializer.h"

#include <type_traits>
#include <variant>

namespace automation_core {

std::string MessageSerializer::serialize(
    const Message& message
) const
{
    return std::visit(
        [](const auto& payload) -> std::string {
            using Payload = std::decay_t<decltype(payload)>;

            if constexpr (std::is_same_v<Payload, HelloMessage>) {
                return
                    "HELLO\n"
                    "MSG=" + payload.message_id + "\n"
                    "TYPE=" + payload.module_type + "\n"
                    "ID=" + payload.module_id + "\n"
                    "FW=" + payload.firmware_version + "\n"
                    "PROTO=" + payload.protocol_version + "\n"
                    "SESSION=" + payload.session_id + "\n"
                    "END\n";
            }
            else if constexpr (
                std::is_same_v<Payload, HelloAckMessage>
            ) {
                return
                    "HELLO_ACK\n"
                    "MSG=" + payload.message_id + "\n"
                    "CONNECTION=" + payload.connection_id + "\n"
                    "STATUS=" + payload.status + "\n"
                    "END\n";
            }
            else if constexpr (
                std::is_same_v<Payload, HeartbeatMessage>
            ) {
                return
                    "HEARTBEAT\n"
                    "MSG=" + payload.message_id + "\n"
                    "ID=" + payload.module_id + "\n"
                    "SESSION=" + payload.session_id + "\n"
                    "SEQ=" + std::to_string(payload.sequence) + "\n"
                    "UPTIME_MS=" + std::to_string(payload.uptime_ms) + "\n"
                    "STATE=" + to_string(payload.state) + "\n"
                    "FAULTS=" +
                        std::to_string(payload.active_fault_count) + "\n"
                    "END\n";
            }
            else if constexpr (
                std::is_same_v<Payload, ErrorMessage>
            ) {
                return
                    "ERROR\n"
                    "MSG=" + payload.message_id + "\n"
                    "CODE=" + payload.code + "\n"
                    "DETAIL=" + payload.detail + "\n"
                    "END\n";
            }
            else {
                return std::string{};
            }
        },
        message.payload
    );
}

} // namespace automation_core
