# Pinba Engine Table Types

Every PINBA table is a virtual view over the in-memory statistics aggregated by the engine.
The table type is specified in the `COMMENT` of the `CREATE TABLE` statement.
The engine ignores the physical column names and maps columns to statistics by their **ordinal position**.

## Core tables

These tables are created by `default_tables.sql` and are always present in a working setup.

| Table name | COMMENT | Description |
|---|---|---|
| `request` | `request` | Ring buffer of raw incoming requests |
| `timer` | `timer` | Per-request timer data |
| `timertag` | `timertag` | Tag key-value pairs attached to each timer |
| `tag` | `tag` | Tag name dictionary |
| `info` | `info` | Aggregated engine-wide counters for the current time window |
| `status` | `status` | Engine status: pool sizes, error counts, plugin version |
| `active_reports` | `active_reports` | List of currently active report objects |

## Standard report tables (`report1`–`report18`)

These tables are also created by `default_tables.sql`.
Each groups requests by a fixed combination of built-in fields.

| COMMENT | Friendly name | Key fields |
|---|---|---|
| `report1` | `report_by_script_name` | `script_name` |
| `report2` | `report_by_server_name` | `server_name` |
| `report3` | `report_by_hostname` | `hostname` |
| `report4` | `report_by_server_and_script` | `server_name`, `script_name` |
| `report5` | `report_by_hostname_and_script` | `hostname`, `script_name` |
| `report6` | `report_by_hostname_and_server` | `hostname`, `server_name` |
| `report7` | `report_by_hostname_server_and_script` | `hostname`, `server_name`, `script_name` |
| `report8` | `report_by_status` | `status` |
| `report9` | `report_by_script_and_status` | `script_name`, `status` |
| `report10` | `report_by_server_and_status` | `server_name`, `status` |
| `report11` | `report_by_hostname_and_status` | `hostname`, `status` |
| `report12` | `report_by_hostname_script_and_status` | `hostname`, `script_name`, `status` |
| `report13` | `report_by_schema` | `schema` |
| `report14` | `report_by_script_and_schema` | `script_name`, `schema` |
| `report15` | `report_by_server_and_schema` | `server_name`, `schema` |
| `report16` | `report_by_hostname_and_schema` | `hostname`, `schema` |
| `report17` | `report_by_hostname_script_and_schema` | `hostname`, `script_name`, `schema` |
| `report18` | `report_by_hostname_status_and_schema` | `hostname`, `status`, `schema` |

### Percentile columns

Any `report*` type accepts an optional percentile suffix in the COMMENT:

```
report3:::90,95,99
```

This adds `p90`, `p95`, `p99` columns (one per percentile value) after the standard columns.

Fractional percentiles use an explicit `p=` prefix in the same suffix:

```
report3:::p=99.9,99.99,100
tag_info:category::p=99.99,99.9,99.99
```

Create one trailing column per value, in the requested order; repeated values
remain separate columns. The new grammar accepts decimal percentages in `(0,100]`
with at most nine digits after the dot. Signs, whitespace, exponents, NaN/Inf,
partial tokens and empty entries are rejected. Ranks use exact decimal arithmetic
and the nearest-rank rule `ceil(observation_count * percentage / 100)`. Timer
observations use their hit weights. Histogram interpolation and resolution remain
unchanged; requesting more decimal places does not create more observations.

Existing suffixes keep their legacy float/floor rank and prefix parsing:
`:::99.9` continues to select p99. Opt into `:::p=99.9` explicitly when creating
new tables. Existing tables, column positions and client queries are not migrated.
Ranks now use the native count width rather than narrowing to 32 bits.

The optional `histogram_max_time` condition must be finite and within
`[1, INT_MAX]` seconds. Existing positive decimal truncation to whole seconds is
preserved. Invalid configurations are rejected before report construction.

```sql
CREATE TABLE `report_by_hostname_p95` (
  -- standard report3 columns ...
  `hostname`          varchar(64)  DEFAULT NULL,
  -- extra percentile columns (must be last, one per value in suffix)
  `p90`               float        DEFAULT NULL,
  `p95`               float        DEFAULT NULL,
  `p99`               float        DEFAULT NULL
) ENGINE=PINBA DEFAULT CHARSET=latin1 COMMENT='report3:::90,95,99';
```

## Tag-based report tables

These tables are user-defined — you create them for each tag name your PHP code sends.
The tag name is embedded in the COMMENT.

### `tag_report` — requests grouped by (script_name, tag_value)

COMMENT syntax: `tag_report:<tag_name>`

Columns (in order):
1. `script_name` varchar(128)
2. `tag_value` varchar(64)
3. `req_count` int
4. `req_per_sec` float
5. `hit_count` int
6. `hit_per_sec` float
7. `timer_value` float
8. `timer_median` float
9. `ru_utime_value` float
10. `ru_stime_value` float
11. `index_value` varchar(256)

