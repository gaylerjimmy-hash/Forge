# Milestone Status

## Implemented and covered by component tests

- `FrameAssembler`
- `Parser`
- `Validator`
- `ConnectionManager`
- `ModuleRegistry`
- `ResponseBuilder`
- `MessageSerializer`
- `MessageRouter`
- `Core::poll_once` orchestration
- `ConsoleTransport` input and output
- Frame assembly timeout enforcement
- CLI lifecycle tracing
- Core-level discovery and failure-path tests
- Reconnect, duplicate identity, and quarantine verification

## Milestone 1 Validation

Milestone 1 implementation is complete when the configured C++17 build, full
CTest suite, and CLI discovery smoke tests pass. Validation results should be
reported with the implementation change.

## Milestone 2 — Module Presence and Health

Implemented and covered by component and Core-level tests:

- Typed `HEARTBEAT` protocol model
- Strict parsing, validation, and serialization
- Normalized module states
- Authoritative connection and session checks
- Duplicate and out-of-order sequence handling
- Unsigned 32-bit sequence rollover
- Uptime regression and probable reboot detection
- Registry health, fault count, and transition records
- Injectable monotonic Core clock
- Idle-tick heartbeat expiration
- Offline rediscovery requirements
- Independent health timelines for multiple modules
- CLI heartbeat and health lifecycle tracing
- Milestone 1 regression coverage

## Milestone 3 — Capability Discovery

Implemented and covered by protocol, registry, router, and Core tests:

- Indexed line-oriented `CAPABILITIES` and `CAPABILITIES_ACK`
- Bounded capability documents and metadata
- Typed capability, data-type, and access models
- Strict structural and semantic validation
- Authoritative connection and session enforcement
- Atomic higher-revision replacement
- Idempotent identical retry
- Stale and same-revision conflict rejection
- Invalid replacement rollback
- Offline, quarantine, and new-session invalidation
- CLI lifecycle tracing and capability summaries

Real serial I/O, measurements, command execution, process execution,
persistence, GUI work, and ESP32 integration remain outside Milestones 1
through 3.
