---
title: "PHP Pinba Extension — Documentation"
type: source
sources:
  - raw/repos/php-pinba-extension.md
related:
  - wiki/concepts/pinba-data-flow.md
  - wiki/concepts/php-pinba-configuration.md
  - wiki/concepts/pinba-udp-protocol.md
confidence: high
updated: 2026-05-23
---

# PHP Pinba Extension

**Source**: tony2001/pinba_extension on GitHub. Latest: 1.1.2 (Aug 2020).

## Key Facts

- **Default port**: 30002 (`PINBA_COLLECTOR_DEFAULT_PORT`)
- **Disabled by default**: `pinba.enabled = 0` — must explicitly enable
- **Max collectors**: 8 (sends to multiple Pinba servers simultaneously)
- **PHP 7+ required**: version 1.1.0+; PHP 5 branch exists separately

## Critical INI Settings

```ini
pinba.server  = "host:30002"  ; NULL default — must be set
pinba.enabled = 1              ; 0 default — must be set to 1!
pinba.auto_flush = 1           ; flush at request end (default: on)
pinba.resolve_interval = 60    ; DNS cache TTL (seconds)
```

The two most common misconfiguration errors:
1. Forgetting `pinba.enabled = 1` — extension loads but sends nothing
2. Wrong `pinba.server` — data silently lost (UDP fire-and-forget, no error)

## Data Sent Per Request

Automatically at shutdown: hostname, server_name, script_name, request_time, ru_utime, ru_stime,
document_size, memory_peak, memory_footprint, HTTP status, schema, all timers, request-level tags.

## Timer API Pattern

```php
$t = pinba_timer_start(['group' => 'db', 'server' => 'replica']);
// ... do work ...
pinba_timer_stop($t);
```

Tags are key-value pairs — any string values. They appear in timer reports in Pinboard.

## Request-Level Tags (since 1.1.0)

```php
pinba_tag_set('api_version', 'v2');
pinba_tag_set('user_type', 'premium');
```

Used for filtering in Pinba report tables (WHERE conditions on tag reports).

See: [[php-pinba-configuration]], [[pinba-udp-protocol]]
