# Automation Core — Engineering Guidelines for Codex

## 1. Project Purpose

Automation Core is a modular, transport-independent automation platform.

Its purpose is to let independent hardware modules connect to a central Core, describe themselves, expose capabilities, publish measurements, accept commands, and participate in coordinated processes without requiring application-specific wiring inside the Core.

Hydroponics is the first reference implementation, not the architectural boundary.

The platform should eventually support modules such as:

- Scale
- Chemistry
- Pump
- Valve
- Flow
- Lighting
- Motion
- Environmental sensing
- PLC gateway
- Camera
- Alarm
- Human interface
- Future custom modules

The Core shall coordinate modules through contracts. Modules shall own hardware. Processes shall own sequence logic.

## 2. Foundational Principle

The most important concept is not modularity.

It is contracts.

Every module shall implement a predictable contract for:

- Identity
- Capabilities
- Measurements
- Commands
- State
- Faults
- Acknowledgments
- Completion
- Heartbeat
- Configuration
- Calibration

The Core shall never need to know how a module performs its work internally.

A process shall ask for a capability, not a specific GPIO, board, library, or vendor device.

## 3. Architectural Rules

### 3.1 Modules Own Hardware

A module owns:

- Sensors
- Actuators
- Low-level drivers
- Calibration
- Local filtering
- Local timing
- Hardware-specific safety behavior
- Hardware faults

Examples:

- A scale module owns the HX711 and load cells.
- A chemistry module owns the pH, EC/TDS, and temperature interfaces.
- A pump module owns relay outputs, motor timing, and pump calibration.
- A PLC gateway owns PLC communication and tag mapping.

The Core shall not toggle module GPIO directly.

### 3.2 Modules Never Control Other Modules

A module may publish measurements, accept commands, reject unsafe commands, report state, and report faults.

A module shall not command another module directly.

Only the Core coordinates modules.

### 3.3 Processes Never Know Hardware

A process may request an action such as opening a fill path, dosing a volume, reading a measurement, waiting for a stable state, or verifying a condition.

A process shall not know pin numbers, relay polarity, serial port names, sensor libraries, device addresses, board type, or vendor API details.

### 3.4 Transport Is an Implementation Detail

The same logical protocol shall work over UART, USB serial, RS-485, CAN, Ethernet, Wi-Fi, TCP, WebSocket, and future transports.

Transport adapters shall be isolated from protocol logic.

### 3.5 The HMI Is Not the Controller

The HMI may display state, configure values, submit commands, show alarms, show logs, and start or pause processes.

The HMI shall not contain critical control logic.

Closing the HMI shall not leave hardware in an unsafe state.

## 4. Core Responsibilities

### 4.1 Transport Manager

Responsibilities:

- Open and close transport connections
- Read and write frames
- Detect disconnects
- Reconnect where permitted
- Maintain transport health
- Avoid transport-specific logic elsewhere

### 4.2 Protocol Parser

Responsibilities:

- Parse incoming frames
- Validate schema
- Reject malformed messages
- Normalize message types
- Reject missing fields and fields with unknown required semantics
- Ignore schema-declared optional fields safely when they are not consumed
- Reject unknown message types with an explicit error
- Enforce protocol version rules

The parser shall not perform process logic.

### 4.3 Module Registry

Responsibilities:

- Track connected modules
- Track identity
- Track capabilities
- Track firmware version
- Track protocol version
- Track last heartbeat
- Track measurements
- Track measurement quality and age
- Track module state
- Track active faults
- Track calibration validity
- Mark modules offline when heartbeat expires

The registry shall be the authoritative view of current module availability.

### 4.4 Command Router

Responsibilities:

- Generate unique command IDs
- Route commands to target modules
- Track acknowledgment
- Track completion
- Track rejection reason
- Enforce timeout
- Prevent duplicate active commands
- Support idempotent retry where defined

No command shall be considered successful solely because it was transmitted.

### 4.5 Safety Manager

Responsibilities:

- Maintain system safety state
- Evaluate interlocks
- Gate actuator commands
- Latch critical faults
- Require explicit reset where appropriate
- Prevent conflicting actions
- Define fail-safe behavior
- Coordinate shutdown on communication loss

