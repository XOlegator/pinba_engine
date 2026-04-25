# Roadmap

## MySQL 8.0 Engine Stabilization

Completed:

- Stabilize the MySQL 8.0 storage engine build path.
- Replace obsolete mock-header assumptions with the real MySQL 8.0 source-header workflow.
- Enable the CMake/CTest test path for current unit tests.

Open work:

- Remove remaining compiler and static-analysis warnings in `src/ha_pinba.cc` and related runtime code.
- Audit all virtual tables supported by the engine and bring `default_tables.sql` in sync with the parser and handler implementation.
- Validate report tables (`report_by_*`, `tag_report*`, `active_reports`) on real traffic and fix runtime/DDL mismatches.
- Add repeatable smoke tests for plugin install, table creation, packet ingestion from PHP, and basic `SELECT` queries.
- Review memory ownership, thread-safety, and long-running stability of collector/report code under MySQL 8.0.
