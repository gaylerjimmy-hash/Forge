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

## Remaining for Milestone 1

1. Implement the `Core::poll_once` orchestration path:
   transport receive, connection activity, frame assembly, parsing, routing,
   serialization, and transport send.
2. Make `ConsoleTransport` provide usable input for the discovery
   demonstration.
3. Add CLI tracing for connection, parsing, validation, registration, and
   response outcomes.
4. Complete frame-timeout behavior; the timeout is configured but not yet
   applied by the current assembler.
5. Add end-to-end tests for valid discovery and every failure response.
6. Verify duplicate identity quarantine and reconnect behavior through the
   complete Core path.
7. Run the full test suite under a configured C++17 toolchain and keep it
   passing.

Real serial I/O, capabilities, measurements, commands, process execution,
persistence, GUI work, and ESP32 integration remain outside Milestone 1.
