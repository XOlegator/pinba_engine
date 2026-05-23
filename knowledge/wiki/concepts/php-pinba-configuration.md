---
title: "PHP Pinba Extension Configuration"
type: concept
sources:
  - raw/repos/php-pinba-extension.md
related:
  - wiki/concepts/pinba-data-flow.md
  - wiki/concepts/pinba-udp-protocol.md
confidence: high
updated: 2026-05-23
---

# PHP Pinba Extension Configuration

## Minimal Working Configuration

```ini
extension=pinba.so

pinba.server  = "pinba-host:30002"
pinba.enabled = 1
```

Both directives are required. The extension **does not send anything by default** (`enabled=0`).

## Deployment Scenarios

### Local PHP + Local MySQL (all on host)
```ini
pinba.server = "127.0.0.1:30002"
pinba.enabled = 1
```

### PHP on host, MySQL+Pinba in Docker
```ini
pinba.server = "127.0.0.1:31002"   # PINBA_UDP_PORT from docker .env (mapped host port)
pinba.enabled = 1
```

### PHP in Docker, MySQL+Pinba in same Docker network
```ini
pinba.server = "mysql-pinba:30002"  # Docker service name, container port (not host port)
pinba.enabled = 1
```

### Pinba PHP extension NOT installed (Pinboard docker stack)
Pinboard uses PHP FPM Alpine image — pinba extension is not installed there because
Pinboard is the consumer (reads from MySQL), not the producer (doesn't send metrics).
Only PHP applications being monitored need the extension.

## Multi-Server (fan-out)
```ini
pinba.server = "server1:30002,server2:30002"
```
Up to 8 collectors. Sends to all simultaneously.

## Silent Failure Pattern

UDP is fire-and-forget. If the server is wrong:
- No PHP error
- No timeout
- No log message
- Data simply doesn't arrive

To verify data is flowing: `SELECT COUNT(*) FROM pinba.request` (should increase with requests).

## Verification Checklist

```bash
# 1. Extension loaded?
php -m | grep pinba

# 2. Configuration visible?
php -r "print_r(ini_get_all('pinba'));"

# 3. Is it enabled?
php -r "var_dump(pinba_get_info());"
# Should return array with req_time, timers etc.

# 4. Is data arriving in MySQL?
mysql -uroot -p -e "SELECT COUNT(*) FROM pinba.request;"
# Value should increase between requests
```

See: [[pinba-data-flow]], [[pinba-udp-protocol]]
