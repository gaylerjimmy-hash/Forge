#include "automation_core/Frame/FrameAssembler.h"
#include "automation_core/Frame/FrameError.h"

#include <utility>

namespace automation_core {
namespace {
// Normalize incrementally.  Serial reads may split CRLF between packets, so a
// trailing CR cannot be committed until the next fragment identifies whether
// it is followed by LF.
void append_normalized(std::string& bytes, bool& trailing_cr, const std::string& input) {
    size_t index = 0;
    if (trailing_cr) {
        bytes += '\n';
        trailing_cr = false;
        if (!input.empty() && input.front() == '\n') index = 1;
    }
    for (; index < input.size(); ++index) {
        if (input[index] != '\r') {
            bytes += input[index];
            continue;
        }
        if (index + 1 == input.size()) {
            trailing_cr = true;
        } else {
            if (input[index + 1] == '\n') ++index;
            bytes += '\n';
        }
    }
}
// Returns the byte count through a terminator, never matching END within data.
size_t terminator_end(const std::string& s) {
    if (s.rfind("END\n", 0) == 0) return 4;
    const size_t at = s.find("\nEND\n");
    if (at != std::string::npos) return at + 5;
    // The historic final newline-less END is accepted only as a complete line.
    if (s.size() >= 4 && s.compare(s.size() - 4, 4, "\nEND") == 0) return s.size();
    return std::string::npos;
}

// This is deliberately only a lexical boundary check, not protocol parsing.
// It preserves the established Parser trailing-data result for a completed
// protocol message followed by a completed non-message record, while allowing
// a possible next protocol message to remain a streaming frame.
bool starts_protocol_message(const std::string& bytes) {
    const size_t line_end = bytes.find('\n');
    const std::string type = bytes.substr(0, line_end);
    return type == "HELLO" || type == "HEARTBEAT" || type == "CAPABILITIES" ||
           type == "MEASUREMENT" || type == "COMMAND" ||
           type == "COMMAND_ACK" || type == "COMMAND_RESULT";
}
}
FrameAssembler::FrameAssembler(size_t max_size, std::chrono::milliseconds timeout)
    : FrameAssembler(max_size, timeout, [] { return Clock::now(); }) {}

FrameAssembler::FrameAssembler(size_t max_size, std::chrono::milliseconds timeout, NowFunction now)
    : max_(max_size), timeout_(timeout), now_(std::move(now)) {}
void FrameAssembler::age() {
    const auto now = now_();
    for (auto it = pending_.begin(); it != pending_.end();) {
        if (now - it->second.started >= timeout_) it = pending_.erase(it); else ++it;
    }
}
void FrameAssembler::clear(const std::string& connection_id) { pending_.erase(connection_id); }
bool FrameAssembler::has_pending(const std::string& connection_id) const { return pending_.find(connection_id) != pending_.end(); }
std::vector<FrameResult> FrameAssembler::assemble_all(const std::string& id, const std::string& raw) {
    std::vector<FrameResult> results;
    if (id.empty()) { results.push_back({std::nullopt, FrameError::Empty, "empty connection ID"}); return results; }
    const auto now = now_();
    auto existing = pending_.find(id);
    if (existing != pending_.end() && now - existing->second.started >= timeout_) {
        pending_.erase(existing);
        results.push_back({std::nullopt, FrameError::TimedOut, "frame assembly timed out"});
    }
    // An empty packet is an explicit framing error, not a fragment.  In
    // particular, do not create pending state merely to represent it.
    if (raw.empty()) {
        if (results.empty()) results.push_back({std::nullopt, FrameError::Empty, "empty frame"});
        return results;
    }
    {
        auto& initial = pending_[id];
        if (initial.bytes.empty() && !initial.trailing_cr) initial.started = now;
        append_normalized(initial.bytes, initial.trailing_cr, raw);
    }

    // Reacquire the map element on every pass.  A completed or rejected frame
    // may erase it, so no reference into pending_ can outlive that erase.
    while (true) {
        auto current = pending_.find(id);
        if (current == pending_.end() || current->second.bytes.empty()) break;

        const size_t end = terminator_end(current->second.bytes);
        if (end == std::string::npos) {
            if (current->second.bytes.size() > max_) {
                pending_.erase(current);
                results.push_back({std::nullopt, FrameError::TooLarge, "too large"});
            }
            break;
        }
        if (end > max_) {
            current->second.bytes.erase(0, end);
            results.push_back({std::nullopt, FrameError::TooLarge, "too large"});
            if (current->second.bytes.empty() && !current->second.trailing_cr) {
                pending_.erase(current);
                break;
            }
            current->second.started = now;
            continue;
        }

        const std::string frame = current->second.bytes.substr(0, end);
        const std::string trailing = current->second.bytes.substr(end);
        // Historically, a completed non-message record after a completed
        // protocol message is trailing data in that message, not an unrelated
        // transport frame.  Keep that distinction without rejecting a genuine
        // partial (or a complete) next protocol message.
        if (starts_protocol_message(frame) && !trailing.empty() &&
            terminator_end(trailing) != std::string::npos &&
            !starts_protocol_message(trailing)) {
            results.push_back({Frame{id, current->second.bytes}, FrameError::None, ""});
            pending_.erase(current);
            break;
        }
        current->second.bytes.erase(0, end);
        results.push_back({Frame{id, frame}, FrameError::None, ""});
        if (current->second.bytes.empty() && !current->second.trailing_cr) {
            pending_.erase(current);
            break;
        }
        current->second.started = now;
    }
    return results;
}
std::vector<FrameResult> FrameAssembler::assemble_all(const std::string& id, const std::string& raw,
                                                       std::chrono::milliseconds assembly_time) {
    if (assembly_time >= timeout_ && !raw.empty()) {
        clear(id);
        return {{std::nullopt, FrameError::TimedOut, "frame assembly timed out"}};
    }
    return assemble_all(id, raw);
}

FrameResult FrameAssembler::assemble(const std::string& id, const std::string& raw,
                                     std::chrono::milliseconds assembly_time) {
    auto results = assemble_all(id, raw, assembly_time);
    if (!results.empty()) return results.front();
    return {std::nullopt, FrameError::MissingTerminator, "missing END"};
}
} // namespace automation_core
