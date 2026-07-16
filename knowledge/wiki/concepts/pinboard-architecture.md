---
title: "Pinboard Architecture"
type: concept
sources:
  - raw/repos/pinboard-architecture.md
related:
  - wiki/concepts/pinba-data-flow.md
  - wiki/concepts/docker-build-strategy.md
  - wiki/concepts/docker-tag-strategy.md
confidence: high
updated: 2026-07-16
---

# Pinboard Architecture

Pinboard is the web dashboard that reads aggregated Pinba statistics and presents them
as a UI. It is a separate project from `pinba_engine` but directly depends on it.

## Role in the Full Stack

```
PHP app → UDP:30002 → pinba_engine (MySQL plugin)
                           │
                     writes live data to: request, timer, timertag, tag
                     exposes virtual views: ipm_pinba_report_*, ipm_pinba_tag_info_*
                           │
                     Pinboard aggregate command (every 15 min)
                           │
                     writes timestamped snapshots to: ipm_report_*, ipm_tag_info, ipm_req_time_details, ...
                           │
                     Pinboard web UI (Symfony + Nginx) reads report tables → dashboard
```

## Technology Stack

| Component | Technology |
|-----------|-----------|
| Framework | Symfony 8 |
| PHP | 8.4 (FPM 8.5 in Docker) |
| Database access | Doctrine DBAL (raw SQL, no ORM writes) |
| Templates | Twig |
| Mail | Symfony Mailer |
| Frontend | pnpm + Webpack Encore |
| Cron | supercronic (Docker) or native crontab |

## Docker Compose Stack

Four services:

| Service | Purpose |
|---------|---------|
| `mysql-pinba` | MySQL + Pinba plugin (wraps `xolegator/pinba-engine:8.0` or `8.4-lts`) |
| `php-fpm` | Symfony app (web requests) |
| `nginx` | Reverse proxy for php-fpm |
| `aggregate` | Same PHP image, runs supercronic with the cron schedule |

The `aggregate` service is intentionally separate from `php-fpm` — cron is not mixed into the
web container. It uses `supercronic` which is a Docker-native cron runner (no syslog requirement).

### mysql-pinba Init Sequence (first start only)

1. `01-init-pinba.sh` (from pinba-engine image): INSTALL PLUGIN + CREATE DATABASE pinba + import schema
2. `20-create-pinboard-user.sh` (from pinboard's mysql wrapper Dockerfile): CREATE USER pinba, GRANT ALL

### Ports

| Port | Protocol | Service |
|------|----------|---------|
| `MYSQL_PORT` (13306) | TCP | MySQL |
| `PINBA_UDP_PORT` (31002) | UDP | Pinba listener |
| `NGINX_HOST_HTTP_PORT` (18088) | TCP | Web UI |

## Aggregation Command (`php bin/console aggregate`)

The core of Pinboard. Runs every 15 minutes by default.

### Execution Steps

1. DB connectivity check (`SELECT 1`)
2. Lock file check (`var/aggregate.lock`, TTL 900 s, auto-removes stale locks)
3. Delete report rows older than `APP_RECORDS_LIFETIME` (default `P1M`) from 9 tables
4. (If notifications enabled) Query HTTP 5xx pages from `request` table → send alert email
5. Compute p90/p95/p99/p100 percentiles for each `(server_name, hostname)` from `request` table → `ipm_report_2_by_hostname_and_server`
6. Copy from Pinba virtual views → `ipm_report_by_hostname`, `ipm_report_by_hostname_and_server`, `ipm_report_by_server_name`
7. Copy timer stats from `ipm_pinba_tag_info_*` → `ipm_tag_info`
8. Top-10 slow requests per server → `ipm_req_time_details` + `ipm_timer`
9. Top-10 heavy-memory pages → `ipm_mem_peak_usage_details`
10. Top-10 heavy-CPU pages → `ipm_cpu_usage_details`
11. Drawdown detection: compare last 2 p90/p95 snapshots against threshold → send alert
12. Remove lock file

### Percentile Implementation

Uses MySQL subquery pattern:
```sql
(SELECT r.req_time FROM request r WHERE ... ORDER BY r.req_time DESC LIMIT N, 1) as req_time_90
```
where `N = count * (1 - 0.90)`. Simple offset-based approximation, not a window function.

### Source vs Report Table Distinction

| Source tables | Report tables |
|---------------|--------------|
| `request`, `timer`, `timertag`, `tag` | `ipm_report_*`, `ipm_tag_info`, `ipm_req_time_details`, `ipm_timer`, `ipm_status_details`, `ipm_mem_peak_usage_details`, `ipm_cpu_usage_details` |
| Live, written by Pinba engine via UDP | Timestamped snapshots, written by aggregate command |
| No history (Pinba's virtual views are live-computed) | History kept for `APP_RECORDS_LIFETIME` |

As of July 16, 2026, the shared `hostname` / `server_name` dimensions in the
engine and Pinboard-managed aggregate tables are standardized on `varchar(64)`.

## Key Configuration Variables

| Variable | Default | Effect |
|----------|---------|--------|
| `APP_RECORDS_LIFETIME` | `P1M` | How far back to keep snapshots |
| `APP_AGGREGATION_PERIOD` | `PT15M` | Cron interval (affects req/s denominator) |
| `APP_AGGREGATE_LOCK_TTL_SECONDS` | 900 | Stale lock auto-removal threshold |
| `APP_LOGGING_LONG_REQUEST_TIME_GLOBAL` | 1.5 | Slow request threshold (s) |
| `APP_LOGGING_HEAVY_REQUEST_GLOBAL` | 30000 | Heavy memory threshold (KB) |
| `APP_LOGGING_HEAVY_CPU_REQUEST_GLOBAL` | 1 | Heavy CPU threshold (s user-time) |
| `APP_NOTIFICATION_ENABLE` | false | Email alerts on/off |
| `APP_NOTIFICATION_REQ_TIME_BORDER_GLOBAL` | 1.5 | Drawdown alert border (s) |

Per-server overrides via `APP_LOGGING_*_MAP` (JSON `{"server_name": value}`).

## mysql-pinba Image Variants

| Env file | `PINBA_IMAGE` | MySQL series |
|----------|--------------|-------------|
| `.env.mysql80` | `xolegator/pinba-engine:8.0` | 8.0.x (Debian bookworm) |
| `.env.mysql84` | `xolegator/pinba-engine:8.4-lts` | 8.4.x (Oracle Linux 9) |
| `.env` (default) | `xolegator/pinba-engine:8.0` | 8.0.x |

## Authentication

Two user sources selectable via `APP_AUTH_USER_SOURCE`:
- `file` — static file at `APP_AUTH_USERS_FILE`
- `db` — Doctrine `User` entity

See: [[pinba-data-flow]], [[docker-build-strategy]], [[docker-tag-strategy]]
