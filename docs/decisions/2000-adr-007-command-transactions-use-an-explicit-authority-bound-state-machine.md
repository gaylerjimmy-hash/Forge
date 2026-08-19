# ADR 2000: ADR-007: Command transactions use an explicit authority-bound state machine

- **Status:** Accepted
- **Date:** 2026-08-18
- **Affected paths:** include/automation_core/Core/Core.h, src/Core/Core.cpp, include/automation_core/Connection/ConnectionManager.h, src/Connection/ConnectionManager.cpp, docs/decisions/ADR-007-command-transaction-lifecycle.md
- **Supersedes:** None
- **Superseded by:** None
- **Related:** None

## Context

Milestone 5 introduces multi-message command execution with asynchronous ACK and RESULT handling, timeouts, and authority-loss invalidation. These lifecycle rules must remain consistent across Core, connection routing, and protocol handling.

## Decision

Represent each command as an explicit transaction state machine owned by Core, bind it to the authoritative module and connection/session at dispatch, use monotonic deadlines, and permit only defined terminal transitions for ACK, RESULT, timeout, rejection, and authority loss.

## Consequences

- Correlation can reject stale, duplicate, mismatched, and invalid-transition responses deterministically.
- Authority replacement or loss has a single defined invalidation path rather than ad hoc pending-command cleanup.
- Core becomes the lifecycle owner, while parser, validator, serializer, and connection layers retain their narrower responsibilities.
- Future retry or cancellation behavior must be introduced as explicit state-machine transitions rather than flags.
