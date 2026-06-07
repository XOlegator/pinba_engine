# Pinba Engine

Pinba Engine is a MySQL storage engine that collects and analyzes PHP runtime statistics sent over UDP by the [Pinba PHP extension](https://github.com/tony2001/pinba2).

This is an actively maintained fork of [tony2001/pinba_engine](https://github.com/tony2001/pinba_engine) with full support for MySQL 8.0 and MySQL 8.4 LTS.

## Install

### Docker

Pre-built images are published to Docker Hub for MySQL 8.0 and MySQL 8.4 LTS:

```bash
# MySQL 8.0
docker run -d \
  --name pinba \
  -e MYSQL_ROOT_PASSWORD=secret \
  -e MYSQL_DATABASE=pinba \
  -p 3306:3306 \
  -p 30002:30002/udp \
  xolegator/pinba-engine:8.0

# MySQL 8.4 LTS
docker run -d \
  --name pinba \
  -e MYSQL_ROOT_PASSWORD=secret \
  -e MYSQL_DATABASE=pinba \
  -p 3306:3306 \
  -p 30002:30002/udp \
  xolegator/pinba-engine:8.4-lts
```

Validate that the plugin is active:

```bash
docker exec pinba mysql -uroot -psecret -e "SHOW PLUGINS LIKE 'pinba';"
```

See [docs/docker.md](docs/docker.md) for full Docker usage including tagging, validation, and troubleshooting.

### Ubuntu / Debian package (PPA)

Packages are available via Launchpad PPA:

| Ubuntu release | MySQL version | Package |
|---|---|---|
| 24.04 Noble | 8.0 | `pinba-engine-mysql-8.0` |
| 26.04 Resolute | 8.4 | `pinba-engine-mysql-8.4` |

```bash
sudo add-apt-repository ppa:xolegator/packages
sudo apt-get update
sudo apt-get install pinba-engine-mysql-8.0   # or pinba-engine-mysql-8.4
```

After install, load the plugin and initialize the schema:

```bash
mysql -u root -p -e "INSTALL PLUGIN pinba SONAME 'ha_pinba.so';"
mysql -u root -p -e "CREATE DATABASE IF NOT EXISTS pinba;"
mysql -u root -p pinba < /usr/share/pinba_engine/default_tables.sql
```

### Build from source

See [docs/build.md](docs/build.md) for full requirements and options.

Quick start with CMake presets:

```bash
cmake --preset release
cmake --build --preset release
# result: build/ha_pinba.so
```

## Documentation

- [docs/build.md](docs/build.md) — build requirements, CMake presets, local plugin install
- [docs/docker.md](docs/docker.md) — Docker images: build, run, publish
- [docs/releasing.md](docs/releasing.md) — release flow and commit message rules
- [docs/roadmap.md](docs/roadmap.md) — open work and known gaps
- [API reference](https://xolegator.github.io/pinba_engine/) — generated Doxygen documentation

## License

This project is licensed under the [GNU General Public License v2.0](LICENSE) (GPL-2.0-only), inheriting the license of the original Pinba Engine by Antony Dovgal.

Copyright (c) 2007–2009 Antony Dovgal  
Copyright (c) 2025–2026 Oleg Ekhlakov

Third-party components bundled in this repository:

| Component | Files | License |
|---|---|---|
| [xxHash](https://github.com/Cyan4973/xxHash) | `src/xxhash.[ch]` | BSD-2-Clause |
| [protobuf-c](https://github.com/protobuf-c/protobuf-c) | `src/protobuf-c.h` | BSD-3-Clause |
