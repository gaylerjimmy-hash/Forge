# Forge OS — Milestone 4 Measurement Ingestion

Forge OS is a C++17 reference implementation for coordinating modular
automation hardware over the Forge Protocol v0.1 line-oriented `KEY=VALUE`
wire format.

The implemented vertical slice is:

```text
Transport
    -> Connection Manager
    -> Frame Assembler
    -> Protocol Parser
    -> Protocol Validator
    -> Message Router
    -> Module Registry
    -> HELLO_ACK / CAPABILITIES_ACK / ERROR
```

`Core::poll_once` wires these components together. `ConsoleTransport` reads
line-oriented frames from standard input and writes protocol responses to
standard output. Lifecycle traces are written to standard error so standard
output remains a machine-readable protocol channel.

## Implemented Milestones

- Milestone 1: module discovery, protocol validation, identity ownership,
  reconnect handling, and quarantine.
- Milestone 2: typed heartbeats, module health, sequence ordering, monotonic
  offline timeouts, and rediscovery.
- Milestone 3: atomic, revisioned capability discovery for measurements,
  commands, and configuration.
- Milestone 4: typed measurement ingestion, capability-aware validation,
  per-capability sequence handling, freshness, invalidation, and lifecycle
  tracing.

Successful heartbeats and measurements are intentionally not acknowledged on
the wire. Rejections produce explicit, correlated `ERROR` messages.

## Build and Test

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The Forge OS reference implementation and Module SDK target C++17. Python is
limited to simulation, tooling, tests, utilities, and clients.

## Current Boundary

Milestone 5 is command transactions. Real serial I/O, process execution,
persistence, GUI/HMI work, MQTT, TCP, and ESP32 integration remain future
work.

The broader Forge v0.1 release definition includes module contracts,
capabilities, measurements, command lifecycle, safety, recovery, simulation,
and HMI behavior. Completing an individual milestone does not imply Forge v0.1
release readiness.
