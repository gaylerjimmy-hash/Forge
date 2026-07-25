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

Real serial I/O, capabilities, measurements, commands, process execution,
persistence, GUI work, and ESP32 integration remain outside Milestones 1 and
2.
