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
        const auto result=parser.parse(frame("MEASUREMENT\nMSG=70\nID=scale01\nSESSION=81A9C5D2\nCAP=weight\nSEQ=2081\nVALUE=42.75\nUNIT=lb\nQUALITY=good\nUNCERTAINTY=0.1\nEND\n"));
        const auto* measurement=result.message?std::get_if<MeasurementMessage>(&result.message->payload):nullptr;
        expect(result.ok()&&measurement&&measurement->sequence==2081&&measurement->uncertainty);
    }

    {
        const auto result=parser.parse(frame("MEASUREMENT\nMSG=70\nID=scale01\nSESSION=s\nCAP=weight\nSEQ=1\nVALUE=1\nQUALITY=stale\nEND\n"));
        expect(!result.ok()); expect(result.error==ParseError::InvalidValue);
    }

    {
        const auto result=parser.parse(frame("CAPABILITIES\nMSG=50\nID=scale01\nSESSION=81A9C5D2\nREV=1\nCOUNT=1\nITEM.0.NAME=weight\nITEM.0.TYPE=measurement\nITEM.0.DATA_TYPE=float\nITEM.0.ACCESS=read\nITEM.0.UNIT=lb\nEND\n"));
        expect(result.ok());
        const auto* caps=result.message?std::get_if<CapabilitiesMessage>(&result.message->payload):nullptr;
        expect(caps&&caps->items.size()==1&&caps->items[0].name=="weight");
    }

    {
        const auto result=parser.parse(frame("CAPABILITIES\nMSG=50\nID=scale01\nSESSION=81A9C5D2\nREV=1\nCOUNT=2\nITEM.0.NAME=weight\nITEM.0.TYPE=measurement\nITEM.0.DATA_TYPE=float\nITEM.0.ACCESS=read\nEND\n"));
        expect(!result.ok());expect(result.error==ParseError::MissingField);
    }

    {
        const auto result=parser.parse(frame("CAPABILITIES\nMSG=50\nID=scale01\nSESSION=81A9C5D2\nREV=1\nCOUNT=1\nITEM.0.NAME=weight\nITEM.0.TYPE=measurement\nITEM.0.DATA_TYPE=float\nITEM.0.ACCESS=read\nITEM.1.NAME=extra\nEND\n"));
        expect(!result.ok());expect(result.error==ParseError::UnknownField);
    }

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

    {
        const auto result = parser.parse(frame("COMMAND\nMSG=cmd-1\nTX=tx-1\nID=scale01\nSESSION=81A9C5D2\nCAP=tare\nCAP_REV=2\nPAYLOAD=now\nEND\n"));
        const auto* command = result.message ? std::get_if<CommandMessage>(&result.message->payload) : nullptr;
        expect(result.ok() && command && command->transaction_id == "tx-1" && command->capability_revision == 2);
    }
    {
        const auto result = parser.parse(frame("COMMAND\nMSG=cmd-1\nTX=tx-1\nID=scale01\nSESSION=81A9C5D2\nCAP=tare\nCAP_REV=x\nPAYLOAD=now\nEND\n"));
        expect(!result.ok() && result.error == ParseError::InvalidValue);
    }
    {
        const auto result = parser.parse(frame("COMMAND_ACK\nMSG=ack-1\nTX=tx-1\nID=scale01\nSESSION=81A9C5D2\nSTATUS=ACCEPTED\nEND\n"));
        const auto* ack = result.message ? std::get_if<CommandAckMessage>(&result.message->payload) : nullptr;
        expect(result.ok() && ack && ack->accepted);
    }
    {
        const auto result = parser.parse(frame("COMMAND_ACK\nMSG=ack-1\nTX=tx-1\nID=scale01\nSESSION=81A9C5D2\nSTATUS=REJECTED\nCODE=COMMAND_DENIED\nDETAIL=not-ready\nEND\n"));
        const auto* ack = result.message ? std::get_if<CommandAckMessage>(&result.message->payload) : nullptr;
        expect(result.ok() && ack && !ack->accepted && ack->code == "COMMAND_DENIED");
    }
    {
        const auto result = parser.parse(frame("COMMAND_ACK\nMSG=ack-1\nTX=tx-1\nID=scale01\nSESSION=81A9C5D2\nSTATUS=REJECTED\nEND\n"));
        expect(!result.ok() && result.error == ParseError::MissingField);
    }
    {
        const auto result = parser.parse(frame("COMMAND_ACK\nMSG=ack-1\nTX=tx-1\nID=scale01\nSESSION=81A9C5D2\nSTATUS=REJECTED\nCODE=\nEND\n"));
        expect(!result.ok() && result.error == ParseError::MalformedField);
    }
    {
        const auto result = parser.parse(frame("COMMAND_ACK\nMSG=ack-1\nTX=tx-1\nID=scale01\nSESSION=81A9C5D2\nSTATUS=PENDING\nEND\n"));
        expect(!result.ok() && result.error == ParseError::InvalidValue);
    }
    {
        const auto result = parser.parse(frame("COMMAND_RESULT\nMSG=result-1\nTX=tx-1\nID=scale01\nSESSION=81A9C5D2\nSTATUS=SUCCESS\nRESULT=done\nEND\n"));
        const auto* command_result = result.message ? std::get_if<CommandResultMessage>(&result.message->payload) : nullptr;
        expect(result.ok() && command_result && command_result->success && command_result->result == "done");
    }
    {
        const auto result = parser.parse(frame("COMMAND_RESULT\nMSG=result-1\nTX=tx-1\nID=scale01\nSESSION=81A9C5D2\nSTATUS=SUCCESS\nRESULT=done\nEXTRA=no\nEND\n"));
        expect(!result.ok() && result.error == ParseError::UnknownField);
    }

    return failures;
}
