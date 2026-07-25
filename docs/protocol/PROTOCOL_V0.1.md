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
