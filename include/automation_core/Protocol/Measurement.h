#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace automation_core {

enum class MeasurementQuality {
    Good,
    Uncertain,
    Bad,
    Calibrating,
    OutOfRange,
    Unavailable,
    Stale
};

using MeasurementValue = std::variant<bool, std::int64_t, std::uint64_t, double, std::string>;

struct MeasurementMessage {
    std::string message_id;
    std::string module_id;
    std::string session_id;
    std::string capability;
    std::uint32_t sequence{0};
    std::string value_text;
    MeasurementQuality quality{MeasurementQuality::Good};
    std::optional<std::string> unit;
    std::optional<double> uncertainty;
    std::optional<std::string> raw;
    std::optional<std::uint32_t> calibration_revision;
};

std::optional<MeasurementQuality> parse_measurement_quality(const std::string& value);
std::string to_string(MeasurementQuality quality);

} // namespace automation_core
