#include "automation_core/Protocol/Message.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <variant>

using namespace automation_core;

int run_heartbeat_message_tests() {
    int failures = 0;

    const Message message{
        HeartbeatMessage{
            "msg-42",
            "scale-01",
            "81A9C5D2",
            std::uint32_t{1042},
            std::uint64_t{381500},
            ModuleState::Ready,
            std::uint32_t{0}
        }
    };

    const auto* heartbeat =
        std::get_if<HeartbeatMessage>(&message.payload);

    if (heartbeat == nullptr) {
        std::cerr << "HeartbeatMessage: variant did not retain heartbeat\n";
        return 1;
    }

    if (heartbeat->message_id != "msg-42") {
        ++failures;
    }
    if (heartbeat->module_id != "scale-01") {
        ++failures;
    }
    if (heartbeat->session_id != "81A9C5D2") {
        ++failures;
    }
    if (heartbeat->sequence != 1042) {
        ++failures;
    }
    if (heartbeat->uptime_ms != 381500) {
        ++failures;
    }
    if (heartbeat->state != ModuleState::Ready) {
        ++failures;
    }
    if (heartbeat->active_fault_count != 0) {
        ++failures;
    }

    return failures;
}
