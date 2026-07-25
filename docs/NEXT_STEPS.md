# Milestone 1 Next Steps

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

Real serial I/O, capabilities, measurements, commands, process execution,
persistence, GUI work, and ESP32 integration remain outside Milestone 1.
