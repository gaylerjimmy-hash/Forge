#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace automation_core {

// This is deliberately evidence, not a snapshot of Core's live object graph.
// In particular it contains no connection, deadline, registry, transaction, or
// ProcessRun object.
enum class OperatorMode : std::uint8_t { Manual = 0, Automatic = 1 };
enum class LoadStatus { Ok, Missing, Malformed, Truncated, UnsupportedVersion, Invalid, Inconsistent, IoError };

struct ModuleEvidence { std::string module_id; std::string session_id; };
struct ProcessEvidence { std::string process_id; std::string run_id; };
struct PendingCommandEvidence { std::string transaction_id; std::string module_id; std::string session_id; };
struct SupervisoryFaultEvidence { std::string fault_id; std::uint8_t classification{}; std::uint8_t source{}; std::uint8_t state{}; };
struct CalibrationEvidence { std::string key; std::string metadata; };

struct DurableState {
    // Revision is owned by the durable format and monotonically supplied by the caller.
    std::uint64_t revision{};
    std::vector<ModuleEvidence> modules;
    bool has_interrupted_process{false};
    ProcessEvidence interrupted_process;
    bool has_last_completed_process{false};
    ProcessEvidence last_completed_process;
    std::vector<PendingCommandEvidence> pending_commands;
    std::vector<SupervisoryFaultEvidence> non_resettable_faults;
    std::string configuration_revision;
    std::vector<CalibrationEvidence> calibration;
    OperatorMode configured_operator_mode{OperatorMode::Manual};
};

class PersistenceStore {
public:
    // Test-only deterministic save-stage injection. Normal operation is None.
    enum class SaveFailure { None, Write, Flush, Close, Replace };

    static constexpr std::uint16_t schema_version = 1;
    static constexpr std::size_t max_count = 256;
    static constexpr std::size_t max_string = 4096;

    explicit PersistenceStore(std::string path);
    [[nodiscard]] LoadStatus load(DurableState& destination) const;
    // Candidate image is fully written, flushed and closed before replacement.
    // A failure leaves the previously accepted destination image untouched.
    [[nodiscard]] bool save(const DurableState& state) const;
    [[nodiscard]] const std::string& path() const noexcept;
    // Intended for focused PersistenceStore tests; resets to None after use.
    static void set_save_failure_for_testing(SaveFailure failure) noexcept;
private:
    std::string path_;
};

} // namespace automation_core
