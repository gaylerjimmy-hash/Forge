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

Complete and covered by Core-level lifecycle, process-impact, authority,
timeout, recovery, duplicate-event, and end-to-end tests:

- typed supervisory fault identities, classes, sources, and lifecycle states
- explicit Core-owned supervisory fault lifecycle
- deterministic active, acknowledged, cleared, and resettable behavior
- generated supervisory faults for authority loss, command failure/timeout,
  process failure, and unavailable operational data
- blocking-fault process inhibition and active-run termination
- explicit reset before manual process restart eligibility
- no automatic restart or hidden recovery loop
- in-memory fault history and correlated traces
- deterministic duplicate and invalid lifecycle operations
- single-terminal-transition process behavior under generated faults

## Milestone 8 — Real Serial Transport

Next implementation milestone:

1. Implement real serial-port open, configure, read, write, and close behavior
   behind the existing transport boundary.
2. Keep platform-specific serial API details inside the serial transport
   implementation; Core, framing, parser, validator, registry, router, and
   process/fault logic must remain transport-agnostic.
3. Support explicit serial configuration including port, baud rate, data bits,
   parity, stop bits, and bounded read/write timing behavior.
4. Make transport reads non-blocking or bounded so Core polling cannot hang on
   an idle or disconnected serial device.
5. Preserve raw byte-stream behavior so Frame Assembler remains the owner of
   framing, frame limits, and frame timeout policy.
6. Handle open failure, configuration failure, disconnect, partial read,
   partial write, write failure, and reconnect/close deterministically.
7. Preserve connection identity semantics required by the existing Connection
   Manager and authoritative module/session ownership model.
8. Avoid hidden reconnect loops; reconnect attempts must be explicit and
   observable to the caller.
9. Add test seams or platform adapters so serial behavior can be regression
   tested without requiring physical hardware for the normal automated suite.
10. Add focused open/configure/read/write/partial-I/O/disconnect/failure tests,
    plus Core-level framing and discovery integration coverage through the real
    serial transport boundary.

Milestone 8 adds the real serial transport boundary only. It does not add
persistent configuration, GUI/HMI behavior, MQTT, TCP, ESP32 firmware,
automatic device discovery, hidden reconnect policy, or functional-safety
behavior.

## Later Work

Persistence, GUI/HMI work, MQTT, TCP, ESP32 integration, automatic device
discovery, functional-safety implementation/certification, and broader
automated recovery policy remain outside Milestones 1 through 8.
