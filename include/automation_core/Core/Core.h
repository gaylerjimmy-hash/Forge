#pragma once

#include "automation_core/Connection/ConnectionManager.h"
#include "automation_core/Process/Process.h"
#include "automation_core/Frame/FrameAssembler.h"
#include "automation_core/Protocol/MessageSerializer.h"
#include "automation_core/Protocol/Parser.h"
#include "automation_core/Protocol/Validator.h"
#include "automation_core/Registry/ModuleRegistry.h"
#include "automation_core/Response/ResponseBuilder.h"
#include "automation_core/Router/MessageRouter.h"
#include "automation_core/Transport/ITransport.h"

#include <chrono>
#include <functional>
#include <iosfwd>
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>


namespace automation_core {

enum class CommandTransactionState { Dispatched, Acknowledged, Rejected, Succeeded, Failed, TimedOut, AuthorityLost };
enum class CommandRejection { None, Protocol, Capability, Routing, Correlation, Lifecycle, Timeout, AuthorityLoss };

// Supervisory faults are Core coordination records, not module hardware safety
// actions. Their identities are stable within the Core lifetime.
struct SupervisoryFaultId { std::string value; };
enum class SupervisoryFaultClass { Advisory, Blocking };
enum class SupervisoryFaultSource { ModuleAuthorityLoss, CommandFailure, CommandTimeout, ProcessFailure, OperationalDataUnavailable };
enum class SupervisoryFaultState { Active, Acknowledged, Cleared, Resettable };
struct SupervisoryFaultCorrelation {
    std::string module_id;
    ProcessId process_id;
    ProcessRunId run_id;
    std::string command_transaction_id;
    std::string capability;
};
struct SupervisoryFaultTrace {
    SupervisoryFaultId fault_id;
    SupervisoryFaultState state{SupervisoryFaultState::Active};
    std::string event;
    SupervisoryFaultCorrelation correlation;
};
struct SupervisoryFault {
    SupervisoryFaultId id;
    SupervisoryFaultClass classification{SupervisoryFaultClass::Advisory};
    SupervisoryFaultSource source{SupervisoryFaultSource::ProcessFailure};
    SupervisoryFaultState state{SupervisoryFaultState::Active};
    SupervisoryFaultCorrelation correlation;
    std::vector<std::string> trace;
};
struct CommandTransaction {
    CommandMessage command;
    std::string connection_id;
    CommandTransactionState state{CommandTransactionState::Dispatched};
    CommandRejection rejection{CommandRejection::None};
    std::chrono::steady_clock::time_point deadline{};
    std::vector<std::string> trace;
};

class Core {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using NowFunction = std::function<TimePoint()>;

    explicit Core(ITransport& transport);
    Core(ITransport& transport, std::ostream& trace_output);
    Core(ITransport& transport, NowFunction now);
    Core(
        ITransport& transport,
        NowFunction now,
        std::chrono::milliseconds heartbeat_timeout
    );
    Core(
        ITransport& transport,
        std::ostream& trace_output,
        NowFunction now,
        std::chrono::milliseconds heartbeat_timeout =
            std::chrono::milliseconds{3000}
    );

    bool poll_once();

    // Dispatch is accepted only for an active module with the accepted command
    // capability. The transaction remains inspectable after a terminal state.
    [[nodiscard]] CommandRejection dispatch_command(CommandMessage command, std::chrono::milliseconds timeout = std::chrono::milliseconds{5000});
    [[nodiscard]] std::optional<CommandTransaction> find_command_transaction(const std::string& transaction_id) const;

    // Starts a Core-owned, non-blocking run. Calls to poll_once advance it;
    // this API never waits for a module or measurement.
    [[nodiscard]] bool start_process(ProcessDefinition definition, ProcessRunId run_id,
        std::chrono::milliseconds run_timeout = std::chrono::milliseconds{30000});
    [[nodiscard]] bool abort_process(const ProcessRunId& run_id);
    [[nodiscard]] std::optional<ProcessRun> find_process_run(const ProcessRunId& run_id) const;

    // Reporting and lifecycle requests are deterministic: repeated reporting of
    // an active identity and repeated permitted operations are idempotent.
    [[nodiscard]] bool report_supervisory_fault(SupervisoryFaultId id,
        SupervisoryFaultClass classification, SupervisoryFaultSource source,
        SupervisoryFaultCorrelation correlation = {});
    [[nodiscard]] bool acknowledge_supervisory_fault(const SupervisoryFaultId& id);
    [[nodiscard]] bool clear_supervisory_fault(const SupervisoryFaultId& id);
    [[nodiscard]] bool reset_supervisory_fault(const SupervisoryFaultId& id);
    [[nodiscard]] std::optional<SupervisoryFault> find_supervisory_fault(const SupervisoryFaultId& id) const;
    [[nodiscard]] const std::vector<SupervisoryFault>& supervisory_fault_history() const;
    [[nodiscard]] const std::vector<SupervisoryFaultTrace>& supervisory_fault_traces() const;

private:
    Core(
        ITransport& transport,
        std::ostream* trace_output,
        NowFunction now,
        std::chrono::milliseconds heartbeat_timeout
    );

    void trace(
        const std::string& event,
        const std::string& detail
    ) const;
    void advance_processes(TimePoint now);
    void finish_process(ProcessRun& run, ProcessRunState state, const std::string& reason);
    bool fault_affects_process(const SupervisoryFault& fault, const ProcessDefinition& definition) const;
    bool process_is_inhibited(const ProcessDefinition& definition) const;
    void append_fault_trace(const SupervisoryFault& fault, const std::string& event);
    void raise_generated_fault(SupervisoryFaultSource source, SupervisoryFaultClass classification,
        SupervisoryFaultCorrelation correlation);

    ITransport& transport_;
    std::ostream* trace_output_;
    NowFunction now_;
    std::chrono::milliseconds heartbeat_timeout_;
    std::chrono::milliseconds measurement_timeout_{5000};
    ConnectionManager connections_;
    FrameAssembler frames_;
    Parser parser_;
    MessageSerializer serializer_;
    Validator validator_;
    ModuleRegistry registry_;
    ResponseBuilder responses_;
    MessageRouter router_;
    std::unordered_map<std::string, std::string> transport_connections_;
    std::unordered_map<std::string, CommandTransaction> command_transactions_;
    std::unordered_map<std::string, ProcessDefinition> process_definitions_;
    std::unordered_map<std::string, ProcessRun> process_runs_;
    std::unordered_map<std::string, SupervisoryFault> supervisory_faults_;
    // Append-only diagnostic snapshots and lifecycle event correlations are
    // intentionally in-memory only.
    std::vector<SupervisoryFault> supervisory_fault_history_;
    std::vector<SupervisoryFaultTrace> supervisory_fault_traces_;
    std::unordered_map<std::string, bool> faulted_processes_;
};

} // namespace automation_core
