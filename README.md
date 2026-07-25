# Forge OS — Milestone 3 Capability Discovery

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

This repository is intentionally skeletal. The discovery components are wired
through `Core::poll_once`, and `ConsoleTransport` can read line-oriented frames
from standard input and write responses to standard output. The CLI writes
discovery lifecycle traces to standard error so standard output remains a
machine-readable protocol channel. It processes frames until standard input
reaches EOF and traces heartbeat health transitions without acknowledging
successful heartbeats on the wire.

Milestone 2 extends that discovery slice with typed `HEARTBEAT` messages,
authoritative registry health, deterministic sequence and uptime validation,
monotonic offline timeouts, rediscovery requirements, and health lifecycle
tracing.

Milestone 3 adds atomic, revisioned capability discovery so modules can
self-describe measurements, commands, and configuration without module-type
branches in Forge OS.

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
