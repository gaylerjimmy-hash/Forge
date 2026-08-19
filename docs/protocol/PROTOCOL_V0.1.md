# Protocol v0.1

## HELLO

```text
HELLO
MSG=00000001
TYPE=Scale
ID=scale01
FW=1.0.0
PROTO=1
SESSION=81A9C5D2
END
```

## HELLO_ACK

```text
HELLO_ACK
MSG=00000001
STATUS=ACCEPTED
CONNECTION=00042
END
```

## ERROR

```text
ERROR
MSG=00000001
CODE=DUPLICATE_MODULE_ID
DETAIL=scale01 already registered
END
```

## HEARTBEAT

`HEARTBEAT` reports the health of a module that has already completed
discovery. It never registers or rebinds a module implicitly.

```text
HEARTBEAT
MSG=00000042
ID=scale01
SESSION=81A9C5D2
SEQ=1042
UPTIME_MS=381500
STATE=ready
FAULTS=0
END
```

Required fields:

- `MSG`: request correlation identifier
- `ID`: registered module identity
- `SESSION`: active session established by `HELLO`
- `SEQ`: unsigned 32-bit heartbeat sequence
- `UPTIME_MS`: unsigned 64-bit module uptime in milliseconds
- `STATE`: normalized module state
- `FAULTS`: unsigned 32-bit active fault count

Allowed states:

- `booting`
- `initializing`
- `ready`
- `busy`
- `degraded`
- `faulted`
- `updating`

`offline` is assigned by Forge OS and is not accepted as a module-reported
state.

Heartbeat rules:

- The module, session, and authoritative connection must match the registry.
- Successful heartbeats do not receive an ACK.
- Rejected heartbeats receive an explicit correlated `ERROR`.
- Duplicate sequence numbers are ignored safely.
- Sequence ordering uses unsigned 32-bit serial arithmetic. An incoming
  sequence is newer when `(incoming - previous) mod 2^32` is between `1` and
  `2^31 - 1`. Other non-equal values are stale or out of order.
- Sequence rollover from `4294967295` to `0` is valid.
- Uptime must not decrease within a session. Regression indicates a probable
  reboot and requires rediscovery.
- A changed session requires a new `HELLO`; it is not accepted through
  `HEARTBEAT`.
- The default expected interval is 1000 ms and the default offline timeout is
  3000 ms. Timeout evaluation uses Forge OS monotonic time and runs even when
  no transport packet arrives.

## CAPABILITIES

`CAPABILITIES` atomically publishes the complete self-description for an
already registered module.

```text
CAPABILITIES
MSG=00000050
ID=scale01
SESSION=81A9C5D2
REV=1
COUNT=2
ITEM.0.NAME=weight
ITEM.0.TYPE=measurement
ITEM.0.DATA_TYPE=float
ITEM.0.ACCESS=read
ITEM.0.UNIT=lb
ITEM.0.QUALITY=true
ITEM.0.CALIBRATION=true
ITEM.1.NAME=tare
ITEM.1.TYPE=command
ITEM.1.ACCESS=command
END
```

Limits:

- 64 capabilities per module
- 32 characters per capability name
- 16 command arguments
- 32 enum values
- 256 characters per description
- 16384 bytes per capability frame

Indexes are zero-based, contiguous, and must match `COUNT`. Names are unique.
Supported types are `measurement`, `command`, and `configuration`. Supported
data types are `bool`, `int`, `uint`, `float`, `string`, and `enum`. Access
values are `read`, `write`, `read_write`, and `command`.

`DATA_TYPE` is required for measurement and configuration capabilities.
Optional fields are `UNIT`, `MIN`, `MAX`, `QUALITY`, `CALIBRATION`, and
`DESCRIPTION`. `MIN` and `MAX` must appear together and `MIN` must not exceed
`MAX`.

Accepted documents receive `CAPABILITIES_ACK` containing correlated `MSG`,
`ID`, `REV`, and `STATUS=accepted`. Rejected documents receive `ERROR`.

