#pragma once

#include "automation_core/Protocol/Message.h"

#include <optional>
#include <string>

namespace automation_core {

enum class ParseError {
    None,
    EmptyFrame,
    UnknownMessageType,
    MalformedField,
    UnknownField,
    DuplicateField,
    MissingField,
    EmptyValue,
    InvalidValue,
    MissingTerminator,
    TrailingData
};

struct ParseResult {
    std::optional<Message> message;
    ParseError error{ParseError::None};
    std::string detail;

    [[nodiscard]] bool ok() const noexcept {
        return message.has_value();
    }
};

} // namespace automation_core
