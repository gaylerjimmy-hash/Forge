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
            else if constexpr (std::is_same_v<Payload, MeasurementMessage>) {
                std::string result =
                    "MEASUREMENT\nMSG=" + payload.message_id +
                    "\nID=" + payload.module_id +
                    "\nSESSION=" + payload.session_id +
                    "\nCAP=" + payload.capability +
                    "\nSEQ=" + std::to_string(payload.sequence) +
                    "\nVALUE=" + payload.value_text + "\n";
                if (payload.unit) result += "UNIT=" + *payload.unit + "\n";
                result += "QUALITY=" + to_string(payload.quality) + "\n";
                if (payload.uncertainty) result += "UNCERTAINTY=" + std::to_string(*payload.uncertainty) + "\n";
                if (payload.raw) result += "RAW=" + *payload.raw + "\n";
                if (payload.calibration_revision) result += "CAL_REV=" + std::to_string(*payload.calibration_revision) + "\n";
                return result + "END\n";
            }
            else if constexpr (
                std::is_same_v<Payload, CapabilitiesMessage>
            ) {
                std::string result="CAPABILITIES\nMSG="+payload.message_id+"\nID="+payload.module_id+"\nSESSION="+payload.session_id+"\nREV="+std::to_string(payload.revision)+"\nCOUNT="+std::to_string(payload.items.size())+"\n";
                for(std::size_t i=0;i<payload.items.size();++i){const auto& c=payload.items[i];const std::string p="ITEM."+std::to_string(i)+".";result+=p+"NAME="+c.name+"\n"+p+"TYPE="+to_string(c.type)+"\n"+p+"ACCESS="+to_string(c.access)+"\n";if(c.data_type)result+=p+"DATA_TYPE="+std::string(to_string(*c.data_type))+"\n";if(c.unit)result+=p+"UNIT="+*c.unit+"\n";if(c.minimum)result+=p+"MIN="+std::to_string(*c.minimum)+"\n";if(c.maximum)result+=p+"MAX="+std::to_string(*c.maximum)+"\n";if(c.supports_quality)result+=p+"QUALITY=true\n";if(c.supports_calibration)result+=p+"CALIBRATION=true\n";if(c.description)result+=p+"DESCRIPTION="+*c.description+"\n";}
                return result+"END\n";
            }
            else if constexpr (
                std::is_same_v<Payload, CapabilitiesAckMessage>
            ) {
                return "CAPABILITIES_ACK\nMSG="+payload.message_id+"\nID="+payload.module_id+"\nREV="+std::to_string(payload.revision)+"\nSTATUS="+payload.status+"\nEND\n";
            }
            else if constexpr (std::is_same_v<Payload, CommandMessage>) {
                return "COMMAND\nMSG="+payload.message_id+"\nTX="+payload.transaction_id+"\nID="+payload.module_id+"\nSESSION="+payload.session_id+"\nCAP="+payload.capability+"\nCAP_REV="+std::to_string(payload.capability_revision)+"\nPAYLOAD="+payload.payload+"\nEND\n";
            }
            else if constexpr (std::is_same_v<Payload, CommandAckMessage>) {
                std::string result="COMMAND_ACK\nMSG="+payload.message_id+"\nTX="+payload.transaction_id+"\nID="+payload.module_id+"\nSESSION="+payload.session_id+"\nSTATUS="+(payload.accepted?"ACCEPTED":"REJECTED")+"\n";
                if(!payload.code.empty()) result+="CODE="+payload.code+"\n"; if(!payload.detail.empty()) result+="DETAIL="+payload.detail+"\n"; return result+"END\n";
            }
            else if constexpr (std::is_same_v<Payload, CommandResultMessage>) {
                std::string result="COMMAND_RESULT\nMSG="+payload.message_id+"\nTX="+payload.transaction_id+"\nID="+payload.module_id+"\nSESSION="+payload.session_id+"\nSTATUS="+(payload.success?"SUCCESS":"FAILURE")+"\n";
                if(!payload.result.empty()) result+="RESULT="+payload.result+"\n"; if(!payload.code.empty()) result+="CODE="+payload.code+"\n"; if(!payload.detail.empty()) result+="DETAIL="+payload.detail+"\n"; return result+"END\n";
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
