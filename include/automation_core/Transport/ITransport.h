#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace automation_core {

struct TransportPacket {
    std::string connection_id;
    std::string payload;
    std::chrono::milliseconds assembly_time{0};
};

class ITransport {
public:
    virtual ~ITransport() = default;

    [[nodiscard]] virtual std::optional<TransportPacket> receive() = 0;
    virtual bool send(
        const std::string& connection_id,
        const std::string& payload
    ) = 0;

    // One-shot physical lifecycle break notification.  The default keeps this
    // narrow addition source-compatible with transports that have no lifecycle.
    [[nodiscard]] virtual std::optional<std::string> consume_lifecycle_break() {
        return std::nullopt;
    }
};

} // namespace automation_core
