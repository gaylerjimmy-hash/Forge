#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace automation_core {

enum class CapabilityType {
    Measurement,
    Command,
    Configuration
};

enum class CapabilityDataType {
    Boolean,
    Integer,
    UnsignedInteger,
    Float,
    String,
    Enumeration
};

enum class CapabilityAccess {
    Read,
    Write,
    ReadWrite,
    Command
};

struct Capability {
    std::string name;
    CapabilityType type{CapabilityType::Measurement};
    std::optional<CapabilityDataType> data_type;
    CapabilityAccess access{CapabilityAccess::Read};
    std::optional<std::string> unit;
    std::optional<double> minimum;
    std::optional<double> maximum;
    bool supports_quality{false};
    bool supports_calibration{false};
    std::optional<std::string> description;
};

struct CapabilitiesMessage {
    std::string message_id;
    std::string module_id;
    std::string session_id;
    std::uint32_t revision{0};
    std::vector<Capability> items;
};

struct CapabilitiesAckMessage {
    std::string message_id;
    std::string module_id;
    std::uint32_t revision{0};
    std::string status;
};

[[nodiscard]] const char* to_string(CapabilityType value) noexcept;
[[nodiscard]] const char* to_string(CapabilityDataType value) noexcept;
[[nodiscard]] const char* to_string(CapabilityAccess value) noexcept;
[[nodiscard]] std::optional<CapabilityType> parse_capability_type(
    const std::string& value
);
[[nodiscard]] std::optional<CapabilityDataType> parse_capability_data_type(
    const std::string& value
);
[[nodiscard]] std::optional<CapabilityAccess> parse_capability_access(
    const std::string& value
);

} // namespace automation_core
