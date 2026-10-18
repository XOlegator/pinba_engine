# Reproducible ingest workload

`scripts/ingest_benchmark.py` runs against an explicitly selected **disposable**
server with the default tables and `pinba_stats_history <= 10`. It creates/drops
its own `_bm_*` reports and waits for raw expiration. Install the optional pinned
[PyMySQL](https://pymysql.readthedocs.io/en/latest/user/examples.html) client in a
virtual environment using `scripts/requirements-benchmark.txt`; the plugin has
no new Python or client dependency.

Example (substitute your private socket, server PID and output path):

```bash
python scripts/ingest_benchmark.py --socket=/path/to/private.sock \
  --server-pid=12345 --udp-port=33002 --label='commit Release compiler flags' \
  --output=knowledge/output/ingest-results.json
```

The default matrix covers low cardinality, unique values, unique timer tag names,
three nested Request nodes, ten timers per node, and 100 distinct active reports.
Each case has three successive history windows, 300 datagrams at 2000/s, and
concurrent SQL reads through **persistent** client connections. Conditions make
every report a separate engine object; table aliases would understate report
cost. Fixtures use deterministic seeds, binary-exact time fractions, independent
count/sum oracles, and SHA-256 fingerprints. The time-sum oracle allows the
existing info-table millisecond truncation plus float conversion error. Every report's request count and time
or timer hit total is checked, together with raw/timer joins and expiration.
Detected loss or incorrect aggregation fails the run rather than producing a
misleading performance comparison.

JSON records the fixture digest, workload, server version/settings, CPU model,
effective affinity, server CPU time and sampled RSS before/peak/after each window.
It includes p50/p95/p99 for a raw query plus a rotating report query, and sampled
UDP-to-SQL visibility upper bounds. SQL timings include driver and wire round trips.
Visibility includes polling/query delay (10 ms observer cadence); these are not
internal stage latency measurements. The reader runs through report subtraction
and collection, so its latency samples include expiration work. Native diagnostics
are captured when available; older builds lacking them remain benchmarkable.
Known byte gauges are lower bounds, not allocator/RSS attribution.

For comparison, run the same matrix in fresh processes with identical server
settings, hardware, compiler/optimization, driver and fixtures. Record the exact
commit and flags in `--label`, and verify fingerprints match. Performance runs
must disable sanitizers and `PINBA_ENABLE_TEST_FAILURES`. Benchmark GC disabled
first for comparable defaults, then enabled separately to evaluate the explicit
catalog-retention tradeoff. Optional limits require a separate admission-loss
experiment; the loss-free oracle intentionally fails if they reject this workload.

This finite, paced workload is a regression baseline, **not saturation throughput**,
a production SLO or a comparison against Pinba2. Repeat noisy results with larger
packet counts/rates that fit the configured raw history/ring; vary history, active
cardinality and report fanout to represent the intended deployment. Process RSS
may retain pool/hash/allocator high-water capacity even when words/descriptors are
collected. Do not interpret a single memory slope or one noisy p99 as proof of a
leak or improvement.
