#include "automation_core/Core/Core.h"

#include "automation_core/Frame/FrameError.h"
#include "automation_core/Protocol/ParseResult.h"

#include <algorithm>
#include <ostream>
#include <string>
#include <utility>
#include <variant>
#include <type_traits>

namespace automation_core {
namespace {

std::string frame_error_code(const FrameError error) {
    switch (error) {
        case FrameError::Empty:
            return "FRAME_EMPTY";
        case FrameError::TooLarge:
            return "FRAME_TOO_LARGE";
        case FrameError::TimedOut:
            return "FRAME_TIMEOUT";
        case FrameError::MissingTerminator:
            return "FRAME_TERMINATOR";
        case FrameError::None:
            return "FRAME_ERROR";
    }

    return "FRAME_ERROR";
}

std::string parse_error_code(const ParseError error) {
    switch (error) {
        case ParseError::EmptyFrame:
            return "EMPTY_FRAME";
        case ParseError::UnknownMessageType:
            return "UNKNOWN_MESSAGE_TYPE";
        case ParseError::MalformedField:
            return "MALFORMED_FIELD";
        case ParseError::UnknownField:
            return "UNKNOWN_FIELD";
        case ParseError::DuplicateField:
            return "DUPLICATE_FIELD";
        case ParseError::MissingField:
            return "MISSING_FIELD";
        case ParseError::EmptyValue:
            return "EMPTY_VALUE";
        case ParseError::InvalidValue:
            return "INVALID_VALUE";
        case ParseError::MissingTerminator:
            return "MISSING_TERMINATOR";
        case ParseError::TrailingData:
            return "TRAILING_DATA";
        case ParseError::None:
            return "PARSE_ERROR";
    }

    return "PARSE_ERROR";
}

} // namespace

Core::Core(ITransport& transport)
    : Core(
        transport,
        nullptr,
        [] { return Clock::now(); },
        std::chrono::milliseconds{3000}
    ) {}

Core::Core(ITransport& transport, const std::string& durable_path)
    : Core(transport) {
    persistence_.emplace(durable_path);
    const LoadStatus status = persistence_->load(durable_evidence_);
    recovery_load_status_ = status;
    // Valid interrupted/pending evidence is deliberately ambiguous: no replay
    // or resume is possible until a normal future reconciliation.
    if (status == LoadStatus::Ok && (durable_evidence_.has_interrupted_process || !durable_evidence_.pending_commands.empty()))
        recovery_state_ = RecoveryState::RecoveryRequired;
}

Core::Core(ITransport& transport, std::ostream& trace_output)
    : Core(
        transport,
        &trace_output,
        [] { return Clock::now(); },
        std::chrono::milliseconds{3000}
    ) {}

Core::Core(ITransport& transport, NowFunction now)
    : Core(
        transport,
        nullptr,
        std::move(now),
        std::chrono::milliseconds{3000}
    ) {}

Core::Core(
    ITransport& transport,
    NowFunction now,
    const std::chrono::milliseconds heartbeat_timeout
)
    : Core(
        transport,
        nullptr,
        std::move(now),
        heartbeat_timeout
    ) {}

Core::Core(
    ITransport& transport,
    std::ostream& trace_output,
    NowFunction now,
    const std::chrono::milliseconds heartbeat_timeout
)
    : Core(
        transport,
        &trace_output,
        std::move(now),
        heartbeat_timeout
    ) {}

Core::Core(
    ITransport& transport,
    std::ostream* trace_output,
    NowFunction now,
    const std::chrono::milliseconds heartbeat_timeout
)
    : transport_(transport),
      trace_output_(trace_output),
      now_(std::move(now)),
      heartbeat_timeout_(heartbeat_timeout),
      connections_(),
      frames_(),
      parser_(),
      serializer_(),
      validator_("1"),
      registry_(),
      responses_(),
      router_(validator_, registry_, responses_),
      transport_connections_() {}

LoadStatus Core::recovery_load_status() const noexcept { return recovery_load_status_; }
RecoveryState Core::recovery_state() const noexcept { return recovery_state_; }
OperatorMode Core::effective_operator_mode() const noexcept { return effective_operator_mode_; }
const DurableState& Core::durable_evidence() const noexcept { return durable_evidence_; }
std::optional<ReconciliationOutcome> Core::last_reconciliation_outcome() const noexcept { return last_reconciliation_outcome_; }
bool Core::confirm_recovery() {
    if (recovery_state_ != RecoveryState::RecoveryRequired || !last_reconciliation_outcome_) return false;
    recovery_state_ = RecoveryState::Normal; return true;
}

bool Core::save_durable_evidence(const std::uint64_t revision, std::string configuration_revision, const OperatorMode configured_mode) {
    if (!persistence_) return false;
    DurableState evidence;
    evidence.revision = revision;
    evidence.configuration_revision = std::move(configuration_revision);
    evidence.configured_operator_mode = configured_mode;
    for (const auto& module : registry_.list()) evidence.modules.push_back({module.module_id, module.session_id});
    for (const auto& entry : command_transactions_) {
        const auto& transaction = entry.second;
        if (transaction.state == CommandTransactionState::Dispatched || transaction.state == CommandTransactionState::Acknowledged)
            evidence.pending_commands.push_back({entry.first, transaction.command.module_id, transaction.command.session_id});
    }
    for (const auto& entry : process_runs_) {
        const auto& run = entry.second;
        if ((run.state == ProcessRunState::Pending || run.state == ProcessRunState::Running) && !evidence.has_interrupted_process) {
            evidence.has_interrupted_process = true; evidence.interrupted_process = {run.process_id.value, run.id.value};
        } else if (run.state == ProcessRunState::Succeeded) { evidence.has_last_completed_process = true; evidence.last_completed_process = {run.process_id.value, run.id.value}; }
    }
    for (const auto& entry : supervisory_faults_) if (entry.second.state != SupervisoryFaultState::Resettable) {
        const auto& f=entry.second; evidence.non_resettable_faults.push_back({f.id.value, static_cast<std::uint8_t>(f.classification), static_cast<std::uint8_t>(f.source), static_cast<std::uint8_t>(f.state)});
    }
    if (!persistence_->save(evidence)) return false;
    durable_evidence_ = std::move(evidence); recovery_load_status_ = LoadStatus::Ok; return true;
}

CommandRejection Core::dispatch_command(CommandMessage command, const std::chrono::milliseconds timeout) {
    if (recovery_state_ == RecoveryState::RecoveryRequired) return CommandRejection::Lifecycle;
    const auto module = registry_.find(command.module_id);
    if (!module || module->status != ModuleStatus::Active || module->session_id != command.session_id) return CommandRejection::Routing;
    if (!module->has_capabilities || !module->capabilities_available) return CommandRejection::Capability;
    const auto valid = validator_.validate_command(command, module->capabilities, module->capability_revision);
    if (!valid.valid()) return CommandRejection::Capability;
    if (command_transactions_.count(command.transaction_id)) return CommandRejection::Lifecycle;
    CommandTransaction transaction{command, module->connection_id, CommandTransactionState::Dispatched, CommandRejection::None, now_()+timeout, {"dispatched"}};
    std::string transport_connection;
    for (const auto& mapping : transport_connections_) if (mapping.second == module->connection_id) { transport_connection=mapping.first; break; }
    if (transport_connection.empty()) return CommandRejection::Routing;
    const bool sent=transport_.send(transport_connection, serializer_.serialize(Message{command}));
    if (!sent) { transaction.state=CommandTransactionState::Rejected; transaction.rejection=CommandRejection::Routing; transaction.trace.push_back("routing_rejected"); command_transactions_.emplace(command.transaction_id,std::move(transaction)); return CommandRejection::Routing; }
    command_transactions_.emplace(command.transaction_id,std::move(transaction)); trace("command_dispatched",command.transaction_id); return CommandRejection::None;
}

std::optional<CommandTransaction> Core::find_command_transaction(const std::string& transaction_id) const { const auto it=command_transactions_.find(transaction_id); if(it==command_transactions_.end()) return std::nullopt; return it->second; }

void Core::append_fault_trace(const SupervisoryFault& fault, const std::string& event) {
    supervisory_fault_traces_.push_back({fault.id, fault.state, event, fault.correlation});
    supervisory_fault_history_.push_back(fault);
    trace("supervisory_fault_" + event, fault.id.value);
}

bool Core::fault_affects_process(const SupervisoryFault& fault, const ProcessDefinition& definition) const {
    if (!fault.correlation.process_id.value.empty() &&
        fault.correlation.process_id.value != definition.id.value) return false;
    if (fault.correlation.module_id.empty()) return true;
    for (const auto& step : definition.steps) {
        const std::string& module = step.kind == ProcessStepKind::Command
            ? step.command.module_id : step.condition.module_id;
        if (module == fault.correlation.module_id) return true;
    }
    return false;
}

bool Core::process_is_inhibited(const ProcessDefinition& definition) const {
    if (faulted_processes_.count(definition.id.value) != 0) return true;
    for (const auto& entry : supervisory_faults_) {
        const SupervisoryFault& fault = entry.second;
        if (fault.classification == SupervisoryFaultClass::Blocking &&
            (fault.state == SupervisoryFaultState::Active || fault.state == SupervisoryFaultState::Acknowledged ||
             fault.state == SupervisoryFaultState::Cleared) &&
            fault_affects_process(fault, definition)) return true;
    }
    return false;
}

bool Core::report_supervisory_fault(SupervisoryFaultId id, const SupervisoryFaultClass classification,
    const SupervisoryFaultSource source, SupervisoryFaultCorrelation correlation) {
    if (id.value.empty()) return false;
    const std::string key = id.value;
    const auto existing = supervisory_faults_.find(key);
    if (existing != supervisory_faults_.end()) {
        // A repeated report never duplicates an active lifecycle or abort action.
        if (existing->second.state != SupervisoryFaultState::Resettable) return true;
        existing->second.state = SupervisoryFaultState::Active;
        existing->second.classification = classification;
        existing->second.source = source;
        existing->second.correlation = std::move(correlation);
        existing->second.trace.push_back("reactivated");
        append_fault_trace(existing->second, "reactivated");
    } else {
        SupervisoryFault fault{std::move(id), classification, source, SupervisoryFaultState::Active,
            std::move(correlation), {"active"}};
        supervisory_faults_.emplace(key, std::move(fault));
        append_fault_trace(supervisory_faults_.at(key), "active");
    }
    SupervisoryFault& fault = supervisory_faults_.at(key);
    if (fault.classification == SupervisoryFaultClass::Blocking) {
        for (const auto& definition : process_definitions_) if (fault_affects_process(fault, definition.second)) faulted_processes_[definition.first] = true;
        for (auto& entry : process_runs_) {
            const auto definition = process_definitions_.find(entry.second.process_id.value);
            if (definition != process_definitions_.end() &&
                (entry.second.state == ProcessRunState::Pending || entry.second.state == ProcessRunState::Running) &&
                fault_affects_process(fault, definition->second)) finish_process(entry.second, ProcessRunState::Aborted, "supervisory_fault:" + fault.id.value);
        }
    }
    return true;
}

void Core::raise_generated_fault(const SupervisoryFaultSource source, const SupervisoryFaultClass classification,
    SupervisoryFaultCorrelation correlation) {
    std::string name;
    switch (source) { case SupervisoryFaultSource::ModuleAuthorityLoss: name="authority_loss"; break; case SupervisoryFaultSource::CommandFailure: name="command_failure"; break; case SupervisoryFaultSource::CommandTimeout: name="command_timeout"; break; case SupervisoryFaultSource::ProcessFailure: name="process_failure"; break; case SupervisoryFaultSource::OperationalDataUnavailable: name="operational_data_unavailable"; break; }
    const std::string key = !correlation.command_transaction_id.empty() ? correlation.command_transaction_id :
        !correlation.run_id.value.empty() ? correlation.run_id.value : !correlation.module_id.empty() ? correlation.module_id : correlation.process_id.value;
    // Generated reports are single attempts.  A rejection is observable, but is
    // never retried here because recovery and retry ownership remain explicit.
    if (!report_supervisory_fault({name + ":" + key}, classification, source, std::move(correlation))) {
        trace("supervisory_fault_report_rejected", name + ":" + key);
    }
}

bool Core::acknowledge_supervisory_fault(const SupervisoryFaultId& id) {
    const auto it = supervisory_faults_.find(id.value); if (it == supervisory_faults_.end()) return false;
    if (it->second.state == SupervisoryFaultState::Acknowledged) return true;
    if (it->second.state != SupervisoryFaultState::Active) return false;
    it->second.state = SupervisoryFaultState::Acknowledged; it->second.trace.push_back("acknowledged"); append_fault_trace(it->second, "acknowledged"); return true;
}
bool Core::clear_supervisory_fault(const SupervisoryFaultId& id) {
    const auto it = supervisory_faults_.find(id.value); if (it == supervisory_faults_.end()) return false;
    if (it->second.state == SupervisoryFaultState::Cleared) return true;
    if (it->second.state != SupervisoryFaultState::Active && it->second.state != SupervisoryFaultState::Acknowledged) return false;
    it->second.state = SupervisoryFaultState::Cleared; it->second.trace.push_back("cleared"); append_fault_trace(it->second, "cleared"); return true;
}
bool Core::reset_supervisory_fault(const SupervisoryFaultId& id) {
    const auto it = supervisory_faults_.find(id.value); if (it == supervisory_faults_.end()) return false;
    if (it->second.state == SupervisoryFaultState::Resettable) return true;
    if (it->second.state != SupervisoryFaultState::Cleared) return false;
    it->second.state = SupervisoryFaultState::Resettable; it->second.trace.push_back("reset"); append_fault_trace(it->second, "reset");
    for (const auto& definition : process_definitions_) if (fault_affects_process(it->second, definition.second)) faulted_processes_.erase(definition.first);
    return true;
}
std::optional<SupervisoryFault> Core::find_supervisory_fault(const SupervisoryFaultId& id) const { const auto it=supervisory_faults_.find(id.value); return it==supervisory_faults_.end()?std::nullopt:std::optional<SupervisoryFault>{it->second}; }
const std::vector<SupervisoryFault>& Core::supervisory_fault_history() const { return supervisory_fault_history_; }
const std::vector<SupervisoryFaultTrace>& Core::supervisory_fault_traces() const { return supervisory_fault_traces_; }

bool Core::start_process(ProcessDefinition definition, ProcessRunId run_id, const std::chrono::milliseconds run_timeout) {
    if (recovery_state_ == RecoveryState::RecoveryRequired) return false;
    if (definition.id.value.empty() || run_id.value.empty() || definition.steps.empty() || run_timeout.count() <= 0 ||
        process_runs_.count(run_id.value) != 0) return false;
    for (const auto& step : definition.steps) if (step.id.value.empty() || step.timeout.count() <= 0) return false;
    if (process_is_inhibited(definition)) { trace("process_inhibited", definition.id.value); return false; }
    const TimePoint now = now_();
    ProcessRun run{run_id, definition.id, ProcessRunState::Pending, 0, now + run_timeout, {}, std::nullopt, {"run_pending"}};
    process_definitions_.emplace(definition.id.value, std::move(definition));
    process_runs_.emplace(run_id.value, std::move(run));
    trace("process_started", "process=" + process_runs_.at(run_id.value).process_id.value + " run=" + run_id.value);
    advance_processes(now);
    return true;
}

bool Core::abort_process(const ProcessRunId& run_id) {
    const auto it = process_runs_.find(run_id.value);
    if (it == process_runs_.end() || it->second.state != ProcessRunState::Running) return false;
    finish_process(it->second, ProcessRunState::Aborted, "explicit_abort");
    return true;
}

std::optional<ProcessRun> Core::find_process_run(const ProcessRunId& run_id) const {
    const auto it = process_runs_.find(run_id.value);
    return it == process_runs_.end() ? std::nullopt : std::optional<ProcessRun>{it->second};
}

void Core::finish_process(ProcessRun& run, const ProcessRunState state, const std::string& reason) {
    // A run has one terminal transition.  Generated blocking faults may be
    // raised during the same poll cycle as a native process outcome; the first
    // terminal outcome is retained rather than overwritten by a later path.
    if (run.state != ProcessRunState::Pending && run.state != ProcessRunState::Running) return;
    run.state = state;
    run.trace.push_back(reason);
    std::string detail = "process=" + run.process_id.value + " run=" + run.id.value + " step=" + std::to_string(run.current_step) + " reason=" + reason;
    if (run.command_transaction_id) detail += " command=" + *run.command_transaction_id;
    trace("process_terminal", detail);
    if (state != ProcessRunState::Succeeded && reason.rfind("supervisory_fault:", 0) != 0 &&
        state != ProcessRunState::Aborted) {
        SupervisoryFaultCorrelation correlation;
        correlation.process_id = run.process_id;
        correlation.run_id = run.id;
        correlation.command_transaction_id = run.command_transaction_id.value_or("");
        if (run.command_transaction_id) {
            const auto command = command_transactions_.find(*run.command_transaction_id);
            if (command != command_transactions_.end()) correlation.module_id = command->second.command.module_id;
        }
        raise_generated_fault(SupervisoryFaultSource::ProcessFailure, SupervisoryFaultClass::Blocking, std::move(correlation));
    }
}

void Core::advance_processes(const TimePoint now) {
    for (auto& item : process_runs_) {
        ProcessRun& run = item.second;
        if (run.state != ProcessRunState::Pending && run.state != ProcessRunState::Running) continue;
        const auto definition_it = process_definitions_.find(run.process_id.value);
        if (definition_it == process_definitions_.end()) { finish_process(run, ProcessRunState::Aborted, "definition_unavailable"); continue; }
        const ProcessDefinition& definition = definition_it->second;
        if (now >= run.deadline) { finish_process(run, ProcessRunState::RunTimedOut, "run_timeout"); continue; }
        if (run.state == ProcessRunState::Pending) { run.state = ProcessRunState::Running; run.trace.push_back("run_running"); }
        if (run.current_step >= definition.steps.size()) { finish_process(run, ProcessRunState::Succeeded, "completed"); continue; }
        const ProcessStep& step = definition.steps[run.current_step];
        if (run.step_deadline.time_since_epoch().count() == 0) {
            run.step_deadline = now + step.timeout;
            run.trace.push_back("step_started:" + step.id.value);
            trace("process_step_started", "process=" + run.process_id.value + " run=" + run.id.value + " step=" + step.id.value);
        }
        // Command transaction expiry is established by the Core-owned command
        // boundary before process advancement.  At an equal deadline it is the
        // more specific command outcome, rather than a generic step timeout.
        if (step.kind == ProcessStepKind::Command && run.command_transaction_id) {
            const auto command = find_command_transaction(*run.command_transaction_id);
            if (command && command->state == CommandTransactionState::TimedOut) {
                finish_process(run, ProcessRunState::CommandTimedOut, "command_timeout");
                continue;
            }
        }
        if (now >= run.step_deadline) { finish_process(run, ProcessRunState::StepTimedOut, "step_timeout"); continue; }
        if (step.kind == ProcessStepKind::Command) {
            if (!run.command_transaction_id) {
                run.command_transaction_id = step.command.transaction_id;
                const CommandRejection rejection = dispatch_command(step.command, step.timeout);
                if (rejection != CommandRejection::None) { finish_process(run, ProcessRunState::CommandRejected, "command_rejected"); continue; }
                run.trace.push_back("command_dispatched:" + step.command.transaction_id);
                trace("process_command_dispatched", "process=" + run.process_id.value + " run=" + run.id.value + " step=" + step.id.value + " command=" + step.command.transaction_id);
            }
            const auto command = find_command_transaction(*run.command_transaction_id);
            if (!command) { finish_process(run, ProcessRunState::CommandRejected, "command_missing"); continue; }
            if (command->state == CommandTransactionState::Succeeded) {
                ++run.current_step; run.step_deadline = {}; run.command_transaction_id.reset(); run.trace.push_back("step_succeeded:" + step.id.value);
            } else if (command->state == CommandTransactionState::Rejected) finish_process(run, ProcessRunState::CommandRejected, "command_rejected");
            else if (command->state == CommandTransactionState::Failed) finish_process(run, ProcessRunState::CommandFailed, "command_failed");
            else if (command->state == CommandTransactionState::TimedOut) finish_process(run, ProcessRunState::CommandTimedOut, "command_timeout");
            else if (command->state == CommandTransactionState::AuthorityLost) finish_process(run, ProcessRunState::AuthorityLost, "authority_lost");
        } else {
            const auto module = registry_.find(step.condition.module_id);
            if (!module || module->status != ModuleStatus::Active) { finish_process(run, ProcessRunState::AuthorityLost, "authority_lost"); continue; }
            const auto measurement = module->measurements.find(step.condition.capability);
            if (measurement != module->measurements.end() &&
                measurement->second.effective_quality == MeasurementQuality::Stale) {
                finish_process(run, ProcessRunState::MeasurementStale, "measurement_stale"); continue;
            }
            if (measurement == module->measurements.end() || !measurement->second.operational ||
                measurement->second.effective_quality == MeasurementQuality::Unavailable) {
                raise_generated_fault(SupervisoryFaultSource::OperationalDataUnavailable, SupervisoryFaultClass::Blocking,
                    {step.condition.module_id, run.process_id, run.id, {}, step.condition.capability});
                finish_process(run, ProcessRunState::MeasurementUnavailable, "measurement_unavailable"); continue;
            }
            if (measurement->second.effective_quality == MeasurementQuality::Good &&
                measurement->second.value_text == step.condition.expected_value) {
                ++run.current_step; run.step_deadline = {}; run.trace.push_back("condition_satisfied:" + step.id.value);
            }
        }
    }
}

void Core::trace(
    const std::string& event,
    const std::string& detail
) const {
    if (trace_output_ == nullptr) {
        return;
    }

    *trace_output_ << "TRACE event=" << event;

    if (!detail.empty()) {
        *trace_output_ << " detail=\"" << detail << '"';
    }

    *trace_output_ << '\n';
}

bool Core::poll_once() {
    // Consume the one-shot physical notification before receive() so stale
    // bytes cannot be joined to a later packet with the same stable transport
    // identity. ConnectionManager remains the owner of connection authority.
    if (const auto lifecycle_break = transport_.consume_lifecycle_break()) {
        const auto connection = transport_connections_.find(*lifecycle_break);
        if (connection != transport_connections_.end()) {
            frames_.clear(connection->second);
            trace("transport_lifecycle_break", *lifecycle_break);
        }
    }
    frames_.age();
    const TimePoint now = now_();
    const auto expired_modules =
        registry_.expire_heartbeats(heartbeat_timeout_, now);

    for (const auto& module_id : expired_modules) {
        trace("module_offline", module_id + ": heartbeat timeout");
        trace("measurement_unavailable", module_id);
        for (auto& entry : command_transactions_) if (entry.second.command.module_id==module_id && (entry.second.state==CommandTransactionState::Dispatched || entry.second.state==CommandTransactionState::Acknowledged)) { entry.second.state=CommandTransactionState::AuthorityLost; entry.second.rejection=CommandRejection::AuthorityLoss; entry.second.trace.push_back("authority_lost"); trace("command_authority_lost",entry.first); }
    }
    std::vector<std::string> timed_out_transactions;
    for (auto& entry : command_transactions_) if ((entry.second.state==CommandTransactionState::Dispatched || entry.second.state==CommandTransactionState::Acknowledged) && now >= entry.second.deadline) { entry.second.state=CommandTransactionState::TimedOut; entry.second.rejection=CommandRejection::Timeout; entry.second.trace.push_back("timed_out"); trace("command_timeout",entry.first); timed_out_transactions.push_back(entry.first); }
    const auto stale_measurements=registry_.expire_measurements(measurement_timeout_,now);
    for(const auto& name:stale_measurements)trace("measurement_stale",name);
    // Generated blocking faults are applied before normal advancement.  This
    // gives the supervisory abort the sole terminal transition for that poll.
    for (const auto& module_id : expired_modules)
        raise_generated_fault(SupervisoryFaultSource::ModuleAuthorityLoss, SupervisoryFaultClass::Blocking, {module_id});
    for (const auto& transaction_id : timed_out_transactions) {
        const auto transaction = command_transactions_.find(transaction_id);
        if (transaction == command_transactions_.end()) continue;
        SupervisoryFaultCorrelation correlation{transaction->second.command.module_id, {}, {}, transaction_id};
        for (const auto& run : process_runs_) if (run.second.command_transaction_id && *run.second.command_transaction_id == transaction_id) { correlation.process_id = run.second.process_id; correlation.run_id = run.second.id; break; }
        raise_generated_fault(SupervisoryFaultSource::CommandTimeout, SupervisoryFaultClass::Blocking, std::move(correlation));
    }
    advance_processes(now);

    const auto packet = transport_.receive();

    if (!packet) {
        trace("poll_idle", "transport returned no packet");
        return false;
    }

    if (packet->connection_id.empty()) {
        trace("packet_rejected", "transport connection ID is empty");
        return true;
    }

    auto connection = transport_connections_.find(packet->connection_id);

    if (connection == transport_connections_.end()) {
        const std::string connection_id =
            connections_.open(packet->connection_id);

        connection = transport_connections_
            .emplace(packet->connection_id, connection_id)
            .first;

        trace(
            "connection_opened",
            packet->connection_id + " -> " + connection_id
        );
    } else {
        const auto existing = connections_.find(connection->second);

        if (existing &&
            existing->state == ConnectionState::Quarantined) {
            trace(
                "connection_rejected",
                packet->connection_id + " is quarantined"
            );

            const Message error = responses_.error(
                "",
                "CONNECTION_QUARANTINED",
                "Transport connection is quarantined"
            );

            const bool sent = transport_.send(
                packet->connection_id,
                serializer_.serialize(error)
            );

            trace(
                sent ? "response_sent" : "response_send_failed",
                packet->connection_id
            );
            return true;
        }

        if (connections_.touch(connection->second)) {
            trace(
                "connection_activity",
                packet->connection_id + " -> " + connection->second
            );
        } else {
            const std::string connection_id =
                connections_.open(packet->connection_id);

            connection->second = connection_id;

            trace(
                "connection_reopened",
                packet->connection_id + " -> " + connection_id
            );
        }
    }

    const std::string& connection_id = connection->second;
    const auto frame_results = frames_.assemble_all(connection_id, packet->payload, packet->assembly_time);
    if (frame_results.empty()) {
        trace("frame_pending", connection_id);
        return true;
    }
    for (const FrameResult& frame_result : frame_results) {
    const auto process_frame = [&]() {
    if (!frame_result.ok()) {
        // A raw serial fragment is not a protocol error.  FrameAssembler owns
        // its retention and timeout; Core must not reject it before completion.
        if (frame_result.error == FrameError::MissingTerminator && frames_.has_pending(connection_id)) {
            trace("frame_pending", connection_id);
            return;
        }
        trace(
            "frame_rejected",
            frame_error_code(frame_result.error) +
                ": " + frame_result.detail
        );

        const Message error = responses_.error(
            "",
            frame_error_code(frame_result.error),
            frame_result.detail
        );

        static_cast<void>(transport_.send(
            packet->connection_id,
            serializer_.serialize(error)
        ));
        return;
    }

    trace("frame_accepted", connection_id);

    const ParseResult parse_result = parser_.parse(*frame_result.frame);

    if (!parse_result.ok()) {
        trace(
            "parse_rejected",
            parse_error_code(parse_result.error) +
                ": " + parse_result.detail
        );

        const Message error = responses_.error(
            "",
            parse_error_code(parse_result.error),
            parse_result.detail
        );

        static_cast<void>(transport_.send(
            packet->connection_id,
            serializer_.serialize(error)
        ));
        return;
    }

    trace("parse_accepted", connection_id);

    const auto handle_command_response = [&](const auto& response, const bool /* acknowledgement */) -> bool {
        const auto it=command_transactions_.find(response.transaction_id);
        if (it==command_transactions_.end() || it->second.connection_id!=connection_id || it->second.command.module_id!=response.module_id || it->second.command.session_id!=response.session_id) { trace("command_correlation_rejected",response.transaction_id); return true; }
        auto& transaction=it->second;
        bool result_failed = false;
        if constexpr (std::is_same_v<std::decay_t<decltype(response)>, CommandAckMessage>) {
            if (transaction.state!=CommandTransactionState::Dispatched) { trace("command_lifecycle_rejected",response.transaction_id); return true; }
            if (response.accepted) { transaction.state=CommandTransactionState::Acknowledged; transaction.trace.push_back("acknowledged"); trace("command_acknowledged",response.transaction_id); }
            else { transaction.state=CommandTransactionState::Rejected; transaction.rejection=CommandRejection::Lifecycle; transaction.trace.push_back("ack_rejected"); trace("command_rejected",response.transaction_id); }
        } else {
            if (transaction.state!=CommandTransactionState::Acknowledged) { trace("command_lifecycle_rejected",response.transaction_id); return true; }
            transaction.state=response.success?CommandTransactionState::Succeeded:CommandTransactionState::Failed; transaction.rejection=response.success?CommandRejection::None:CommandRejection::Lifecycle; transaction.trace.push_back(response.success?"result_success":"result_failure"); trace("command_result",response.transaction_id);
            result_failed = !response.success;
        }
        if (result_failed) {
            SupervisoryFaultCorrelation correlation{transaction.command.module_id, {}, {}, response.transaction_id};
            for (const auto& run : process_runs_) if (run.second.command_transaction_id && *run.second.command_transaction_id == response.transaction_id) { correlation.process_id = run.second.process_id; correlation.run_id = run.second.id; break; }
            // Apply the generated blocking outcome before normal advancement so
            // it cannot be followed by a second process terminal transition.
            raise_generated_fault(SupervisoryFaultSource::CommandFailure, SupervisoryFaultClass::Blocking, std::move(correlation));
        }
        advance_processes(now);
        return true;
    };
    if (const auto* ack=std::get_if<CommandAckMessage>(&parse_result.message->payload)) {
        static_cast<void>(handle_command_response(*ack, true));
        return;
    }
    if (const auto* result=std::get_if<CommandResultMessage>(&parse_result.message->payload)) {
        static_cast<void>(handle_command_response(*result, false));
        return;
    }

    const RouteResult route_result =
        router_.route(
            connection_id,
            *parse_result.message,
            now
        );

    // Router has already parsed, validated and registered the HELLO.  Compare
    // only after its normal HELLO_ACK path, so history never gains authority.
    if (const auto* hello = std::get_if<HelloMessage>(&parse_result.message->payload)) {
        if (route_result.response && std::get_if<HelloAckMessage>(&route_result.response->payload)) {
            const auto prior = std::find_if(durable_evidence_.modules.begin(), durable_evidence_.modules.end(),
                [&](const ModuleEvidence& e) { return e.module_id == hello->module_id; });
            if (durable_evidence_.modules.empty()) last_reconciliation_outcome_ = ReconciliationOutcome::NoPriorEvidence;
            else if (prior == durable_evidence_.modules.end()) last_reconciliation_outcome_ = ReconciliationOutcome::IdentityChanged;
            else if (prior->session_id == hello->session_id) last_reconciliation_outcome_ = ReconciliationOutcome::SameSession;
            else last_reconciliation_outcome_ = ReconciliationOutcome::ChangedSession;
        }
    }

    const auto* heartbeat =
        std::get_if<HeartbeatMessage>(&parse_result.message->payload);
    const auto* capabilities =
        std::get_if<CapabilitiesMessage>(&parse_result.message->payload);
    const auto* measurement =
        std::get_if<MeasurementMessage>(&parse_result.message->payload);

    if (!route_result.has_response()) {
        if (heartbeat != nullptr) {
            const std::string detail =
                heartbeat->module_id +
                " state=" + to_string(heartbeat->state) +
                " seq=" + std::to_string(heartbeat->sequence) +
                " uptime_ms=" + std::to_string(heartbeat->uptime_ms) +
                " faults=" +
                    std::to_string(heartbeat->active_fault_count);

            trace(
                route_result.detail == "Duplicate heartbeat ignored"
                    ? "heartbeat_duplicate"
                    : "heartbeat_accepted",
                detail
            );
        }
        if(measurement!=nullptr){
            const std::string detail=measurement->module_id+"."+measurement->capability+" seq="+std::to_string(measurement->sequence);
            trace(route_result.detail=="Duplicate measurement ignored"?"measurement_duplicate":route_result.detail=="Measurement recovered"?"measurement_recovered":"measurement_accepted",detail);
        }

        trace("route_completed", route_result.detail + "; no response");
        advance_processes(now);
        return;
    }

    trace("route_completed", route_result.detail);

    const auto* error =
        std::get_if<ErrorMessage>(&route_result.response->payload);

    if (error != nullptr &&
        (error->code == "DUPLICATE_IDENTITY" ||
         error->code == "QUARANTINED")) {
        static_cast<void>(connections_.quarantine(connection_id));
    }

    if (error != nullptr && heartbeat != nullptr) {
        trace(
            error->code == "SESSION_MISMATCH" ||
                error->code == "UPTIME_REGRESSION"
                ? "module_reboot_suspected"
                : "heartbeat_rejected",
            heartbeat->module_id + ": " + error->code
        );
    }

    if (capabilities != nullptr) {
        if (error != nullptr) {
            trace(
                "capabilities_rejected",
                capabilities->module_id + ": " + error->code
            );
        } else {
            std::string detail =
                capabilities->module_id +
                " revision=" +
                std::to_string(capabilities->revision) +
                " count=" +
                std::to_string(capabilities->items.size()) +
                " names=";

            for (std::size_t i=0;i<capabilities->items.size();++i) {
                if (i != 0) detail += ',';
                detail += capabilities->items[i].name;
            }

            trace(
                route_result.detail ==
                    "Identical capabilities already accepted"
                    ? "capabilities_idempotent"
                    : "capabilities_accepted",
                detail
            );
        }
    }

    if(measurement!=nullptr&&error!=nullptr)trace("measurement_rejected",measurement->module_id+"."+measurement->capability+": "+error->code);

    const auto* acknowledgement =
        std::get_if<HelloAckMessage>(&route_result.response->payload);

    if (acknowledgement != nullptr) {
        if (route_result.detail == "Offline module registered on a new connection") {
            trace("module_recovered", acknowledgement->connection_id);
        } else if (route_result.detail == "Existing session rebound to connection") {
            trace("module_reconnected", acknowledgement->connection_id);
        }
    }

    const bool sent = transport_.send(
        packet->connection_id,
        serializer_.serialize(*route_result.response)
    );

    trace(
        sent ? "response_sent" : "response_send_failed",
        packet->connection_id
    );

    };
    process_frame();
    }
    return true;
}

} // namespace automation_core
