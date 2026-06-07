---
title: "PHP Pinba Extension API"
type: concept
sources:
  - raw/repos/php-pinba-extension.md
related:
  - wiki/sources/pinba-extension-fork.md
  - wiki/concepts/php-pinba-configuration.md
  - wiki/concepts/pinba-udp-protocol.md
  - wiki/concepts/pinba-data-flow.md
confidence: high
updated: 2026-06-07
---

# PHP Pinba Extension API

All functions are in the global namespace. The extension exposes no classes.

## Timer Lifecycle

### Start / Stop

```php
resource pinba_timer_start(array $tags, array $data = null, int $hit_count = 1): resource|false
```
Starts a running timer. `$tags` must be a non-empty array of string key-value pairs.
`$data` is arbitrary per-timer payload (not sent to Pinba; for application use).
Returns an opaque timer resource, or `false` on error.

```php
bool pinba_timer_stop(resource $timer): bool
```
Stops a running timer. Time is measured from `pinba_timer_start()` call.

```php
bool pinba_timer_delete(resource $timer): bool
```
Deletes a timer entirely — it will not be included in the next flush.

### Add a Pre-Completed Timer

```php
resource pinba_timer_add(
    array $tags,
    float $value,
    array $data = null,
    int $hit_count = 1
): resource|false
```
Registers a timer that has already completed with value `$value` (seconds). Useful when
the actual timing was done externally (e.g., a database client returning elapsed time).

## Timer Mutation

These functions modify an existing timer after creation.

```php
bool pinba_timer_data_merge(resource $timer, array $data): bool
```
Merges `$data` into the timer's application payload (array_merge semantics).

```php
bool pinba_timer_data_replace(resource $timer, array|null $data): bool
```
Replaces the timer's application payload entirely. Pass `null` to clear it.

```php
bool pinba_timer_tags_merge(resource $timer, array $tags): bool
```
Merges `$tags` into the timer's tag set (existing keys overwritten, new keys added).

```php
bool pinba_timer_tags_replace(resource $timer, array $tags): bool
```
Replaces the timer's tag set entirely.

## Request-Level Overrides

These functions override the values that the extension auto-detects from the PHP runtime.
Useful when the detected values are wrong or incomplete (e.g., CLI scripts, proxies).

```php
bool pinba_script_name_set(string $script_name): bool
```
Overrides `$_SERVER['SCRIPT_NAME']` sent in the protobuf message.

```php
bool pinba_hostname_set(string $hostname): bool
```
Overrides the server's hostname (defaults to `gethostname()`).

```php
bool pinba_schema_set(string $schema): bool
```
Sets the schema field (e.g., `"http"`, `"https"`, `"grpc"`). Empty by default.

```php
bool pinba_server_name_set(string $server_name): bool
```
Overrides `$_SERVER['SERVER_NAME']` / `HTTP_HOST` (the virtual host name sent to Pinba).

```php
bool pinba_request_time_set(float $time): bool
```
Overrides the measured request time (seconds). Useful for workers that accumulate
time across multiple sub-requests.

## Request-Level Tags

Request-level tags appear in `tag_report`, `tag2_info`, and `tagN_info` tables in Pinba.
They are separate from timer tags — they apply to the entire request.

```php
bool pinba_tag_set(string $name, string $value): bool
```
Sets a request-level tag. Creates it if it does not exist; overwrites if it does.

```php
string|false pinba_tag_get(string $name): string|false
```
Returns the current value of a request-level tag, or `false` if not set.

```php
bool pinba_tag_delete(string $name): bool
```
Removes a request-level tag.

```php
array pinba_tags_get(): array
```
Returns all current request-level tags as an associative array.

## Data Access

```php
array pinba_get_info(): array
```
Returns the current request snapshot as an associative array. Keys include:
`hostname`, `server_name`, `script_name`, `request_count`, `document_size`,
`memory_peak`, `memory_footprint`, `request_time`, `ru_utime`, `ru_stime`,
`timers` (array of timer info arrays), `tags` (request-level tags).

```php
array pinba_get_data(int $flags = 0): array
```
Like `pinba_get_info()` but returns the raw protobuf-ready parallel arrays.
`$flags` accepts `PINBA_FLUSH_ONLY_STOPPED_TIMERS`.

```php
array pinba_timer_get_info(resource $timer): array
```
Returns info for a single timer: `value`, `hit_count`, `tags`, `data`, `started`.

```php
array pinba_timers_get(int $flags = 0): array
```
Returns all timer resources. `$flags` accepts `PINBA_TIMERS_STOP_ALL`.

```php
bool pinba_timers_stop(): bool
```
Stops all currently running timers.

## Flow Control

```php
bool pinba_flush(string $script_name = null, int $flags = 0): bool
```
Sends the current request data to the Pinba server immediately, then resets timers.
Optional `$script_name` overrides the script name for this flush only.
`$flags` accepts `PINBA_FLUSH_ONLY_STOPPED_TIMERS` — excludes still-running timers.

```php
bool pinba_reset(): bool
```
Resets all timers and request-level data without flushing. Useful in long-running workers
between processing units.

## Constants

| Constant | Value | Used with |
|----------|-------|-----------|
| `PINBA_FLUSH_ONLY_STOPPED_TIMERS` | 1 | `pinba_flush()`, `pinba_get_data()` |
| `PINBA_TIMERS_STOP_ALL` | 1 | `pinba_timers_get()` |

## Typical Usage Pattern

```php
// Start timer before expensive operation
$t = pinba_timer_start(['group' => 'db', 'op' => 'query']);

$result = $db->query($sql);

// Stop timer after operation completes
pinba_timer_stop($t);

// Optional: annotate the request with business-level tags
pinba_tag_set('customer_tier', 'premium');
pinba_tag_set('api_version', 'v3');

// Auto-flush happens at RSHUTDOWN (pinba.auto_flush=1)
// Or explicitly: pinba_flush();
```

## Worker / CLI Pattern

```php
// CLI worker: process multiple units, track each separately
while ($job = $queue->pop()) {
    $t = pinba_timer_start(['queue' => $job->queue]);
    process($job);
    pinba_timer_stop($t);

    pinba_script_name_set($job->type);   // meaningful script name per unit
    pinba_flush();                        // send immediately
    pinba_reset();                        // clear for next iteration
}
```

See: [[php-pinba-configuration]], [[pinba-udp-protocol]], [[pinba-data-flow]]
