- Diagnostics: add `EDEN_NX2_MEMORY_ATTRIBUTION=1` (off by default) to log a single-line MiB memory
  attribution breakdown every 10 seconds and at renderer shutdown across process, guest, texture and
  buffer caches, GPU retirement queues, staging/transcode pools, shader/pipeline maps, and the
  driver pipeline cache blob ([#136](https://github.com/karimalsalah/guarana/issues/136)).
