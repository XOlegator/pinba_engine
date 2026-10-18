# Runtime diagnostics

`SHOW GLOBAL STATUS LIKE 'Pinba_%'` exposes unsigned 64-bit engine counters and
gauges without changing existing virtual-table layouts. Values reset at plugin
initialization. `Pinba_started_unix_us` identifies the current initialization;
`Pinba_uptime_seconds` uses a monotonic clock. A multi-variable read is a live
observation, not a transactional snapshot.

| Variables after `Pinba_` | Meaning |
|---|---|
| `udp_datagrams_received`, `udp_bytes_received` | Every datagram delivered by the socket, including empty/invalid datagrams; payload bytes |
| `udp_decode_errors`, `udp_datagrams_enqueued`, `udp_queue_drops` | Decode failures, decoded roots queued, and decoded roots rejected by queue capacity/allocation |
| `udp_datagrams_completed`, `queued_datagrams_live`, `queued_datagrams_peak` | Decoded trees destroyed after processing; trees currently owned by queues/batches; high-water count |
| `udp_kernel_drops`, `udp_kernel_drop_tracking`, `udp_receive_buffer_bytes` | Linux socket-overflow samples, whether tracking was enabled, and actual SO_RCVBUF value |
| `requests_checked`, `requests_validated`, `requests_invalid` | Individual Request nodes, including descendants; structural validation outcomes |
| `invalid_timer_arrays`, `invalid_flattened_tags`, `invalid_dictionary_index`, `invalid_representation` | Breakdown of structural rejection reasons |
| `requests_staged`, `requests_stage_rejected`, `requests_resource_rejected` | Staging successes, staging capacity/allocation rejects, and preparation resource/admission rejects |
| `requests_admitted`, `requests_capacity_dropped`, `requests_allocation_dropped` | Raw rows published; raw-ring capacity rejects; raw-copy/report-job allocation rejects |
| `requests_live`, `requests_expired`, `expiration_deferred_batches` | Current raw occupancy; expired rows; safe expiration deferrals when report-job buffers are unavailable |
| `timers_admitted`, `timers_skipped`, `timers_allocation_dropped`, `timer_tags_allocation_dropped` | Stored timers; zero-hit/untagged input timers skipped in admitted requests; unavailable timer slots/tags |
| `dictionary_words_live`, `dictionary_words_peak`, `dictionary_words_created`, `dictionary_words_collected` | Allocated interned words, including zero-reference words awaiting collection |
| `dictionary_bytes_known`, `dictionary_bytes_peak` | Word structs plus the two owned NUL-terminated string copies (word and map key); excludes map nodes/buckets and allocator overhead |
| `dictionary_limit_rejected`, `tag_limit_rejected` | Admission refusals by explicitly configured limits |
| `tag_descriptors_live`, `tag_descriptors_created`, `tag_descriptors_collected`, `tag_descriptor_lookups_reused` | Name/ID registry size and lifecycle activity |
| `tag_descriptor_bytes_known` | Descriptor structs only; excludes index allocations |
| `staging_capacity_slots`, `staging_capacity_bytes` | Combined backing allocations for both decoded queues and the staging-record arrays across workers; excludes decoded protobuf trees and per-slot buffers |
| `request_pool_bytes`, `timer_pool_bytes`, `timers_live` | Pool backing storage and live timer occupancy; capacities may exceed live counts |
| `report_count`, `report_rows`, `report_struct_bytes`, `report_cpu_us` | Registered reports, result rows, top-level report struct bytes, and accumulated report CPU microseconds |
| `report_add_candidates`, `report_add_filtered`, `report_delete_candidates`, `report_delete_filtered` | Per-report Request visits and condition-filter exclusions for addition/subtraction; these are not ingestion losses |
| `pipeline_batches`, `pipeline_wall_ns` | Processed batches and total wall time through report updates and decoded-tree destruction |
| `udp_decode_ns`, `request_prepare_ns`, `report_update_ns` | Accumulated stage wall times; parallel worker times are summed |
| `dictionary_write_wait_ns`, `report_lock_wait_ns` | Waits for dictionary/tag insertion upgrades and report-update write locks |
| `dictionary_gc_runs`, `dictionary_gc_ns` | Dictionary sweeps and wall time including lock acquisition |
| `tag_gc_runs`, `tag_gc_ns`, `tag_gc_deferred` | Opt-in descriptor sweeps, total wall time including lock acquisition, and passes deferred after selector-snapshot allocation failure |
| `workers`, `healthy` | Ready collector/data/stats workers (worker-pool threads are additional); no worker has requested shutdown |

For quiescent measurement intervals after all queued batches complete:

```
udp_datagrams_received = udp_decode_errors + udp_queue_drops + udp_datagrams_enqueued
udp_datagrams_enqueued = udp_datagrams_completed + queued_datagrams_live
requests_checked = requests_invalid + requests_validated
requests_validated = requests_staged + requests_stage_rejected + requests_resource_rejected
requests_staged = requests_admitted + requests_capacity_dropped + requests_allocation_dropped
```

Reason counters break down parent counters; do not add them to the parent when
calculating losses. A datagram may contain several Requests. Request counters
count nodes/raw rows, not the protobuf `request_count` weight. Kernel losses are
outside received datagrams; the kernel reports overflow on later delivered
packets, so a quiet socket may not have reported its latest drops yet. Samples
from collectors may arrive out of order; modular deltas tolerate 32-bit wraps
smaller than half the counter range between useful samples.

`status.current_temp_pool_size` now reports `staging_capacity_slots` instead of
the old constant zero, saturating at INT_MAX to preserve its SQL INT layout.
`status.invalid_packets` reports decode errors. Native-width SHOW variables are
the authoritative interface for long-running counters; existing SQL INT columns
cannot represent the full counter range and saturate at INT_MAX. No columns are appended automatically.

Known byte gauges are explicit lower bounds, not RSS or an engine-wide limit.
Report row keys, histograms, protobuf trees, container nodes/buckets and allocator
retention need additional memory. Existing `active_reports` provides per-report
rows and CPU; compare actual server RSS/CPU across controlled workloads rather
than presenting top-level struct bytes as complete report memory.

Allocation failures before raw/report publication reject rows as a complete
batch or compact successful worker reservations. Report-job buffers are prepared
before advancing expiration; failure defers expiry rather than creating ghost
report rows. Expiration holds the collector write lock through subtraction so
new reports cannot outgrow those prepared buffers. SQL latency under expiration
is part of the workload measurements. Timer allocation failure preserves raw
requests and increments explicit timer/tag-loss counters; it does not fabricate
complete timer statistics.

`scripts/runtime_metrics_test.py` verifies the accounting with real datagrams,
structural failures, a nested batch and zero-length input. An optional test-only
failure file exercises mixed raw-copy rejects, failed report-job admission and
deferred/recovered expiration; see `scripts/lifecycle_test.py` for test-build
requirements. Failure controls compile out of ordinary builds.

See [memory admission](memory-admission.md) for opt-in word/byte/name budgets and descriptor collection.
