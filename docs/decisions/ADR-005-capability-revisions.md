# ADR-005: Capability Documents Replace Atomically by Revision

Status: Accepted

A capability publication is the module's complete document, not a patch.

- The first valid document is accepted.
- An identical document at the same revision is idempotent.
- Changed content at the same revision is rejected.
- A lower revision is rejected as stale.
- A higher valid revision replaces the previous document atomically.
- Invalid replacements leave the previous accepted document unchanged.
- A new module session invalidates the previous document.
- Offline modules retain last-known metadata for diagnostics, but it is not
  operationally available.

Reason:

- consumers never observe partial capability updates
- revisions make retries and replacement deterministic
- session invalidation prevents stale firmware contracts from remaining active
