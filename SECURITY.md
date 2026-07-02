# Security Policy

## Supported Branch

Security fixes are developed on the active `master` branch of this fork.

## Supported Database Matrix

The plugin is built and tested against the database branches tracked by this
fork's CI and release policy. As of 2026-07-02, the active matrix is:

- `MySQL 8.0`
- `MySQL 8.4 LTS`
- `MariaDB 10.11`
- `MariaDB 11.8 LTS`

Older end-of-life database branches (including the original project's MySQL ≤ 5.6
target) are not supported for security maintenance in this fork.

## Threat Model

`pinba_engine` runs inside the database server and listens on a UDP port for
protobuf packets emitted by the Pinba PHP extension. The packet-parsing path
processes **untrusted network input**, so memory-safety and input-validation
issues there are treated as security-relevant.

## Reporting A Vulnerability

Please do not open a public issue for a suspected security vulnerability.

Report it privately via GitHub's
[private vulnerability reporting](https://github.com/XOlegator/pinba_engine/security/advisories/new),
or by email to:

- Oleg Ekhlakov <o.ekhlakov@protonmail.com>

Include, when possible:

- affected database server and version (MySQL / MariaDB);
- affected engine version or commit;
- reproduction steps, ideally a sample UDP packet or workload;
- whether the issue impacts confidentiality, integrity, availability, or data correctness;
- whether the issue is remotely triggerable via the UDP listener.

## Response Policy

- Acknowledgement target: reasonable best effort.
- Fixes should preserve the existing public contract unless a breaking security
  mitigation is unavoidable.
- Public release notes are published after a fix is prepared or released.
