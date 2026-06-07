# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.4.1](https://github.com/XOlegator/pinba_engine/compare/v2.4.0...v2.4.1) (2026-06-07)


### Bug Fixes

* **ci:** use passive FTP and retry on Launchpad upload errors ([#34](https://github.com/XOlegator/pinba_engine/issues/34)) ([70489fa](https://github.com/XOlegator/pinba_engine/commit/70489fab43afad7f62c3e8ede7d71c7124e7b4e3))

## [2.4.0](https://github.com/XOlegator/pinba_engine/compare/v2.3.0...v2.4.0) (2026-06-07)


### Features

* **ci:** automate full release cycle on MySQL version updates ([#32](https://github.com/XOlegator/pinba_engine/issues/32)) ([c6dc449](https://github.com/XOlegator/pinba_engine/commit/c6dc449cd978059db990b46afeaafe29869cb4e8))

## [2.3.0](https://github.com/XOlegator/pinba_engine/compare/v2.2.1...v2.3.0) (2026-06-07)


### Features

* **debian:** add AppStream metainfo for Discover and GNOME Software ([#30](https://github.com/XOlegator/pinba_engine/issues/30)) ([12f16f9](https://github.com/XOlegator/pinba_engine/commit/12f16f9d601fe0d1abc836ccc2a49e8005c3f2df))

## [2.2.1](https://github.com/XOlegator/pinba_engine/compare/v2.2.0...v2.2.1) (2026-06-06)


### Bug Fixes

* **ci:** install paramiko for ppa upload ([#26](https://github.com/XOlegator/pinba_engine/issues/26)) ([a5706d0](https://github.com/XOlegator/pinba_engine/commit/a5706d01d26f25b195cea9717b6d99c4a34070d1))
* **ci:** pin manual ppa uploads to source refs ([#29](https://github.com/XOlegator/pinba_engine/issues/29)) ([e7d9b54](https://github.com/XOlegator/pinba_engine/commit/e7d9b548ffa62a12e904947d369b175ca7fd6d7d))
* **ci:** preserve buildinfo for ppa signing ([#24](https://github.com/XOlegator/pinba_engine/issues/24)) ([a350c9e](https://github.com/XOlegator/pinba_engine/commit/a350c9eb800659c9938c9f8982f614297aaba853))
* **ci:** split multi-series ppa uploads ([#28](https://github.com/XOlegator/pinba_engine/issues/28)) ([b344106](https://github.com/XOlegator/pinba_engine/commit/b34410624d8a6acbcfb0ac3341085adee5d58cf0))
* **ci:** upload launchpad packages over ftp ([#27](https://github.com/XOlegator/pinba_engine/issues/27)) ([7ed63e5](https://github.com/XOlegator/pinba_engine/commit/7ed63e53bc3c7e5e1ac6bef6d6c9e46695dfa0db))

## [2.2.0](https://github.com/XOlegator/pinba_engine/compare/v2.1.2...v2.2.0) (2026-06-06)


### Features

* **ci:** automate multi-distro Launchpad PPA builds ([#21](https://github.com/XOlegator/pinba_engine/issues/21)) ([c3107a6](https://github.com/XOlegator/pinba_engine/commit/c3107a6dfc27f1196591fffb4a39f8224a088ae0))


### Bug Fixes

* **ci:** repair ppa workflow matrix selection ([#23](https://github.com/XOlegator/pinba_engine/issues/23)) ([26dccd5](https://github.com/XOlegator/pinba_engine/commit/26dccd5bb8ff11eab27f1f4b72dd4e93559b9574))

## [2.1.2](https://github.com/XOlegator/pinba_engine/compare/v2.1.1...v2.1.2) (2026-05-31)


### Bug Fixes

* **cmake:** guard install(pinba_test) with PINBA_WITH_TESTS ([#16](https://github.com/XOlegator/pinba_engine/issues/16)) ([804ac96](https://github.com/XOlegator/pinba_engine/commit/804ac96d6fce73299dc901520cdcee3f7fe54f2f))

## [2.1.1](https://github.com/XOlegator/pinba_engine/compare/v2.1.0...v2.1.1) (2026-05-31)


### Bug Fixes

* **cmake:** guard pinba_test target with PINBA_WITH_TESTS ([#14](https://github.com/XOlegator/pinba_engine/issues/14)) ([65a55ae](https://github.com/XOlegator/pinba_engine/commit/65a55ae80105e7c5ed610375c875e81f07ab792b))

## [2.1.0](https://github.com/XOlegator/pinba_engine/compare/v2.0.2...v2.1.0) (2026-05-31)


### Features

* P3 — SHOW STATUS version, benchmarks CI, Doxygen docs ([#11](https://github.com/XOlegator/pinba_engine/issues/11)) ([d66e842](https://github.com/XOlegator/pinba_engine/commit/d66e8425bb4626042afb095aa54868be66f8c1f1))

## [2.0.2](https://github.com/XOlegator/pinba_engine/compare/v2.0.1...v2.0.2) (2026-05-31)


### Bug Fixes

* **build:** fix FetchContent/find_package conflict for GTest and benchmark ([900a434](https://github.com/XOlegator/pinba_engine/commit/900a434fb74c5b86ce3534555492c98fb010a3a9))
* **ci:** add rapidjson-dev and fix gitignore blocking docker config ([8dab3f7](https://github.com/XOlegator/pinba_engine/commit/8dab3f79046f8545f13ea9dc2b0b6970e6ac9589))

## [2.0.1](https://github.com/XOlegator/pinba_engine/compare/v2.0.0...v2.0.1) (2026-05-31)


### Bug Fixes

* avoid warning-prone error formatting ([a8f8c72](https://github.com/XOlegator/pinba_engine/commit/a8f8c724a644c4229495b3a9cee8eafc77a97633))
* **docker:** fix plugin ABI, OL9 library paths, and mysql_version.h generation ([87e9fbc](https://github.com/XOlegator/pinba_engine/commit/87e9fbc3fc82bd86f6661b90d082857a41fcb097))
* **docker:** stabilize MySQL 8 Pinba image build and runtime for pinboard integration ([3cd3fe8](https://github.com/XOlegator/pinba_engine/commit/3cd3fe830718873aa64b31e8230379fb591f28da))
* **docker:** stabilize pinba_engine image build for MySQL 8 and update Docker guide ([f4a8025](https://github.com/XOlegator/pinba_engine/commit/f4a8025e3d0992e521f74ed91a9486eab29e276d))
* **docker:** stabilize pinba_engine image build for MySQL 8 and update Docker guide ([fc3b285](https://github.com/XOlegator/pinba_engine/commit/fc3b285c1dfb513851cb469c0e2857c66755cf7c))

## [2.0.0](https://github.com/XOlegator/pinba_engine/compare/v1.3.0...v2.0.0) (2026-03-08)


### ⚠ BREAKING CHANGES

* **mysql8:** build system and compatibility baseline were changed for MySQL 8 modernization.

### Features

* **mysql8:** migrate build and engine code to modern MySQL 8 stack ([1c7ace7](https://github.com/XOlegator/pinba_engine/commit/1c7ace7a2af8fb48ca0d8ec8ea606ff7f09f482b))

## [1.3.0](https://github.com/XOlegator/pinba_engine/compare/v1.2.0...v1.3.0) (2026-03-07)


### Features

* **release:** setup semi-automated GitHub releases with Release Please ([901b294](https://github.com/XOlegator/pinba_engine/commit/901b2947bfa6e5c708521606cab069001dd3bf85))

## [1.2.0] - Legacy baseline

- Historical baseline imported from old `RELEASE_1_2_0` line.
- Previous release notes were maintained in `docs/legacy-news.md`.
- New releases are managed by GitHub Release Please and added here automatically.
