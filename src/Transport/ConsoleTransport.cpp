#include "automation_core/Transport/ConsoleTransport.h"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace automation_core {

ConsoleTransport::ConsoleTransport()
    : ConsoleTransport(std::cin, std::cout) {}

ConsoleTransport::ConsoleTransport(
    std::istream& input,
    std::ostream& output,
    std::string connection_id
)
    : input_(input),
      output_(output),
      connection_id_(std::move(connection_id)) {
    if (connection_id_.empty()) {
        throw std::invalid_argument(
            "console connection ID must not be empty"
        );
    }
}

std::optional<TransportPacket> ConsoleTransport::receive() {
    const auto started_at = std::chrono::steady_clock::now();
    std::string payload;
    std::string line;

    while (std::getline(input_, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        payload += line;
        payload += '\n';

        if (line == "END") {
            break;
        }
    }

    if (payload.empty()) {
        return std::nullopt;
    }

    const auto assembly_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started_at
        );

    return TransportPacket{
        connection_id_,
        std::move(payload),
        assembly_time
    };
}

bool ConsoleTransport::send(
    const std::string& connection_id,
    const std::string& payload
) {
    if (connection_id != connection_id_) {
        return false;
    }

    output_ << payload;
    output_.flush();
    return output_.good();
}

} // namespace automation_core
