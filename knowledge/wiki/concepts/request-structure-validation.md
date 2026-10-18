---
title: "Request Structure Validation and Nested Ownership"
type: concept
sources: []
related:
  - wiki/concepts/pinba-data-flow.md
  - wiki/concepts/dictionary-word-lifetime.md
confidence: high
updated: 2026-10-18
---

# Request Structure Validation and Nested Ownership

`pinba_validate_request()` checks timer-array lengths, the exact sum of flattened
tag spans, request/timer dictionary indices and storage representation bounds
before dictionary interning. Subtraction bounds the tag sum without overflow.
Timer hit counts fit the signed histogram weight, and timer/tag counts fit the
existing 16-bit internal fields. Structurally inconsistent input is rejected;
valid request_count normalization, float sanitation and absent/partial timer CPU
arrays retain their legacy behavior.

Skipped zero-hit or untagged timers consume their input tag span. Surviving
timers are stored contiguously. Worker reservations count surviving timers,
preventing gaps between workers as well as within each request.

The detached read pool owns each complete decoded protobuf tree until all batch
jobs finish. Staging records borrow nodes. A rejected parent does not invalidate
its children; every descendant is validated independently. This corrects an
old use-after-free when an invalid parent reused its staging slot before its
children were processed. Protobuf's existing recursion limit and UDP datagram
size bound decoding; no new arbitrary protocol limit is introduced.

Semantic rejects increment `status.invalid_request_data`, including batches
with no accepted requests. Cleanup also covers capacity-dropped records.

Unit tests cover flattened lengths, indices and signed/16-bit bounds. The actual
UDP/SQL regression verifies that a timer following a zero-hit timer receives its
own tag, malformed packets do not enter raw tables, and a valid child/grandchild
survives a rejected parent. The previous plugin produces the skipped timer's
wrong tag in this regression. The corrected path passes the full exercised SQL
suite under ASan/UBSan on an isolated MariaDB.
