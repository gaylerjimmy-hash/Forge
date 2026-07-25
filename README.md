# Forge OS — Milestone 1 Discovery Slice

Milestone 1 proves one narrow vertical slice:

```text
Transport
    -> Connection Manager
    -> Frame Assembler
    -> Protocol Parser
    -> Protocol Validator
    -> Message Router
    -> Module Registry
    -> HELLO_ACK / ERROR
```

This repository is intentionally skeletal. Most discovery components exist and
are unit-tested independently, but the end-to-end `Core::poll_once` path is not
yet wired.

## Build

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Milestone 1 Acceptance Criteria

Milestone 1 is accepted when the repository provides an end-to-end,
line-oriented discovery path covering:

- HELLO
- HELLO_ACK
- ERROR
- connection IDs
- session IDs
- message IDs
- frame limits
- frame timeout handling
- protocol version validation
- duplicate identity quarantine
- CLI tracing
- exhaustive failure-path tests

Milestone 1 does not include:

- MQTT
- TCP
- persistence
- process engine
- GUI
- command transactions
- capability discovery
- real serial I/O

The broader Forge v0.1 release definition includes module contracts,
capabilities, measurements, command lifecycle, safety, recovery, simulation,
and HMI behavior. Milestone 1 completion does not imply Forge v0.1 release
readiness.