The Safety Manager shall be authoritative.

All process-critical commands must pass through it.

### 4.6 Process Engine

Responsibilities:

- Execute explicit state machines
- Request capabilities
- Wait for acknowledgments and completion
- Verify physical response
- Apply timeouts
- Pause, abort, and recover
- Log state transitions
- Never bypass the Safety Manager

### 4.7 Event Broker

Responsibilities:

- Publish module events
- Publish measurement updates
- Publish fault changes
- Publish command lifecycle events
- Publish process transitions
- Allow HMI and logging subscribers
- Avoid tight coupling between subsystems

### 4.8 Configuration Manager

Responsibilities:

- Load configuration
- Validate configuration
- Version configuration
- Apply defaults safely
- Persist changes
- Separate hardware, safety, process, and UI configuration
- Reject invalid configuration before automatic operation

### 4.9 Logger

Responsibilities:

- Structured event logging
- Measurement history
- Fault history
- Command history
- Process history
- Configuration changes
- Calibration events
- Operator actions

Logging shall not block control execution.

## 5. Required Module Contract

Every module shall support a minimum common contract.

### 5.1 Identity

Required fields:

- module_id
- module_type
- protocol_version
- firmware_version
- hardware_version if available
- manufacturer or project namespace
- serial number if available
- boot_id
- uptime

Forge Protocol v0.1 represents initial identity with the line-oriented
`HELLO` message:

```text
HELLO
MSG=00000001
TYPE=Scale
ID=scale_01
FW=0.1.0
PROTO=1
SESSION=5FBC2C1E
END
```

Additional identity fields belong to a future protocol revision and shall not
be added to v0.1 without an explicit schema and compatibility decision.

### 5.2 Capabilities

A capability describes what the module can measure or do.

Each capability should include:

- name
- capability_type
- data type
- unit
- read/write/command access
- valid range
- quality support
- calibration support
- command arguments
- safety constraints where relevant

Capability discovery is part of the broader Forge v0.1 release definition,
not Forge Protocol v0.1 or Milestone 1. Its line-oriented message schema must
be specified before implementation; JSON is not the Forge wire protocol.

### 5.3 Measurements

Required fields:

- module_id
- name
- value
- unit
- timestamp or sequence
- quality

Optional fields:

- uncertainty
- raw value
- filtered value
- calibration reference
- metadata

Approved quality values:

- good
- uncertain
- bad
- stale
- calibrating
- out_of_range
- unavailable

### 5.4 Module State

Approved minimum states:

- booting
- initializing
- ready
- busy
- degraded
- faulted
- offline
- updating

Modules may define additional states, but the Core must map them to a common parent state.

### 5.5 Commands

Every command shall include:

- command_id
- target
- name
- arguments
- issued_at if supported
- timeout if applicable

Modules shall validate arguments, validate current state, validate local interlocks, reject unsupported or unsafe commands explicitly, acknowledge accepted commands, and report completion or failure.

### 5.6 Acknowledgment

Acknowledgment means the module accepted responsibility for processing the command.

It does not mean the command completed successfully.

Required fields:

- command_id
- accepted
- reason if rejected

### 5.7 Completion

Completion means execution ended.

Required fields:

- command_id
- result
- details

Approved result values:

- success
- failed
- aborted
- timed_out
- partial

### 5.8 Faults

Required fields:

- module_id
- fault_code
- severity
- active
- latched
- message
- timestamp or sequence

Approved severities:

- info
- warning
- fault
- critical

Fault codes shall be machine-readable and stable across firmware versions.

### 5.9 Heartbeat

Heartbeat shall include:

- module_id
- state
- uptime
- boot_id
- sequence
- active fault count

The Core shall mark a module offline after a configurable heartbeat timeout.

## 6. Command Lifecycle

The required lifecycle is:

```text
Core creates command
    ↓
Command transmitted
    ↓
Module ACKs or rejects
    ↓
Module executes
    ↓
Module reports COMPLETE or FAILED
    ↓
Core verifies physical response if required
```

The Core shall track the following states:

