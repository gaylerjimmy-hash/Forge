#include "automation_core/Frame/FrameAssembler.h"

#include <chrono>
#include <vector>

using namespace automation_core;

namespace {
bool is_frame(const FrameResult& result, const std::string& payload) {
    return result.ok() && result.frame.has_value() && result.frame->payload == payload;
}
}

int run_frame_tests() {
    int f = 0;
    using namespace std::chrono;

    FrameAssembler fragmented(64, milliseconds{100});
    const auto pending = fragmented.assemble_all("c", "HELLO");
    if (!pending.empty() || !fragmented.has_pending("c")) ++f;
    const auto completed = fragmented.assemble_all("c", "\nEND\n");
    if (completed.size() != 1 || !is_frame(completed[0], "HELLO\nEND\n") || fragmented.has_pending("c")) ++f;
    // An empty transport packet is an explicit frame error and never creates
    // pending state after the completed frame was drained.
    const auto empty = fragmented.assemble_all("c", "");
    if (empty.size() != 1 || empty[0].error != FrameError::Empty || fragmented.has_pending("c")) ++f;

    FrameAssembler sized(10, milliseconds{100});
    if (!sized.assemble_all("s", "12345").empty() ||
        sized.assemble("s", "678901").error != FrameError::TooLarge || sized.has_pending("s")) ++f;
    const auto clean = sized.assemble_all("s", "X\nEND\n");
    if (clean.size() != 1 || !is_frame(clean[0], "X\nEND\n")) ++f;
    // The limit is logical-frame based, not aggregate input-chunk based.
    const auto independent = sized.assemble_all("s", "A\nEND\nB\nEND\n");
    if (independent.size() != 2 || !is_frame(independent[0], "A\nEND\n") ||
        !is_frame(independent[1], "B\nEND\n")) ++f;

    FrameAssembler multi(64, milliseconds{100});
    const auto frames = multi.assemble_all("m", "ONE\nEND\nTWO\nEND\nTAIL");
    if (frames.size() != 2 || !is_frame(frames[0], "ONE\nEND\n") ||
        !is_frame(frames[1], "TWO\nEND\n") || !multi.has_pending("m")) ++f;
    const auto tail = multi.assemble_all("m", "\nEND\n");
    if (tail.size() != 1 || !is_frame(tail[0], "TAIL\nEND\n") || multi.has_pending("m")) ++f;

    // A complete protocol message followed by a possible protocol-message tail
    // emits only the completed frame and retains that tail for exact-once later
    // completion.
    FrameAssembler protocol_tail(256, milliseconds{100});
    const auto protocol_first = protocol_tail.assemble_all("p", "HELLO\nEND\nHEART");
    if (protocol_first.size() != 1 || !is_frame(protocol_first[0], "HELLO\nEND\n") ||
        !protocol_tail.has_pending("p")) ++f;
    const auto protocol_second = protocol_tail.assemble_all("p", "BEAT\nEND\n");
    if (protocol_second.size() != 1 || !is_frame(protocol_second[0], "HEARTBEAT\nEND\n") ||
        protocol_tail.has_pending("p")) ++f;

    // A completed non-message record after a completed protocol message retains
    // the established trailing-data classification for Parser.
    FrameAssembler invalid_trailing(256, milliseconds{100});
    const auto invalid = invalid_trailing.assemble_all("i", "HELLO\nEND\nGARBAGE\nEND\n");
    if (invalid.size() != 1 || !is_frame(invalid[0], "HELLO\nEND\nGARBAGE\nEND\n") ||
        invalid_trailing.has_pending("i")) ++f;

    FrameAssembler lines(64, milliseconds{100});
    if (!lines.assemble_all("l", "FRIEND\nSOMETHINGEND\n").empty() || !lines.has_pending("l")) ++f;
    const auto line_end = lines.assemble_all("l", "END\n");
    if (line_end.size() != 1 || !is_frame(line_end[0], "FRIEND\nSOMETHINGEND\nEND\n")) ++f;
    const auto historic = lines.assemble_all("l", "FINAL\nEND");
    if (historic.size() != 1 || !is_frame(historic[0], "FINAL\nEND")) ++f;
    // A serial read can split a CRLF boundary; normalization must not duplicate
    // the newline or turn a valid terminator into an extra blank protocol line.
    FrameAssembler crlf(64, milliseconds{100});
    if (!crlf.assemble_all("cr", "HELLO\r").empty()) ++f;
    const auto crlf_complete = crlf.assemble_all("cr", "\nEND\r\n");
    if (crlf_complete.size() != 1 || !is_frame(crlf_complete[0], "HELLO\nEND\n")) ++f;

    FrameAssembler::Clock::time_point now{};
    FrameAssembler timed(64, milliseconds{10}, [&now] { return now; });
    static_cast<void>(timed.assemble_all("t", "PART"));
    now += milliseconds{9};
    static_cast<void>(timed.assemble_all("t", "IAL")); // must not reset first-fragment deadline
    now += milliseconds{2};
    const auto expired = timed.assemble_all("t", "");
    if (expired.size() != 1 || expired[0].error != FrameError::TimedOut || timed.has_pending("t")) ++f;
    const auto fresh = timed.assemble_all("t", "FRESH\nEND\n");
    if (fresh.size() != 1 || !is_frame(fresh[0], "FRESH\nEND\n")) ++f;

    return f;
}
