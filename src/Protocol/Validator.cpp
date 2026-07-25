#include "automation_core/Protocol/Validator.h"

#include <algorithm>
#include <cctype>
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

    if (hello == nullptr) {
        return {
            ValidationStatus::Rejected,
            "UNSUPPORTED",
            "Only HELLO supported"
        };
    }

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

} // namespace automation_core