- created
- transmitted
- acknowledged
- rejected
- executing
- completed
- failed
- timed_out
- cancelled

A rejected command must be surfaced immediately to the process engine.

It shall not degrade into a generic acknowledgment timeout.

## 7. Safety Requirements

### 7.1 Safe Boot

All modules shall initialize outputs to safe states before starting communication or application logic.

Examples:

- Pumps off
- Valves closed
- Motion disabled
- Heaters off
- Relays de-energized unless fail-safe design requires otherwise

Automatic operation shall default OFF after boot unless an explicitly approved restart policy exists.

### 7.2 Communication Loss

On loss of Core communication:

- Active timed operations shall terminate safely
- Continuous actuator commands shall expire
- Modules shall enter a defined local safe state
- No automatic restart shall occur without policy approval
- Fault state shall be reported when communication returns

### 7.3 Local Safety Authority

A module may reject a command even if the Core requested it.

Examples:

- Pump runtime limit exceeded
- Valve interlock active
- Calibration invalid
- Local temperature too high
- Emergency input active
- Motion limit active

The Core shall treat local rejection as authoritative.

### 7.4 Hardwired Safety

Where practical, critical protection shall not rely solely on software.

Examples:

- E-stop
- High-high level float
- Motor overload
- Thermal cutout
- Pressure relief
- Mechanical overflow
- Limit switch chain

Automation Core may monitor such devices, but shall not replace required hardwired protection.

### 7.5 Fault Latching

Critical faults shall latch.

Reset shall require the fault condition to be cleared, the module to be ready, an explicit reset request, and Safety Manager approval.

## 8. Process Engine Rules

All automated sequences shall be explicit state machines.

Each state shall define:

- Entry action
- Expected module state
- Expected measurement response
- Completion condition
- Timeout
- Fault transition
- Abort behavior
- Recovery behavior

No hidden background action may bypass the process engine.

Example:

```text
State: REFILLING

Entry:
- Request fill capability
- Wait for ACK
- Wait for command completion policy

Expected response:
- Flow becomes present
- Reservoir weight rises

Success:
- Target volume reached
- Fill command stopped
- Stable verification passes

Fault:
- No flow
- No weight change
- High-high input
- Timeout
- Module offline
```

Timers shall advance from the Core scheduler, not only when sensor messages arrive.

## 9. Capability-Based Design

Processes shall request capabilities rather than fixed module IDs when possible.

Example requirement set:

- One reservoir volume measurement
- One refill actuator
- One drain actuator
- Two calibrated dosing channels
- One pH measurement

The Core resolves available modules that satisfy the requirement.

Binding may be automatic, configuration-based, or operator-selected.

The binding result shall be visible and logged.

## 10. Self-Describing HMI

The HMI should be able to generate controls from module metadata.

Examples:

- Numeric measurement → gauge or value card
- Boolean state → indicator
- Command with no arguments → button
- Command with numeric argument → input plus execute button
- Calibratable measurement → calibration panel
- Fault capability → alarm view
- Enumerated configuration → dropdown
- Valid range → input limits

The HMI may provide custom views for known module types, but generic operation must remain possible.

## 11. SDK Requirements

The first SDK target should be ESP32.

The SDK should allow module authors to write code resembling:

```cpp
AutomationModule module("scale_01", "scale");

module.addMeasurement("weight_lb", "lb");
module.addMeasurement("volume_gal", "gal");

module.addCommand("tare", handleTare);
module.addCommand("calibrate_span", handleSpan);

module.publish("weight_lb", weight, Quality::Good);
module.setState(ModuleState::Ready);
```

The SDK shall handle:

- Message framing
- Serialization
- Identity
- Capabilities
- Heartbeat
- Command parsing
- ACK
- Completion
- Fault publication
- Configuration storage
- Version negotiation
- Duplicate command detection
- Safe communication timeout callbacks

The SDK shall not hide application-specific safety logic.

## 12. Protocol Design Rules

### 12.1 Line-Oriented Protocol

Forge Protocol v0.1 uses the existing human-readable, line-oriented
`KEY=VALUE` wire format.