```sql
CREATE TABLE `tag_report_by_action` (
  `script_name`     varchar(128) DEFAULT NULL,
  `tag_value`       varchar(64)  DEFAULT NULL,
  `req_count`       int          DEFAULT NULL,
  `req_per_sec`     float        DEFAULT NULL,
  `hit_count`       int          DEFAULT NULL,
  `hit_per_sec`     float        DEFAULT NULL,
  `timer_value`     float        DEFAULT NULL,
  `timer_median`    float        DEFAULT NULL,
  `ru_utime_value`  float        DEFAULT NULL,
  `ru_stime_value`  float        DEFAULT NULL,
  `index_value`     varchar(256) DEFAULT NULL
) ENGINE=PINBA DEFAULT CHARSET=latin1 COMMENT='tag_report:action';
```

### `tag_report2` — requests grouped by tag_value only

COMMENT syntax: `tag_report2:<tag_name>`

Same columns as `tag_report` but keyed by `tag_value` alone (script_name not in the key).

### `tagN_report` / `tagN_report2` — multiple tags

COMMENT syntax: `tagN_report:<tag1>,<tag2>,...`

Groups by multiple tag values. Column order: tag value columns first (one per tag), then the standard timer/count columns.

```sql
CREATE TABLE `tag_report_category_group` (
  `tag1_value`      varchar(64)  DEFAULT NULL,  -- category
  `tag2_value`      varchar(64)  DEFAULT NULL,  -- group
  `req_count`       int          DEFAULT NULL,
  -- ... rest of standard columns
  `index_value`     varchar(256) DEFAULT NULL
) ENGINE=PINBA DEFAULT CHARSET=latin1 COMMENT='tagN_report:category,group';
```

## Tag info tables

Tag info tables aggregate timer statistics per tag value, decoupled from script name.

### `tag_info` — timer info for one tag

COMMENT syntax: `tag_info:<tag_name>`

### `tag2_info` — timer info for two fixed tags

COMMENT syntax: `tag2_info:<tag1>,<tag2>`

### `tagN_info` — timer info for N tags with built-in field dimensions

COMMENT syntax: `tagN_info:<tag1>,...[,__server_name][,__hostname]::[percentiles]`

Built-in field names prefixed with `__` can be mixed into the tag list as additional grouping dimensions:
- `__server_name` — groups by `server_name` field from the request
- `__hostname` — groups by `hostname` field from the request

Percentile suffix works the same as for report tables.

```sql
-- Timer info by custom tags "category" and "group",
-- further broken down by server_name, with 90th and 95th percentiles.
CREATE TABLE `timer_by_category_and_server` (
  `tag1_value`       varchar(64)  DEFAULT NULL,
  `tag2_value`       varchar(64)  DEFAULT NULL,
  `tag3_value`       varchar(64)  DEFAULT NULL,
  `req_count`        int          DEFAULT NULL,
  `req_per_sec`      float        DEFAULT NULL,
  `hit_count`        int          DEFAULT NULL,
  `hit_per_sec`      float        DEFAULT NULL,
  `timer_value`      float        DEFAULT NULL,
  `timer_median`     float        DEFAULT NULL,
  `ru_utime_value`   float        DEFAULT NULL,
  `ru_stime_value`   float        DEFAULT NULL,
  `index_value`      varchar(256) DEFAULT NULL,
  `p90`              float        DEFAULT NULL,
  `p95`              float        DEFAULT NULL
) ENGINE=PINBA DEFAULT CHARSET=latin1 COMMENT='tagN_info:category,group,__server_name::90,95';
```

## Reversed tag tables (`rtag_*`)

The `rtag_*` family mirrors the `tag_*` family but groups by tag value **without** script name as the primary key — useful for aggregating across all scripts.

| Type | COMMENT prefix |
|---|---|
| `rtag_report` | `rtag_report:<tag>` |
| `rtag2_report` | `rtag2_report:<tag1>,<tag2>` |
| `rtagN_report` | `rtagN_report:<tag1>,...` |
| `rtag_info` | `rtag_info:<tag>` |
| `rtag2_info` | `rtag2_info:<tag1>,<tag2>` |
| `rtagN_info` | `rtagN_info:<tag1>,...` |

## Column ordering rules

The engine identifies columns **by position only** — the SQL column name is ignored.
When defining a custom tag table:
1. Tag value columns come first (one per tag, in COMMENT order).
2. Built-in dimension columns (`__server_name`, `__hostname`) follow in COMMENT order.
3. Standard stat columns follow in the fixed order shown above.
4. Percentile columns come last, in the order listed in the suffix.

## Testing a new table

Use `scripts/smoke_test.sh` to verify that your custom table definition works before deploying:

```bash
./scripts/smoke_test.sh 127.0.0.1 3306 root "" pinba
```
