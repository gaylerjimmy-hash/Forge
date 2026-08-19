#include "automation_core/Protocol/Validator.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <unordered_set>
#include <utility>

namespace automation_core {
namespace {

bool is_ascii_alpha(const char value) {
    const auto ch = static_cast<unsigned char>(value);
    return std::isalpha(ch) != 0;
}

bool is_ascii_alnum(const char value) {
    const auto ch = static_cast<unsigned char>(value);
    return std::isalnum(ch) != 0;
}

bool is_hex_digit(const char value) {
    const auto ch = static_cast<unsigned char>(value);
    return std::isxdigit(ch) != 0;
}

bool is_identifier(const std::string& value) {
    if (value.empty() || !is_ascii_alpha(value.front())) {
        return false;
    }

    return std::all_of(
        value.begin() + 1,
        value.end(),
        [](const char ch) {
            return is_ascii_alnum(ch) || ch == '_' || ch == '-';
        }
    );
}

bool is_message_id(const std::string& value) {
    return !value.empty() &&
        std::all_of(
            value.begin(),
            value.end(),
            [](const char ch) {
                return is_ascii_alnum(ch) || ch == '_' || ch == '-';
            }
        );
}

bool is_session_id(const std::string& value) {
    return value.size() == 8 &&
        std::all_of(value.begin(), value.end(), is_hex_digit);
}

bool is_firmware_version(const std::string& value) {
    int component_count = 0;
    bool component_has_digit = false;

    for (const char ch : value) {
        if (std::isdigit(static_cast<unsigned char>(ch)) != 0) {
            component_has_digit = true;
            continue;
        }

        if (ch != '.' || !component_has_digit) {
            return false;
        }

        ++component_count;
        component_has_digit = false;
    }

    return component_count == 2 && component_has_digit;
}

} // namespace

Validator::Validator(std::string supported_protocol_version)
    : supported_protocol_version_(std::move(supported_protocol_version)) {}

ValidationResult Validator::validate(const Message& message) const {
    const auto* hello = std::get_if<HelloMessage>(&message.payload);

    if (hello != nullptr) {
        if (hello->protocol_version != supported_protocol_version_) {
            return {
                ValidationStatus::VersionMismatch,
                "PROTO_VERSION",
                "Unsupported protocol"
            };
        }

        if (!is_session_id(hello->session_id)) {
            return {
                ValidationStatus::Rejected,
                "SESSION",
                "Invalid session"
            };
        }

        if (!is_identifier(hello->module_type)) {
            return {
                ValidationStatus::Rejected,
                "MODULE_TYPE",
                "Invalid module type"
            };
        }

        if (!is_identifier(hello->module_id)) {
            return {
                ValidationStatus::Rejected,
                "MODULE_ID",
                "Invalid module id"
            };
        }

        if (!is_firmware_version(hello->firmware_version)) {
            return {
                ValidationStatus::Rejected,
                "FW",
                "Invalid firmware"
            };
        }

        if (!is_message_id(hello->message_id)) {
            return {
                ValidationStatus::Rejected,
                "MSG",
                "Invalid message id"
            };
        }

        return {
            ValidationStatus::Valid,
            "",
            ""
        };
    }

    const auto* heartbeat =
        std::get_if<HeartbeatMessage>(&message.payload);

    if (heartbeat != nullptr) {
        if (!is_session_id(heartbeat->session_id)) {
            return {
                ValidationStatus::Rejected,
                "SESSION",
                "Invalid session"
            };
        }

        if (!is_identifier(heartbeat->module_id)) {
            return {
                ValidationStatus::Rejected,
                "MODULE_ID",
                "Invalid module id"
            };
        }

        if (!is_message_id(heartbeat->message_id)) {
            return {
                ValidationStatus::Rejected,
                "MSG",
                "Invalid message id"
            };
        }

        return {
            ValidationStatus::Valid,
            "",
            ""
        };
    }

    const auto* capabilities =
        std::get_if<CapabilitiesMessage>(&message.payload);

    if (capabilities != nullptr) {
        if (!is_session_id(capabilities->session_id)) return {ValidationStatus::Rejected,"SESSION","Invalid session"};
        if (!is_identifier(capabilities->module_id)) return {ValidationStatus::Rejected,"MODULE_ID","Invalid module id"};
        if (!is_message_id(capabilities->message_id)) return {ValidationStatus::Rejected,"MSG","Invalid message id"};
        if (capabilities->items.empty() || capabilities->items.size() > 64) return {ValidationStatus::Rejected,"CAPABILITY_COUNT","Capability count must be between 1 and 64"};
        std::unordered_set<std::string> names;
        for (const auto& item : capabilities->items) {
            if (!is_identifier(item.name) || item.name.size() > 32) return {ValidationStatus::Rejected,"CAPABILITY_NAME","Invalid capability name"};
            if (!names.insert(item.name).second) return {ValidationStatus::Rejected,"DUPLICATE_CAPABILITY","Capability names must be unique"};
            if (item.type == CapabilityType::Command) {
                if (item.access != CapabilityAccess::Command) return {ValidationStatus::Rejected,"CAPABILITY_ACCESS","Command capability requires command access"};
            } else {
                if (!item.data_type) return {ValidationStatus::Rejected,"CAPABILITY_DATA_TYPE","Measurement and configuration require data type"};
                if (item.access == CapabilityAccess::Command) return {ValidationStatus::Rejected,"CAPABILITY_ACCESS","Non-command capability cannot use command access"};
            }
            if (item.minimum.has_value() != item.maximum.has_value()) return {ValidationStatus::Rejected,"CAPABILITY_RANGE","MIN and MAX must appear together"};
            if (item.minimum && (!std::isfinite(*item.minimum) || !std::isfinite(*item.maximum) || *item.minimum > *item.maximum)) return {ValidationStatus::Rejected,"CAPABILITY_RANGE","Invalid capability range"};
            if (item.unit && item.unit->size() > 32) return {ValidationStatus::Rejected,"CAPABILITY_UNIT","Capability unit is too long"};
            if (item.description && item.description->size() > 256) return {ValidationStatus::Rejected,"CAPABILITY_DESCRIPTION","Capability description is too long"};
        }
        return {ValidationStatus::Valid,"",""};
    }

    const auto* command = std::get_if<CommandMessage>(&message.payload);
    if (command != nullptr) {
        if (!is_message_id(command->message_id) || !is_message_id(command->transaction_id)) return {ValidationStatus::Rejected,"COMMAND_CORRELATION","Invalid command correlation"};
        if (!is_identifier(command->module_id) || !is_session_id(command->session_id) || !is_identifier(command->capability) || command->payload.empty()) return {ValidationStatus::Rejected,"COMMAND_PROTOCOL","Invalid command fields"};
        return {ValidationStatus::Valid,"",""};
    }
    const auto* ack = std::get_if<CommandAckMessage>(&message.payload);
    if (ack != nullptr) {
        if (!is_message_id(ack->message_id) || !is_message_id(ack->transaction_id) || !is_identifier(ack->module_id) || !is_session_id(ack->session_id) || (!ack->accepted && ack->code.empty())) return {ValidationStatus::Rejected,"COMMAND_PROTOCOL","Invalid command acknowledgement"};
        return {ValidationStatus::Valid,"",""};
    }
    const auto* result = std::get_if<CommandResultMessage>(&message.payload);
    if (result != nullptr) {
        if (!is_message_id(result->message_id) || !is_message_id(result->transaction_id) || !is_identifier(result->module_id) || !is_session_id(result->session_id)) return {ValidationStatus::Rejected,"COMMAND_PROTOCOL","Invalid command result"};
        return {ValidationStatus::Valid,"",""};
    }

    const auto* measurement = std::get_if<MeasurementMessage>(&message.payload);
    if (measurement != nullptr) {
        if (!is_message_id(measurement->message_id)) return {ValidationStatus::Rejected,"MSG","Invalid message id"};
        if (!is_identifier(measurement->module_id)) return {ValidationStatus::Rejected,"MODULE_ID","Invalid module id"};
        if (!is_session_id(measurement->session_id)) return {ValidationStatus::Rejected,"SESSION","Invalid session"};
        if (!is_identifier(measurement->capability) || measurement->capability.size()>32) return {ValidationStatus::Rejected,"CAPABILITY_NAME","Invalid capability name"};
        if (measurement->value_text.empty()) return {ValidationStatus::Rejected,"VALUE","Measurement value is empty"};
        if (measurement->unit && measurement->unit->size()>32) return {ValidationStatus::Rejected,"UNIT","Measurement unit is too long"};
        if (measurement->uncertainty && (!std::isfinite(*measurement->uncertainty) || *measurement->uncertainty<0.0)) return {ValidationStatus::Rejected,"UNCERTAINTY","Uncertainty must be finite and nonnegative"};
        if (measurement->quality==MeasurementQuality::Stale) return {ValidationStatus::Rejected,"QUALITY","stale is reserved for Forge OS"};
        return {ValidationStatus::Valid,"",""};
    }

    return {
        ValidationStatus::Rejected,
        "UNSUPPORTED",
        "Unsupported message type"
    };
}

ValidationResult Validator::validate_command(const CommandMessage& command, const std::vector<Capability>& accepted_capabilities, const std::uint32_t accepted_revision) const {
    const auto basic = validate(Message{command});
    if (!basic.valid()) return basic;
    if (command.capability_revision != accepted_revision) return {ValidationStatus::Rejected,"COMMAND_CAPABILITY_REVISION","Command capability revision is not accepted"};
    const auto found = std::find_if(accepted_capabilities.begin(), accepted_capabilities.end(), [&](const Capability& item) { return item.name == command.capability && item.type == CapabilityType::Command && item.access == CapabilityAccess::Command; });
    if (found == accepted_capabilities.end()) return {ValidationStatus::Rejected,"COMMAND_CAPABILITY","Command capability is not accepted"};
    return {ValidationStatus::Valid,"",""};
}

} // namespace automation_core
