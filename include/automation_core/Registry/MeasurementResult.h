#pragma once
#include <string>
namespace automation_core {
enum class MeasurementPublishStatus {Accepted,Duplicate,OutOfOrder,UnknownModule,Unavailable,Quarantined,Offline,ConnectionMismatch,SessionMismatch,UnknownCapability,InvalidValue};
struct MeasurementPublishResult {
    MeasurementPublishStatus status;
    std::string error_code;
    std::string detail;
    [[nodiscard]] bool accepted() const noexcept {return status==MeasurementPublishStatus::Accepted||status==MeasurementPublishStatus::Duplicate;}
};
}
