#pragma once
#include <string>
namespace automation_core {
enum class CapabilityPublishStatus{Accepted,Idempotent,UnknownModule,ConnectionMismatch,SessionMismatch,Offline,Quarantined,StaleRevision,RevisionConflict};
struct CapabilityPublishResult{
 CapabilityPublishStatus status{CapabilityPublishStatus::UnknownModule};
 std::string error_code;
 std::string detail;
 bool accepted()const noexcept{return status==CapabilityPublishStatus::Accepted||status==CapabilityPublishStatus::Idempotent;}
};
}