## MEASUREMENT

`MEASUREMENT` publishes one value against an accepted measurement capability.

```text
MEASUREMENT
MSG=00000070
ID=scale01
SESSION=81A9C5D2
CAP=weight
SEQ=2081
VALUE=42.75
UNIT=lb
QUALITY=good
END
```

Required fields are `MSG`, `ID`, `SESSION`, `CAP`, `SEQ`, `VALUE`, and
`QUALITY`. `UNIT` is required when the capability declares one. Optional
fields are `UNCERTAINTY`, `RAW`, and `CAL_REV`.

Published quality values are `good`, `uncertain`, `bad`, `calibrating`,
`out_of_range`, and `unavailable`. `stale` is assigned only by Forge OS.
Values are interpreted strictly from the accepted capability data type.
Sequences use unsigned 32-bit serial arithmetic independently per capability.

Limits:

- capability name: 32 characters
- string value: 256 characters
- enum value: 64 characters
- unit: 32 characters
- default stale timeout: 5000 ms
- uncertainty: finite and nonnegative
- numeric values: finite and within declared capability bounds

Successful measurements are not acknowledged. Rejections return correlated
`ERROR` messages.

## Initial Rules

- Maximum frame size is enforced before parsing.
- The first line identifies the message type.
- `END` terminates the frame.
- Fields declared optional by the applicable schema are ignored safely by a
  receiver that does not consume them.
- Missing required fields are rejected.
- Fields whose required semantics cannot be established from the applicable
  schema and protocol version are rejected; receivers do not guess.
- `HELLO` currently defines no optional fields, so every unrecognized `HELLO`
  field is rejected.
- Unknown message types are rejected explicitly as `UnknownMessageType` during
  parsing. Unsupported typed messages are rejected by the current router with
  the `UNSUPPORTED` error code.
- Unsupported protocol versions are rejected with an explicit
  `PROTO_VERSION` error unless a future message type defines negotiation.
- Duplicate fields are rejected.
- Protocol compatibility is validated before registration.
- Duplicate module identities are quarantined unless proven to be the same module reconnecting.
- Responses correlate to requests using `MSG`.

## Milestone 5 — Command transaction lifecycle

`COMMAND` is emitted only by Forge OS to an active authoritative module.
`TX` is the durable transaction correlation identifier and `MSG` identifies an
individual wire message. `CAP_REV` must equal the accepted capability document
revision and `CAP` must name an accepted command capability.

```text
COMMAND
MSG=cmd-1
TX=tx-1
ID=scale01
SESSION=81A9C5D2
CAP=tare
CAP_REV=4
PAYLOAD=now
END
```

A module replies first with `COMMAND_ACK`. `STATUS` is `ACCEPTED` or
`REJECTED`; rejected acknowledgements include `CODE` and may include `DETAIL`.
Only an accepted acknowledgement permits a final result.

```text
COMMAND_ACK
MSG=ack-1
TX=tx-1
ID=scale01
SESSION=81A9C5D2
STATUS=ACCEPTED
END
```

`COMMAND_RESULT` is terminal. Its `STATUS` is `SUCCESS` or `FAILURE`; it may
carry `RESULT`, `CODE`, and `DETAIL`.

```text
COMMAND_RESULT
MSG=result-1
TX=tx-1
ID=scale01
SESSION=81A9C5D2
STATUS=SUCCESS
RESULT=completed
END
```

The authoritative module identity, session, and connection must match the
transaction for both responses. Duplicate, stale, mismatched, and
invalid-state responses are rejected. Transactions are terminal on rejected
acknowledgement, result, monotonic-clock timeout, or authority loss.

### Milestone 5 clarification

For `COMMAND_ACK`, `STATUS=REJECTED` requires a present, non-empty `CODE`.
A rejected acknowledgement that omits `CODE` or supplies an empty value is
malformed and must be rejected.
