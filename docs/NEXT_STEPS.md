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

Next implementation milestone:

1. Define typed process, run, and step identities.
2. Add an explicit non-blocking process execution state machine.
3. Define ordered command steps and measurement-condition wait steps.
4. Keep process coordination in Core-side orchestration; modules continue to
   own hardware and local protective behavior.
5. Dispatch command steps only through the accepted Milestone 5 command
   transaction boundary.
6. Evaluate measurement conditions only against operationally current accepted
   measurement state.
7. Use monotonic time for step and run deadlines; do not use blocking delays.
8. Handle command rejection, failure, timeout, authority loss, stale or
   unavailable measurements, and explicit abort deterministically.
9. Correlate process, step, and command transaction traces.
10. Add focused state-machine, failure-path, timeout, abort, and end-to-end
    tests and run a clean build and full CTest suite.

Milestone 6 does not add automatic retry, persistence, distributed execution,
GUI/HMI behavior, or hardware safety logic. Retry or recovery policy must be
introduced later as explicit state-machine behavior rather than hidden loops.

## Later Work

Real serial I/O, persistence, GUI/HMI work, MQTT, TCP, ESP32 integration,
and the broader safety and recovery model remain outside Milestones 1 through
6.
