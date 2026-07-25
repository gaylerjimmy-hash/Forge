#include "automation_core/Protocol/Capability.h"

namespace automation_core {

const char* to_string(CapabilityType v) noexcept {
    switch(v){case CapabilityType::Measurement:return "measurement";case CapabilityType::Command:return "command";case CapabilityType::Configuration:return "configuration";}
    return "measurement";
}
const char* to_string(CapabilityDataType v) noexcept {
    switch(v){case CapabilityDataType::Boolean:return "bool";case CapabilityDataType::Integer:return "int";case CapabilityDataType::UnsignedInteger:return "uint";case CapabilityDataType::Float:return "float";case CapabilityDataType::String:return "string";case CapabilityDataType::Enumeration:return "enum";}
    return "string";
}
const char* to_string(CapabilityAccess v) noexcept {
    switch(v){case CapabilityAccess::Read:return "read";case CapabilityAccess::Write:return "write";case CapabilityAccess::ReadWrite:return "read_write";case CapabilityAccess::Command:return "command";}
    return "read";
}
std::optional<CapabilityType> parse_capability_type(const std::string& v){
    if(v=="measurement")return CapabilityType::Measurement;if(v=="command")return CapabilityType::Command;if(v=="configuration")return CapabilityType::Configuration;return std::nullopt;
}
std::optional<CapabilityDataType> parse_capability_data_type(const std::string& v){
    if(v=="bool")return CapabilityDataType::Boolean;if(v=="int")return CapabilityDataType::Integer;if(v=="uint")return CapabilityDataType::UnsignedInteger;if(v=="float")return CapabilityDataType::Float;if(v=="string")return CapabilityDataType::String;if(v=="enum")return CapabilityDataType::Enumeration;return std::nullopt;
}
std::optional<CapabilityAccess> parse_capability_access(const std::string& v){
    if(v=="read")return CapabilityAccess::Read;if(v=="write")return CapabilityAccess::Write;if(v=="read_write")return CapabilityAccess::ReadWrite;if(v=="command")return CapabilityAccess::Command;return std::nullopt;
}
}
