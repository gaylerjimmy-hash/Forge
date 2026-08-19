# Milestone Status

Forge OS uses C++17 for the reference implementation and Module SDK. Forge
Protocol v0.1 uses the existing line-oriented `KEY=VALUE` wire format.

## Milestone 1 — Discovery

Complete and covered by component, Core-level, and failure-path tests:

- framing limits and timeouts
- strict parsing and validation
- protocol-version enforcement
- module registration and authoritative connection ownership
- reconnect, duplicate identity, and quarantine behavior
- `HELLO_ACK` and correlated `ERROR` responses
- functional `ConsoleTransport` and continuous CLI operation

## Milestone 2 — Module Presence and Health

Complete and covered by component and Core-level tests:

- typed `HEARTBEAT` messages
- normalized module states
- authoritative connection and session checks
- duplicate, out-of-order, and unsigned 32-bit rollover handling
- uptime regression and probable reboot detection
- injectable monotonic clock and heartbeat expiration
- offline rediscovery and lifecycle tracing

## Milestone 3 — Capability Discovery

Complete and covered by protocol, registry, router, and Core tests:

- indexed `CAPABILITIES` and `CAPABILITIES_ACK` messages
- bounded, typed capability metadata
- atomic higher-revision replacement
- idempotent identical retries
- stale and same-revision conflict rejection
- offline, quarantine, and new-session invalidation
- capability lifecycle tracing

## Milestone 4 — Measurement Ingestion

Complete and covered by protocol, validation, registry, router, and end-to-end
Core tests:

- typed `MEASUREMENT` model and deterministic serialization
- strict parsing of required and declared optional fields
- capability-aware data-type, access, unit, length, and range validation
- per-capability unsigned 32-bit sequence ordering
- duplicate suppression without freshness refresh
- retained typed values and published/effective quality
- monotonic five-second stale transition
- offline, quarantine, capability-revision, and new-session invalidation
- accepted, duplicate, rejected, stale, unavailable, and recovered traces
- no success acknowledgement; correlated `ERROR` on rejection

## Milestone 5 — Command Transactions

Complete and covered by protocol, validator, serializer, and Core tests:

- typed `COMMAND`, `COMMAND_ACK`, and `COMMAND_RESULT` messages
- strict parsing, validation, and deterministic serialization
- accepted command-capability and capability-revision validation
- Core-owned authority-bound command transaction state machine
- authoritative connection routing and ACK/RESULT correlation
- rejection of invalid, duplicate, stale, and out-of-state responses
- monotonic transaction timeouts and authority-loss invalidation
- command lifecycle tracing
- rejected acknowledgements require a non-empty rejection code
- append-only Milestone 5 protocol documentation

## Milestone 6 — Process Orchestration

Complete and covered by Core-level state-machine, failure-path, timeout,
abort, and end-to-end tests:

- typed process, run, and step identities
- explicit non-blocking Core-owned process state machine
- ordered command and measurement-condition steps
- command dispatch through the accepted Milestone 5 transaction boundary
- operationally-current measurement-condition evaluation
- monotonic process and step deadlines
- deterministic command rejection, failure, timeout, and authority-loss outcomes
- distinct stale and unavailable measurement outcomes
- explicit process abort
- correlated process, run, step, and command transaction tracing

## Milestone 7 — Supervisory Fault Handling and Recovery

Next implementation milestone:

1. Define typed supervisory fault identities, classes, sources, and lifecycle
   states.
2. Add an explicit Core-owned fault state machine for active, acknowledged,
   cleared, and resettable faults.
3. Raise deterministic supervisory faults from relevant module authority loss,
   command failure/timeout, process failure, and unavailable operational data.
4. Keep module-local protective action independent; Core supervisory faults
   must not replace hardware or module safety behavior.
5. Inhibit starting affected process work while blocking faults are active.
6. Abort or deterministically terminate affected active process work when a
   configured blocking fault becomes active.
7. Require explicit recovery/reset action before previously faulted process
   work may restart; never auto-restart a process.
8. Preserve fault history and correlated traces in memory for diagnostics
   without adding persistence.
9. Make duplicate fault reports, repeated acknowledgement, clear, and reset
   operations deterministic and idempotent where appropriate.
10. Add focused fault-lifecycle, process-inhibit, abort, recovery, authority,
    timeout, duplicate-event, and end-to-end tests.

Milestone 7 is supervisory fault and recovery behavior only. It does not claim
functional-safety certification and does not add hardware safety logic,
automatic process restart, persistence, distributed execution, or GUI/HMI
behavior.

## Later Work

Real serial I/O, persistence, GUI/HMI work, MQTT, TCP, ESP32 integration,
functional-safety implementation/certification, and broader automated recovery
policy remain outside Milestones 1 through 7.
