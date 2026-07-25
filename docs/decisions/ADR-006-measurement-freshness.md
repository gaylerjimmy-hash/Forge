# ADR-006: Measurement Freshness Is Derived from Monotonic Time

Status: Accepted

Forge OS stores published quality separately from effective quality.

- Modules never publish `stale`; Forge derives it from monotonic age.
- The default stale timeout is 5000 ms.
- Duplicate measurements do not refresh freshness.
- Last-known values remain available for diagnostics after timeout or
  disconnect, but are not operationally current.
- Offline, quarantined, new-session, and invalidated-capability states make
  measurements unavailable.
- A new accepted measurement restores effective quality from published
  quality.

Reason:

- clock corrections cannot create false freshness
- replayed packets cannot keep a value current
- diagnostics remain useful without allowing stale data into control logic
