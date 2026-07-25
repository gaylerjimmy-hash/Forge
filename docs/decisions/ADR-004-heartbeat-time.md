# ADR-004: Heartbeat Health Uses Monotonic Time and Serial Arithmetic

Status: Accepted

Forge OS evaluates heartbeat freshness with an injectable monotonic clock.
Wall-clock time is not used for timeout decisions.

Heartbeat sequence values are unsigned 32-bit integers ordered with serial
arithmetic:

- equal values are duplicates
- a modular forward distance from 1 through `2^31 - 1` is newer
- all other non-equal values are stale or out of order
- rollover from `4294967295` to `0` is valid

Uptime is an unsigned 64-bit millisecond count and cannot decrease within an
active session. A session change or uptime regression requires rediscovery.

Reason:

- wall-clock corrections must not create false timeouts
- serial arithmetic handles rollover without special-case reset logic
- session identity separates legitimate reboot recovery from stale traffic
- injectable time makes scheduler and timeout tests deterministic
