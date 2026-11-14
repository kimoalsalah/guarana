- CPU: add an experimental, default-off `EDEN_NX2_LSE_HOST_ATOMICS` path for
  scalar ARMv8.1 LSE atomics on x86-64 fastmem with the global monitor ignored.
  Unaligned accesses or fastmem faults restore the original translation only
  at the affected guest instruction; single-stepping keeps the original path.
  No verified performance gain is claimed.
