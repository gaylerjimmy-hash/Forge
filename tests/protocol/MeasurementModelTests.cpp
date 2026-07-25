#include "automation_core/Protocol/Measurement.h"

#include <iostream>

int run_measurement_model_tests()
{
    int failures = 0;
    const auto good = automation_core::parse_measurement_quality("good");
    if (!good || *good != automation_core::MeasurementQuality::Good)
    {
        std::cerr << "measurement quality GOOD was not parsed\n";
        ++failures;
    }
    if (automation_core::parse_measurement_quality("stale"))
    {
        std::cerr << "wire quality STALE must be rejected\n";
        ++failures;
    }
    if (automation_core::to_string(automation_core::MeasurementQuality::Stale) != "stale")
    {
        std::cerr << "internal stale quality was not rendered\n";
        ++failures;
    }
    automation_core::MeasurementValue value = std::int64_t{-3};
    if (std::get<std::int64_t>(value) != -3)
    {
        ++failures;
    }
    return failures;
}
