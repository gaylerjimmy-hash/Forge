#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace automation_core {

// These are intentionally evidence records, rather than runtime objects.  In
// particular they contain no connection, deadline, or transport state.
enum class OperatorMode : std::uint8_t { Manual = 0, Automatic = 1 };
enum class PersistenceLoadStatus { Ok, Missing, Malformed, Truncated, UnsupportedVersion, Invalid, Inconsistent, IoError };
enum class PersistenceSaveStatus { Ok, WriteError, FlushError, CloseError, ReplaceError, IoError };

struct ModuleEvidence { std::string module_id; std::string session_id; };
struct ProcessEvidence { std::string process_id; std::string run_id; std::string outcome; };
struct PendingCommandEvidence { std::string transaction_id; std::string module_id; std::string session_id; std::string command; };
struct FaultEvidence { std::string fault_id; std::uint8_t classification{}; std::uint8_t source{}; std::uint8_t state{}; };
struct CalibrationMetadata { std::string calibration_id; std::string revision; };

struct DurableRecoveryState {
    std::uint32_t revision{};
    std::vector<ModuleEvidence> modules;
    std::vector<ProcessEvidence> interrupted_processes;
    std::vector<ProcessEvidence> completed_processes;
    std::vector<PendingCommandEvidence> pending_commands;
    std::vector<FaultEvidence> non_resettable_faults;
    std::string configuration_revision;
    std::vector<CalibrationMetadata> calibrations;
    OperatorMode persisted_operator_mode{OperatorMode::Manual};
};

// Optional seam for deterministic save error coverage. Production callers do
// not need to provide it; each failure happens before replacing the old image.
class PersistenceFileOps {
public:
    virtual ~PersistenceFileOps() = default;
    virtual bool write_candidate(const std::string& candidate, const std::vector<std::uint8_t>& bytes) = 0;
    virtual bool flush_candidate(const std::string& candidate) = 0;
    virtual bool close_candidate(const std::string& candidate) = 0;
    virtual bool replace_candidate(const std::string& candidate, const std::string& destination) = 0;
};

class PersistenceStore {
public:
    static constexpr std::uint16_t SchemaVersion = 1;
    static constexpr std::uint32_t MaxCollectionCount = 1024;
    static constexpr std::uint32_t MaxStringLength = 4096;

    explicit PersistenceStore(std::string path, PersistenceFileOps* file_ops = nullptr);
    PersistenceLoadStatus load(DurableRecoveryState& output) const;
    PersistenceSaveStatus save(const DurableRecoveryState& state) const;

    // Exposed for format-aware test fixtures; the result includes the checksum.
    static std::vector<std::uint8_t> serialize(const DurableRecoveryState& state);
    static PersistenceLoadStatus deserialize(const std::vector<std::uint8_t>& image, DurableRecoveryState& output);
private:
    std::string path_;
    PersistenceFileOps* file_ops_;
};

} // namespace automation_core