- The first line identifies the message type.
- Fields occupy one line each as `KEY=VALUE`.
- `END` terminates the message.
- Message-specific schemas define required and optional fields.
- Binary encoding shall not replace this format without measurements showing
  the need and an explicit protocol-version decision.

### 12.2 Versioning

Every module and Core shall report protocol version.

Compatibility rules shall define major version incompatibility, minor version compatibility, optional fields, deprecated fields, and unknown message handling.

For the line-oriented protocol:

- Fields declared optional by the applicable message schema shall be ignored
  safely when the receiving subsystem does not consume them.
- Missing required fields shall be rejected.
- A field whose required semantics cannot be established from the applicable
  schema and negotiated protocol version shall be rejected; the receiver shall
  not guess.
- Unknown message types shall be rejected with an explicit error.
- Unsupported protocol versions shall be rejected unless a message type
  explicitly defines version negotiation.

Forge Protocol v0.1 currently defines no optional `HELLO` fields. Therefore,
an unrecognized `HELLO` field is rejected by the current implementation. This
is consistent with rejecting unknown required semantics and does not authorize
silent acceptance of arbitrary extension fields.

### 12.3 Validation

Reject:

- Missing required fields
- Invalid types
- Unsupported commands
- Invalid ranges
- Duplicate active command IDs
- Unsupported protocol versions
- Malformed frames

Never silently guess missing safety-critical data.

### 12.4 Idempotency

Commands shall declare whether they are:

- Idempotent
- Non-idempotent
- Retry-safe with command ID
- Not retry-safe

A repeated non-idempotent command must not execute twice simply because an ACK was lost.

## 13. Concurrency and Scheduling

The Core shall use a deterministic scheduler or event loop.

Requirements:

- No blocking I/O in control paths
- No long sleeps in process logic
- Bounded work per iteration
- Transport receive queues
- Command timeout checks
- Heartbeat timeout checks
- Process ticks at a fixed cadence
- Logging on a separate queue or worker
- HMI updates decoupled from control execution

Thread boundaries shall be explicit.

Shared mutable state shall be minimized.

## 14. Configuration Structure

Recommended structure:

```text
config/
├── core.json
├── transports.json
├── modules.json
├── bindings.json
├── safety.json
├── processes/
├── recipes/
└── ui.json
```

Configuration shall include:

- schema_version
- revision
- validation status
- source
- last_modified
- safe defaults

The system shall refuse automatic operation with invalid safety-critical configuration.

## 15. Logging and Observability

Every important event shall be traceable.

Log:

- Module connect/disconnect
- Identity changes
- Boot ID changes
- Capability changes
- Command created
- Command ACK
- Command rejection
- Command completion
- Command timeout
- Fault raised
- Fault cleared
- Fault reset
- Process transition
- Safety transition
- Configuration change
- Calibration event
- Operator action
- Core restart

Use structured logs with fields such as timestamp, severity, source, module_id, process_id, command_id, event_type, message, and details.

## 16. Persistence and Recovery

The Core shall persist enough state to determine what happened before a restart.

Persist:

- Last known module identities
- Active process
- Last completed state
- Pending command records
- Active latched faults
- Configuration revision
- Calibration metadata
- Operator mode

On restart:

- Do not assume pending commands completed
- Re-discover modules
- Compare boot IDs
- Reconcile actual state
- Enter recovery or paused mode
- Require operator confirmation where unsafe ambiguity exists

## 17. Testing Requirements

### 17.1 Unit Tests

Test:

- Protocol parsing
- Schema validation
- Registry freshness
- Module offline detection
- Command lifecycle
- Duplicate command handling
- Fault latching
- Safety transitions
- Process transitions
- Configuration validation

### 17.2 Integration Tests

Simulate:

- Module discovery
- Command ACK and completion
- Rejected command
- Lost ACK
- Lost completion
- Module reboot
- Core reboot
- Stale data
- Bad quality data
- Conflicting commands
- Communication loss
- Transport reconnect

### 17.3 Plant Simulation

Build a simulated process environment supporting virtual sensors, virtual actuators, physical response over time, noise, drift, stuck values, delayed response, wrong-direction response, module dropout, and fault injection.

