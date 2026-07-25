#include "automation_core/Protocol/Measurement.h"

namespace automation_core {

std::optional<MeasurementQuality> parse_measurement_quality(const std::string& value)
{
    if (value == "good") return MeasurementQuality::Good;
    if (value == "uncertain") return MeasurementQuality::Uncertain;
    if (value == "bad") return MeasurementQuality::Bad;
    if (value == "calibrating") return MeasurementQuality::Calibrating;
    if (value == "out_of_range") return MeasurementQuality::OutOfRange;
    if (value == "unavailable") return MeasurementQuality::Unavailable;
    return std::nullopt;
}

std::string to_string(MeasurementQuality quality)
{
    switch (quality)
    {
    case MeasurementQuality::Good: return "good";
    case MeasurementQuality::Uncertain: return "uncertain";
    case MeasurementQuality::Bad: return "bad";
    case MeasurementQuality::Calibrating: return "calibrating";
    case MeasurementQuality::OutOfRange: return "out_of_range";
    case MeasurementQuality::Unavailable: return "unavailable";
    case MeasurementQuality::Stale: return "stale";
    }
    return "bad";
}

} // namespace automation_core
