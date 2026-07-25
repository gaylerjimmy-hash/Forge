#include "automation_core/Protocol/MessageSerializer.h"
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

int run_message_serializer_tests() {
    using namespace automation_core;

    MessageSerializer serializer;
    Parser parser;

    {
        const Message message{CapabilitiesMessage{"50","scale01","81A9C5D2",1,{Capability{"weight",CapabilityType::Measurement,CapabilityDataType::Float,CapabilityAccess::Read,std::string{"lb"}}}}};
        const auto serialized=serializer.serialize(message);
        const auto parsed=parser.parse(frame(serialized));
        expect(parsed.ok());
        expect(parsed.message&&std::holds_alternative<CapabilitiesMessage>(parsed.message->payload));
    }

    {
        const Message message{
            HeartbeatMessage{
                "00000042",
                "scale01",
                "81A9C5D2",
                1042,
                381500,
                ModuleState::Ready,
                0
            }
        };

        const std::string expected =
            "HEARTBEAT\n"
            "MSG=00000042\n"
            "ID=scale01\n"
            "SESSION=81A9C5D2\n"
            "SEQ=1042\n"
            "UPTIME_MS=381500\n"
            "STATE=ready\n"
            "FAULTS=0\n"
            "END\n";

        const std::string serialized = serializer.serialize(message);
        expect(serialized == expected);

        const auto parsed = parser.parse(frame(serialized));
        expect(parsed.ok());
        expect(
            parsed.message &&
            std::holds_alternative<HeartbeatMessage>(
                parsed.message->payload
            )
        );
    }

    {
        const Message message{
            HelloMessage{
                "00000001",
                "Scale",
                "scale01",
                "1.0.0",
                "1",
                "81A9C5D2"
            }
        };

        const std::string expected =
            "HELLO\n"
            "MSG=00000001\n"
            "TYPE=Scale\n"
            "ID=scale01\n"
            "FW=1.0.0\n"
            "PROTO=1\n"
            "SESSION=81A9C5D2\n"
            "END\n";

        expect(serializer.serialize(message) == expected);
    }

    {
        const Message message{
            HelloAckMessage{
                "00000001",
                "connection-1",
                "accepted"
            }
        };

        const std::string expected =
            "HELLO_ACK\n"
            "MSG=00000001\n"
            "CONNECTION=connection-1\n"
            "STATUS=accepted\n"
            "END\n";

        expect(serializer.serialize(message) == expected);
    }

    {
        const Message message{
            ErrorMessage{
                "00000001",
                "INVALID_MESSAGE",
                "Message validation failed"
            }
        };

        const std::string expected =
            "ERROR\n"
            "MSG=00000001\n"
            "CODE=INVALID_MESSAGE\n"
            "DETAIL=Message validation failed\n"
            "END\n";

        expect(serializer.serialize(message) == expected);
    }

    {
        const Message original{
            HelloMessage{
                "00000001",
                "Scale",
                "scale01",
                "1.0.0",
                "1",
                "81A9C5D2"
            }
        };

        const std::string serialized =
            serializer.serialize(original);

        const auto result =
            parser.parse(frame(serialized));

        expect(result.ok());
        expect(result.error == ParseError::None);
        expect(result.message.has_value());

        if (result.message.has_value()) {
            const auto* hello =
                std::get_if<HelloMessage>(
                    &result.message->payload
                );

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

    return failures;
}
