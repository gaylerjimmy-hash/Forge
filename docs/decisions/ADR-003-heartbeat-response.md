# ADR-003: Successful Heartbeats Are Not Acknowledged

Status: Accepted

Forge Protocol v0.1 does not send an acknowledgment for a valid heartbeat.
Rejected heartbeats produce a correlated `ERROR` response using `MSG`.

Reason:

- heartbeats are periodic state reports rather than commands
- per-heartbeat acknowledgments add traffic without changing authority
- silence after a valid heartbeat is deterministic
- explicit errors preserve diagnosability
- module presence is determined by subsequent heartbeats and Core timeout
  policy, not acknowledgment delivery
