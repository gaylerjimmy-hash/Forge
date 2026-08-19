#pragma once

#include "automation_core/Protocol/Message.h"

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace automation_core {

// Strongly named orchestration identities. They deliberately remain separate
// from wire command transaction identifiers owned by the command boundary.
struct ProcessId { std::string value; };
struct ProcessRunId { std::string value; };
struct ProcessStepId { std::string value; };

struct MeasurementCondition {
    std::string module_id;
    std::string capability;
    std::string expected_value;
};

enum class ProcessStepKind { Command, MeasurementCondition };
struct ProcessStep {
    ProcessStepId id;
    ProcessStepKind kind{ProcessStepKind::Command};
    std::chrono::milliseconds timeout{5000};
    CommandMessage command;
    MeasurementCondition condition;
};

struct ProcessDefinition {
    ProcessId id;
    std::vector<ProcessStep> steps;
};

enum class ProcessRunState {
    Pending, Running, Succeeded, CommandRejected, CommandFailed,
    CommandTimedOut, AuthorityLost, MeasurementStale,
    MeasurementUnavailable, StepTimedOut, RunTimedOut, Aborted
};

struct ProcessRun {
    ProcessRunId id;
    ProcessId process_id;
    ProcessRunState state{ProcessRunState::Pending};
    std::size_t current_step{0};
    std::chrono::steady_clock::time_point deadline{};
    std::chrono::steady_clock::time_point step_deadline{};
    std::optional<std::string> command_transaction_id;
    std::vector<std::string> trace;
};

} // namespace automation_core
