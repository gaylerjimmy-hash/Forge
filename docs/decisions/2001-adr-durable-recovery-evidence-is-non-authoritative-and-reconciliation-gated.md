# ADR 2001: ADR: Durable recovery evidence is non-authoritative and reconciliation-gated

- **Status:** Accepted
- **Date:** 2026-08-20
- **Affected paths:** include/automation_core/Persistence/*.h, src/Persistence/*.cpp, include/automation_core/Core/Core.h, src/Core/Core.cpp, docs/decisions/
- **Supersedes:** None
- **Superseded by:** None
- **Related:** ADR 2000

## Context

Milestone 9 introduces persistent local recovery records that span process lifetimes while existing architecture assigns live module, command, and process authority to Core, Registry, and ADR-007 transaction ownership.

## Decision

Persist only versioned historical evidence. On restart, apply validated evidence transactionally without reconstructing live authority; force effective Manual mode and gate recovery confirmation on a new normal HELLO reconciliation through Parser, Router, Registry, and HELLO_ACK.

## Consequences

- Persistence records cannot resume commands, processes, deadlines, transports, or registry authority.
- Schema evolution, bounded decoding, integrity checking, and deterministic rejection statuses become an explicit compatibility boundary.
- Historical identity/session comparisons remain observational and cannot override current Registry authority.
- Recovery-required state remains possible after a syntactically valid restart when evidence is ambiguous.
