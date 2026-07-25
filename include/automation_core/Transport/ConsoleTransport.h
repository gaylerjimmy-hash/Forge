#pragma once

#include "automation_core/Transport/ITransport.h"

#include <iosfwd>
#include <string>

namespace automation_core {

class ConsoleTransport final : public ITransport {
public:
    ConsoleTransport();

    ConsoleTransport(
        std::istream& input,
        std::ostream& output,
        std::string connection_id = "console"
    );

    [[nodiscard]] std::optional<TransportPacket> receive() override;

    bool send(
        const std::string& connection_id,
        const std::string& payload
    ) override;

private:
    std::istream& input_;
    std::ostream& output_;
    std::string connection_id_;
};

} // namespace automation_core
