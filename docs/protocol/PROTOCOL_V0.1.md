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
