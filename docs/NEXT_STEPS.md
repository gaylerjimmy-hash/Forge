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

Next implementation milestone:

1. Define line-oriented `COMMAND`, `COMMAND_ACK`, and `COMMAND_RESULT`.
2. Record command identity, timeout, retry, and terminal-state semantics.
3. Add typed C++17 command and argument models.
4. Implement strict parsing and deterministic serialization.
5. Validate commands against accepted command capabilities.
6. Add a command transaction registry and deterministic state machine.
7. Route commands over the module's authoritative connection.
8. Correlate acknowledgements and results and reject invalid transitions.
9. Add monotonic timeouts, disconnect invalidation, and lifecycle tracing.
10. Add end-to-end coverage and run a clean build and full CTest suite.

Milestone 5 excludes process orchestration. It establishes safe command
transport and transaction lifecycle behavior for a later process engine.

## Later Work

Real serial I/O, process execution, persistence, GUI/HMI work, MQTT, TCP,
ESP32 integration, and the broader safety and recovery model remain outside
Milestones 1 through 5.
