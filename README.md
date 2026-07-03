# Pinba Engine

Pinba Engine is a MySQL/MariaDB storage engine that collects and analyzes PHP runtime statistics sent over UDP by the [Pinba PHP extension](https://github.com/tony2001/pinba2).

This is an actively maintained fork of [tony2001/pinba_engine](https://github.com/tony2001/pinba_engine) with full support for MySQL 8.0 and MySQL 8.4 LTS, and for MariaDB 10.11 and 11.8 LTS from the same source tree. Pre-built packages are available for both databases: **Docker images**, an **Ubuntu PPA** (MySQL and MariaDB), and **Fedora/Enterprise-Linux RPMs** via Copr (MariaDB, plus MySQL on Fedora).

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

Packages are available via Launchpad PPA. Each Ubuntu release ships a plugin
built for its native MySQL and MariaDB servers — install the one matching your
database (the plugin is ABI-bound to the server's major.minor series):

| Ubuntu release | MySQL | MariaDB |
|---|---|---|
| 24.04 Noble | `pinba-engine-mysql-8.0` | `pinba-engine-mariadb-10.11` |
| 26.04 Resolute | `pinba-engine-mysql-8.4` | `pinba-engine-mariadb-11.8` |

```bash
sudo add-apt-repository ppa:xolegator/packages
sudo apt-get update
# MySQL (pick your release's series):
sudo apt-get install pinba-engine-mysql-8.0     # 24.04 Noble
# MariaDB (pick your release's series):
sudo apt-get install pinba-engine-mariadb-10.11 # 24.04 Noble
```

After install, load the plugin and initialize the schema:

```bash
mysql -u root -p -e "INSTALL PLUGIN pinba SONAME 'ha_pinba.so';"
mysql -u root -p -e "CREATE DATABASE IF NOT EXISTS pinba;"
mysql -u root -p pinba < /usr/share/pinba_engine/default_tables.sql
```

### Fedora / Enterprise Linux package (Copr RPM)

RPM packages are published to the [`xolegator/pinba`](https://copr.fedorainfracloud.org/coprs/xolegator/pinba/) Copr repository, in two mutually-exclusive flavors — install the one matching your database server (`x86_64`):

| Distribution | Database | Package |
|---|---|---|
| Fedora 43 / 44 | MariaDB | `pinba-engine-mariadb` |
| Fedora 43 / 44 | MySQL | `pinba-engine-mysql` |
| AlmaLinux / Rocky / RHEL 9 / 10 | MariaDB | `pinba-engine-mariadb` |

```bash
sudo dnf install dnf-plugins-core
sudo dnf copr enable xolegator/pinba
sudo dnf install pinba-engine-mariadb   # or pinba-engine-mysql (Fedora only)
```

The plugin (`ha_pinba.so`) and the shared schema (`pinba-engine-common`) are installed into the server's plugin and data dirs. After install, load the plugin and initialize the schema — use the `mariadb` client (or `mysql` for the MySQL flavor):

```bash
sudo mariadb -e "INSTALL PLUGIN pinba SONAME 'ha_pinba.so';"
sudo mariadb -e "CREATE DATABASE IF NOT EXISTS pinba;"
sudo mariadb pinba < /usr/share/pinba_engine/default_tables.sql
```

> On Enterprise Linux the native database is MariaDB, so only the MariaDB flavor is published there; the MySQL flavor is built for Fedora. The plugin is ABI-matched to each distribution's server version.

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
- [docs/tables.md](docs/tables.md) — all table types: core, report1–18, tag_report, tagN_info, rtag_* with COMMENT syntax
- [docs/releasing.md](docs/releasing.md) — release flow and commit message rules
- [API reference](https://xolegator.github.io/pinba_engine/) — generated Doxygen documentation

## License

This project is licensed under the [GNU General Public License v2.0](LICENSE) (GPL-2.0-only), inheriting the license of the original Pinba Engine by Antony Dovgal.

Copyright (c) 2007–2009 Antony Dovgal  
Copyright (c) 2025–2026 Oleg Ekhlakov

Third-party components bundled in this repository:

| Component | Files | License |
|---|---|---|
| [xxHash](https://github.com/Cyan4973/xxHash) | `src/xxhash.[ch]` | BSD-2-Clause |