The simulator shall run the real Core and process engine.

### 17.4 Hardware-in-the-Loop

Before production release:

- Connect real ESP32 modules
- Simulate sensors where necessary
- Test fail-safe outputs
- Disconnect transports during operation
- Reboot modules mid-command
- Reboot Core mid-process
- Verify no unsafe restart

## 18. Repository Structure

Recommended initial structure:

```text
automation-core/
├── README.md
├── docs/
│   ├── Constitution.md
│   ├── Protocol_v0.1.md
│   ├── Module_Contract.md
│   ├── Safety_Model.md
│   ├── Process_Engine.md
│   └── SDK_Guide.md
├── core/
│   ├── transport/
│   ├── protocol/
│   ├── registry/
│   ├── commands/
│   ├── safety/
│   ├── process/
│   ├── events/
│   ├── config/
│   └── logging/
├── sdk/
│   ├── esp32/
│   └── simulator/
├── modules/
│   ├── scale/
│   ├── chemistry/
│   ├── pump/
│   └── flow/
├── processes/
│   └── hydro/
├── ui/
├── simulator/
├── tests/
│   ├── unit/
│   ├── integration/
│   └── hil/
└── examples/
```

## 19. Coding Standards

### Authoritative Languages

- Forge OS reference implementation language: C++17
- Module SDK target language: C++17
- Python may be used for simulation, testing tools, utilities, and clients.

- Forge OS and Module SDK production code shall compile as standard C++17.
- Compiler-specific extensions shall not be required.
- A newer C++ standard shall not be adopted without an explicit architectural
  decision and corresponding build, toolchain, and compatibility updates.
- Python components shall not replace or redefine the authoritative Forge OS
  or Module SDK contracts.

### General

- Prefer explicit code over clever code
- Use stable names
- Avoid hidden global state
- Avoid magic numbers
- Document failure behavior
- Use typed structures where available
- Validate all external input
- Keep modules small and testable
- Preserve backward compatibility deliberately, not accidentally

### C++17 Core

- Use explicit ownership and RAII
- Prefer value types and deterministic lifetime management
- Use smart pointers only when ownership cannot be expressed by values or
  references
- Avoid owning raw pointers
- Use typed structures and scoped enumerations for protocol and state models
- Keep public interfaces small and const-correct
- Use exceptions only where failure cannot be represented clearly in a result
  type; never use exceptions for routine protocol validation
- Separate transport, protocol, registry, safety, and process logic
- Never swallow exceptions silently
- Keep thread ownership and synchronization boundaries explicit
- Use queues or event dispatch between asynchronous subsystems
- Log rejected and malformed messages
- Keep process transitions deterministic
- Compile with strict warnings and treat new warnings as defects

### ESP32 Firmware

- No long blocking delays
- Use `millis()`-based state machines or RTOS tasks carefully
- Initialize outputs safe
- Use watchdogs
- Keep communication responsive during actuator operation
- Separate hardware driver from module contract layer
- Use bounded buffers
- Validate persisted configuration
- Store calibration with version and checksum
- Report reset reason where possible

## 20. Anti-Patterns to Reject

Codex shall not:

- Put hardware pin logic in the Core
- Put process logic in the HMI
- Let modules command other modules
- Treat transmit as success
- Treat ACK as completion
- Ignore command rejection reason
- Use blocking delays in control paths
- Hide faults with broad exception handlers
- Add binary protocol complexity without evidence
- Add cloud dependence to core safety behavior
- Require internet access for local operation
- Rewrite working subsystems without tests
- Add features before safety and lifecycle correctness
- Hardcode hydroponics assumptions into generic Core components
- Couple process timing to sensor message arrival
- Allow stale data to be treated as valid
- Automatically resume ambiguous interrupted processes

## 21. Initial Implementation Plan

### Phase 1 — Constitution and Contract

1. Create project structure.
2. Write `Constitution.md`.
3. Write protocol message schemas.
4. Define common enums.
5. Define module identity and capability models.
6. Define command lifecycle.

### Phase 2 — Minimal Core

