# Forge Copilot Instructions

## Working model

- Treat the current repository state, build configuration, tests, protocol behavior, and documentation as authoritative evidence. Inspect before editing; do not assume a prior proposed patch or chat change exists.
- Prefer the smallest coherent change that satisfies the requested scope. Do not mix unrelated refactors with feature or repair work.
- Identify the root cause before repairing a failure. Do not weaken tests merely to accommodate incorrect runtime behavior.
- Preserve accepted behavior and architecture unless the requirement or repository evidence justifies changing them.
- Keep the requested milestone or task boundary authoritative. Do not silently broaden scope or implement future work.

## Architecture and protocol

- Forge OS is a C++17 automation core. Keep the core transport-independent above the transport boundary.
- Preserve the existing layered flow and ownership boundaries among transport, connection management, framing, protocol parsing/validation, routing, registry/state, response serialization, and Core orchestration.
- Keep protocol handling deterministic and explicit. Reject invalid or unauthorized input rather than silently accepting or repairing it.
- Preserve identity, sequence, capability, transaction, process, fault, timeout, recovery, and correlation semantics unless an explicit requirement changes them.
- Use monotonic time for elapsed-time, timeout, freshness, and deadline behavior.
- Keep standard output machine-readable where it is used as the protocol channel; diagnostics and lifecycle traces belong on standard error.
- Do not couple higher layers to a specific transport when the existing abstraction can carry the behavior.
- Functional-safety claims require explicit requirements and evidence. Do not infer that ordinary supervisory fault handling is a certified safety function.

## C++ and tests

- Target C++17. Do not introduce a newer language requirement without explicit approval.
- Preserve warning-clean builds under the repository's configured compiler warnings.
- Add focused regression coverage for confirmed bugs when practical.
- Tests should exercise observable behavior and invariants rather than implementation accidents.
- Do not claim a build or test passed unless it actually ran successfully.

## Validation

- Use the repository-native CMake/CTest workflow from the repository root:
  `cmake -S . -B build`
  `cmake --build build`
  `ctest --test-dir build --output-on-failure`
- For a code change, run relevant focused tests during implementation when useful, then run the complete repository-native build and CTest suite before declaring the task complete.
- If validation fails, diagnose and repair the actual failure. Do not alter tests solely to make incorrect runtime behavior pass.
- Inspect the final diff after validation and exclude generated build artifacts from source changes.

## Documentation and completion

- Keep README and protocol/milestone documentation consistent with implemented behavior when a change affects them.
- Do not encode transient milestone status into durable engineering instructions; derive current status from repository state and task-specific documentation.
- Before declaring implementation complete, inspect the final diff for unrelated changes, generated artifacts, stale documentation, and accidental API/protocol changes.
- Report what changed, what validation actually ran, its result, and any remaining risk.
- Do not commit, push, merge, rewrite history, or delete branches unless the user explicitly requests the consequential Git action.
