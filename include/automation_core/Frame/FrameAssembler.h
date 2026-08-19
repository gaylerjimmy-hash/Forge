#pragma once

#include "automation_core/Frame/FrameResult.h"

#include <chrono>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace automation_core {
class FrameAssembler {
public:
    using Clock = std::chrono::steady_clock;
    using NowFunction = std::function<Clock::time_point()>;

    explicit FrameAssembler(size_t max_size = 16384,
                            std::chrono::milliseconds timeout = std::chrono::milliseconds{1000});
    FrameAssembler(size_t max_size, std::chrono::milliseconds timeout,
                   NowFunction now);
    // Compatibility single-result entry point. New transport integration uses
    // assemble_all so no complete result from a packet is discarded.
    FrameResult assemble(const std::string& connection_id, const std::string& raw_input,
                         std::chrono::milliseconds assembly_time = std::chrono::milliseconds{0});
    std::vector<FrameResult> assemble_all(const std::string& connection_id, const std::string& raw_input);
    std::vector<FrameResult> assemble_all(const std::string& connection_id, const std::string& raw_input,
                                          std::chrono::milliseconds assembly_time);
    void clear(const std::string& connection_id);
    [[nodiscard]] bool has_pending(const std::string& connection_id) const;
    void age();

private:
    // A CR at a transport-fragment boundary is retained separately so a split
    // CRLF is normalized as one newline rather than two.
    struct Pending { std::string bytes; Clock::time_point started; bool trailing_cr{false}; };
    size_t max_;
    std::chrono::milliseconds timeout_;
    NowFunction now_;
    std::unordered_map<std::string, Pending> pending_;
};
} // namespace automation_core
