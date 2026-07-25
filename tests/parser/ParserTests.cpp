#include "automation_core/Protocol/Parser.h"

#include <string>
#include <variant>

namespace {

int failures = 0;

void expect(bool condition) {
    if (!condition) {
        ++failures;
    }
}

automation_core::Frame frame(const std::string& payload) {
    return {"connection-1", payload};
}

} // namespace

int run_parser_tests() {
    using namespace automation_core;

    Parser parser;

    {
        const auto result = parser.parse(frame(
            "HELLO\n"
            "MSG=00000001\n"
            "TYPE=Scale\n"
            "ID=scale01\n"
            "FW=1.0.0\n"
            "PROTO=1\n"
            "SESSION=81A9C5D2\n"
            "END\n"
        ));

        expect(result.ok());
        expect(result.error == ParseError::None);

        if (result.message.has_value()) {
            const auto* hello =
                std::get_if<HelloMessage>(&result.message->payload);

            expect(hello != nullptr);

            if (hello != nullptr) {
                expect(hello->message_id == "00000001");
                expect(hello->module_type == "Scale");
                expect(hello->module_id == "scale01");
                expect(hello->firmware_version == "1.0.0");
                expect(hello->protocol_version == "1");
                expect(hello->session_id == "81A9C5D2");
            }
        }
    }

    {
        const auto result = parser.parse(frame(""));
        expect(!result.ok());
        expect(result.error == ParseError::EmptyFrame);
    }

    {
        const auto result = parser.parse(frame(
            "HEARTBEAT\n"
            "MSG=42\n"
            "ID=scale01\n"
            "SESSION=81A9C5D2\n"
            "SEQ=1042\n"
            "UPTIME_MS=381500\n"
            "STATE=ready\n"
            "FAULTS=0\n"
            "END\n"
        ));
        expect(result.ok());
        const auto* heartbeat = result.message
            ? std::get_if<HeartbeatMessage>(&result.message->payload)
            : nullptr;
        expect(heartbeat != nullptr);
        if (heartbeat != nullptr) {
            expect(heartbeat->sequence == 1042);
            expect(heartbeat->uptime_ms == 381500);
            expect(heartbeat->state == ModuleState::Ready);
            expect(heartbeat->active_fault_count == 0);
        }
    }

    {
        const auto result = parser.parse(frame(
            "HEARTBEAT\n"
            "MSG=42\n"
            "ID=scale01\n"
            "SESSION=81A9C5D2\n"
            "SEQ=not-a-number\n"
            "UPTIME_MS=381500\n"
            "STATE=ready\n"
            "FAULTS=0\n"
            "END\n"
        ));
        expect(!result.ok());
        expect(result.error == ParseError::InvalidValue);
    }

    {
        const auto result = parser.parse(frame(
            "UNKNOWN\nMSG=1\nEND\n"
        ));
        expect(!result.ok());
        expect(result.error == ParseError::UnknownMessageType);
    }

    {
        const auto result = parser.parse(frame(
            "HELLO\n"
            "MSG=1\n"
            "TYPE=Scale\n"
            "ID=scale01\n"
            "FW=1.0.0\n"
            "PROTO=1\n"
            "PROTO=1\n"
            "SESSION=abc\n"
            "END\n"
        ));
        expect(!result.ok());
        expect(result.error == ParseError::DuplicateField);
    }

    {
        const auto result = parser.parse(frame(
            "HELLO\n"
            "MSG=1\n"
            "TYPE=Scale\n"
            "ID=scale01\n"
            "FW=1.0.0\n"
            "PROTO=1\n"
            "SESSION=abc\n"
            "COLOR=blue\n"
            "END\n"
        ));
        expect(!result.ok());
        expect(result.error == ParseError::UnknownField);
    }

    {
        const auto result = parser.parse(frame(
            "HELLO\n"
            "MSG=1\n"
            "TYPE=Scale\n"
            "ID=scale01\n"
            "FW=1.0.0\n"
            "PROTO=1\n"
            "SESSION\n"
            "END\n"
        ));
        expect(!result.ok());
        expect(result.error == ParseError::MalformedField);
    }

    {
        const auto result = parser.parse(frame(
            "HELLO\n"
            "MSG=1\n"
            "TYPE=Scale\n"
            "ID=scale01\n"
            "FW=1.0.0\n"
            "PROTO=1\n"
            "END\n"
        ));
        expect(!result.ok());
        expect(result.error == ParseError::MissingField);
    }

    {
        const auto result = parser.parse(frame(
            "HELLO\n"
            "MSG=1\n"
            "TYPE=Scale\n"
            "ID=scale01\n"
            "FW=1.0.0\n"
            "PROTO=1\n"
            "SESSION=abc\n"
        ));
        expect(!result.ok());
        expect(result.error == ParseError::MissingTerminator);
    }

    {
        const auto result = parser.parse(frame(
            "HELLO\n"
            "MSG=1\n"
            "TYPE=Scale\n"
            "ID=scale01\n"
            "FW=1.0.0\n"
            "PROTO=1\n"
            "SESSION=abc\n"
            "END\n"
            "GARBAGE\n"
        ));
        expect(!result.ok());
        expect(result.error == ParseError::TrailingData);
    }

    return failures;
}
