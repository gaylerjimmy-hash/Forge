# ADR 2001: ADR-008: Durable recovery evidence is versioned, non-authoritative, and reconciled after normal registration

- **Status:** Accepted
- **Date:** 2026-08-20
- **Affected paths:** docs/decisions/ADR-008-durable-recovery-evidence-and-reconciliation.md, include/automation_core/Core/Core.h, src/Core/Core.cpp
- **Supersedes:** None
- **Superseded by:** None
- **Related:** ADR 2000

## Context

Milestone 9 introduces local persistence that must survive restart without restoring live runtime authority, command transactions, process execution, or transport state. Its relationship to the accepted command-transaction authority model must remain explicit.

## Decision

Persist only bounded, checksummed, versioned durable evidence. On restart, restore it transactionally as historical evidence with effective Manual operator mode; require normal Parser, Router, Registry, and HELLO_ACK processing before reconciliation and operator recovery confirmation. Never restore or replay live authority, transactions, process runs, connections, frame buffers, or monotonic deadlines.

## Consequences

- Persistence is a recovery-audit and operator-decision input rather than an authority source.
- Schema evolution and malformed-image behavior are explicit and testable through deterministic load statuses.
- ADR 2000 command transactions remain owned by Core at runtime and are not serialized as live transactions.
- Recovery confirmation is gated by successful normal reconciliation, which may delay operator acknowledgement but prevents stale evidence from becoming authoritative.
