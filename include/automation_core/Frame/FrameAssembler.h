#pragma once
#include "automation_core/Frame/FrameResult.h"
#include <chrono>
#include <string>
namespace automation_core {
class FrameAssembler {
public:
    explicit FrameAssembler(size_t maxSize=16384,std::chrono::milliseconds timeout=std::chrono::milliseconds{1000});
    FrameResult assemble(
        const std::string& connection_id,
        const std::string& raw_input,
        std::chrono::milliseconds assembly_time =
            std::chrono::milliseconds{0}
    ) const;
private:
    size_t max_;
    std::chrono::milliseconds timeout_;
};
}
