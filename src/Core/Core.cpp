#include "automation_core/Core/Core.h"

#include "automation_core/Frame/FrameError.h"
#include "automation_core/Protocol/ParseResult.h"

#include <ostream>
#include <string>
#include <utility>
#include <variant>

namespace automation_core {
namespace {

std::string frame_error_code(const FrameError error) {
    switch (error) {
        case FrameError::Empty:
            return "FRAME_EMPTY";
        case FrameError::TooLarge:
            return "FRAME_TOO_LARGE";
        case FrameError::TimedOut:
            return "FRAME_TIMEOUT";
        case FrameError::MissingTerminator:
            return "FRAME_TERMINATOR";
        case FrameError::None:
            return "FRAME_ERROR";
    }

    return "FRAME_ERROR";
}

std::string parse_error_code(const ParseError error) {
    switch (error) {
        case ParseError::EmptyFrame:
            return "EMPTY_FRAME";
        case ParseError::UnknownMessageType:
            return "UNKNOWN_MESSAGE_TYPE";
        case ParseError::MalformedField:
            return "MALFORMED_FIELD";
        case ParseError::UnknownField:
            return "UNKNOWN_FIELD";
        case ParseError::DuplicateField:
            return "DUPLICATE_FIELD";
        case ParseError::MissingField:
            return "MISSING_FIELD";
        case ParseError::EmptyValue:
            return "EMPTY_VALUE";
        case ParseError::InvalidValue:
            return "INVALID_VALUE";
        case ParseError::MissingTerminator:
            return "MISSING_TERMINATOR";
        case ParseError::TrailingData:
            return "TRAILING_DATA";
        case ParseError::None:
            return "PARSE_ERROR";
    }

    return "PARSE_ERROR";
}

} // namespace

Core::Core(ITransport& transport)
    : Core(
        transport,
        nullptr,
        [] { return Clock::now(); },
        std::chrono::milliseconds{3000}
    ) {}

Core::Core(ITransport& transport, std::ostream& trace_output)
    : Core(
        transport,
        &trace_output,
        [] { return Clock::now(); },
        std::chrono::milliseconds{3000}
    ) {}

Core::Core(ITransport& transport, NowFunction now)
    : Core(
        transport,
        nullptr,
        std::move(now),
        std::chrono::milliseconds{3000}
    ) {}

Core::Core(
    ITransport& transport,
    NowFunction now,
    const std::chrono::milliseconds heartbeat_timeout
)
    : Core(
        transport,
        nullptr,
        std::move(now),
        heartbeat_timeout
    ) {}

Core::Core(
    ITransport& transport,
    std::ostream& trace_output,
    NowFunction now,
    const std::chrono::milliseconds heartbeat_timeout
)
    : Core(
        transport,
        &trace_output,
        std::move(now),
        heartbeat_timeout
    ) {}

Core::Core(
    ITransport& transport,
    std::ostream* trace_output,
    NowFunction now,
    const std::chrono::milliseconds heartbeat_timeout
)
    : transport_(transport),
      trace_output_(trace_output),
      now_(std::move(now)),
      heartbeat_timeout_(heartbeat_timeout),
      connections_(),
      frames_(),
      parser_(),
      serializer_(),
      validator_("1"),
      registry_(),
      responses_(),
      router_(validator_, registry_, responses_),
      transport_connections_() {}

void Core::trace(
    const std::string& event,
    const std::string& detail
) const {
    if (trace_output_ == nullptr) {
        return;
    }

    *trace_output_ << "TRACE event=" << event;

    if (!detail.empty()) {
        *trace_output_ << " detail=\"" << detail << '"';
    }

    *trace_output_ << '\n';
}

bool Core::poll_once() {
    const TimePoint now = now_();
    const auto expired_modules =
        registry_.expire_heartbeats(heartbeat_timeout_, now);

    for (const auto& module_id : expired_modules) {
        trace("module_offline", module_id + ": heartbeat timeout");
    }

    const auto packet = transport_.receive();

    if (!packet) {
        trace("poll_idle", "transport returned no packet");
        return false;
    }

    if (packet->connection_id.empty()) {
        trace("packet_rejected", "transport connection ID is empty");
        return true;
    }

    auto connection = transport_connections_.find(packet->connection_id);

    if (connection == transport_connections_.end()) {
        const std::string connection_id =
            connections_.open(packet->connection_id);

        connection = transport_connections_
            .emplace(packet->connection_id, connection_id)
            .first;

        trace(
            "connection_opened",
            packet->connection_id + " -> " + connection_id
        );
    } else {
        const auto existing = connections_.find(connection->second);

        if (existing &&
            existing->state == ConnectionState::Quarantined) {
            trace(
                "connection_rejected",
                packet->connection_id + " is quarantined"
            );

            const Message error = responses_.error(
                "",
                "CONNECTION_QUARANTINED",
                "Transport connection is quarantined"
            );

            const bool sent = transport_.send(
                packet->connection_id,
                serializer_.serialize(error)
            );

            trace(
                sent ? "response_sent" : "response_send_failed",
                packet->connection_id
            );
            return true;
        }

        if (connections_.touch(connection->second)) {
            trace(
                "connection_activity",
                packet->connection_id + " -> " + connection->second
            );
        } else {
            const std::string connection_id =
                connections_.open(packet->connection_id);

            connection->second = connection_id;

            trace(
                "connection_reopened",
                packet->connection_id + " -> " + connection_id
            );
        }
    }

    const std::string& connection_id = connection->second;
    const FrameResult frame_result =
        frames_.assemble(
            connection_id,
            packet->payload,
            packet->assembly_time
        );

    if (!frame_result.ok()) {
        trace(
            "frame_rejected",
            frame_error_code(frame_result.error) +
                ": " + frame_result.detail
        );

        const Message error = responses_.error(
            "",
            frame_error_code(frame_result.error),
            frame_result.detail
        );

        static_cast<void>(transport_.send(
            packet->connection_id,
            serializer_.serialize(error)
        ));
        return true;
    }

    trace("frame_accepted", connection_id);

    const ParseResult parse_result = parser_.parse(*frame_result.frame);

    if (!parse_result.ok()) {
        trace(
            "parse_rejected",
            parse_error_code(parse_result.error) +
                ": " + parse_result.detail
        );

        const Message error = responses_.error(
            "",
            parse_error_code(parse_result.error),
            parse_result.detail
        );

        static_cast<void>(transport_.send(
            packet->connection_id,
            serializer_.serialize(error)
        ));
        return true;
    }

    trace("parse_accepted", connection_id);

    const RouteResult route_result =
        router_.route(
            connection_id,
            *parse_result.message,
            now
        );

    const auto* heartbeat =
        std::get_if<HeartbeatMessage>(&parse_result.message->payload);

    if (!route_result.has_response()) {
        if (heartbeat != nullptr) {
            const std::string detail =
                heartbeat->module_id +
                " state=" + to_string(heartbeat->state) +
                " seq=" + std::to_string(heartbeat->sequence) +
                " uptime_ms=" + std::to_string(heartbeat->uptime_ms) +
                " faults=" +
                    std::to_string(heartbeat->active_fault_count);

            trace(
                route_result.detail == "Duplicate heartbeat ignored"
                    ? "heartbeat_duplicate"
                    : "heartbeat_accepted",
                detail
            );
        }

        trace("route_completed", route_result.detail + "; no response");
        return true;
    }

    trace("route_completed", route_result.detail);

    const auto* error =
        std::get_if<ErrorMessage>(&route_result.response->payload);

    if (error != nullptr &&
        (error->code == "DUPLICATE_IDENTITY" ||
         error->code == "QUARANTINED")) {
        static_cast<void>(connections_.quarantine(connection_id));
    }

    if (error != nullptr && heartbeat != nullptr) {
        trace(
            error->code == "SESSION_MISMATCH" ||
                error->code == "UPTIME_REGRESSION"
                ? "module_reboot_suspected"
                : "heartbeat_rejected",
            heartbeat->module_id + ": " + error->code
        );
    }

    const auto* acknowledgement =
        std::get_if<HelloAckMessage>(&route_result.response->payload);

    if (acknowledgement != nullptr) {
        if (route_result.detail == "Offline module registered on a new connection") {
            trace("module_recovered", acknowledgement->connection_id);
        } else if (route_result.detail == "Existing session rebound to connection") {
            trace("module_reconnected", acknowledgement->connection_id);
        }
    }

    const bool sent = transport_.send(
        packet->connection_id,
        serializer_.serialize(*route_result.response)
    );

    trace(
        sent ? "response_sent" : "response_send_failed",
        packet->connection_id
    );

    return true;
}

} // namespace automation_core