1. Implement one transport adapter.
2. Implement parser and validator.
3. Implement module registry.
4. Implement heartbeat timeout.
5. Implement command router.
6. Implement structured logging.

### Phase 3 — First Module

Use the scale module as the first proof of concept.

Required behavior:

- Discovery
- Identity
- Capabilities
- Weight publication
- Quality
- Tare
- Span calibration
- Heartbeat
- Fault reporting

### Phase 4 — Safety and Process

1. Implement Safety Manager.
2. Require all commands to pass through it.
3. Implement generic state machine framework.
4. Add process timeout support.
5. Add abort and recovery.
6. Add integration tests.

### Phase 5 — Generic HMI

1. Show registry.
2. Show measurements.
3. Show quality and freshness.
4. Show module state.
5. Show faults.
6. Generate generic command controls from capabilities.

### Phase 6 — Hydro Process Package

Move hydro-specific logic into:

```text
processes/hydro/
```

Hydro shall consume generic capabilities.

Hydro logic shall not modify generic Core code unless a genuinely reusable requirement is discovered.

## 22. Milestone 1 Acceptance Criteria

Milestone 1 is the narrow Forge Protocol v0.1 discovery slice. It is accepted
when:

- One transport can deliver a complete line-oriented frame to Forge OS.
- Frame size, termination, and malformed-frame failures are enforced.
- `HELLO` is parsed and validated against protocol version 1.
- Connection ID, session ID, and message ID semantics are enforced.
- A valid module identity is registered.
- Duplicate identity behavior follows ADR-002.
- `HELLO_ACK` and explicit `ERROR` responses correlate through `MSG`.
- Unsupported message types and protocol versions produce explicit errors.
- The CLI demonstrates the complete discovery path.
- Unit and end-to-end failure-path tests pass.

Milestone 1 does not require capability discovery, measurements, heartbeat,
commands, process execution, persistence, a GUI, a real serial adapter, or an
ESP32 module.

## 23. Broader Forge v0.1 Release Definition

The Forge v0.1 release is broader than Milestone 1. Milestone 1 completion does
not imply Forge v0.1 release readiness.

Forge v0.1 is complete when:

- Modules can connect and self-identify
- Capabilities can be discovered
- Measurements include quality and freshness
- Commands have full lifecycle tracking
- Rejections are immediate and explicit
- Critical faults latch
- Modules fail safe on communication loss
- Core detects module reboot and disconnect
- Process timing is independent of message arrival
- Safety Manager is authoritative
- One real ESP32 scale module works end to end
- One simulated module works end to end
- Generic HMI renders both
- Integration tests cover disconnect, restart, rejection, timeout, and recovery
- Hydro-specific code remains outside generic Core components
- Documentation matches implementation

## 24. Codex Working Rules

When modifying this project:

1. Inspect existing code before changing it.
2. Preserve working behavior.
3. Make small, reviewable commits.
4. Add or update tests with every behavioral change.
5. Do not silently change protocol fields.
6. Do not add new dependencies without justification.
7. Do not bypass the Safety Manager.
8. Do not duplicate registry state in multiple subsystems.
9. Do not catch exceptions without logging and handling them.
10. Stop and report architectural conflicts instead of forcing a workaround.
11. Prefer boring, explicit code over elegant hidden behavior.
12. Keep generic Core code independent of hydroponics.
13. Treat documentation as part of the implementation.
14. Do not claim completion unless tests pass.
15. Clearly identify unimplemented safety behavior.

## 25. Engineering Intent

Automation Core is not a collection of device drivers.

It is a contract-driven coordination platform.

The design succeeds when:

- Hardware can be replaced without rewriting processes
- Transports can be replaced without rewriting modules
- The HMI can interpret modules from metadata
- Processes can coordinate capabilities without knowing implementation details
- Faults are explicit
- Command state is never ambiguous
- Safety does not depend on the UI
- The platform remains useful outside hydroponics

When forced to choose between convenience and explicit behavior, choose explicit behavior.

When forced to choose between cleverness and testability, choose testability.

When forced to choose between adding a feature and closing a safety gap, close the safety gap.
