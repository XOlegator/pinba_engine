# Docker Guide (MySQL 8.0 / 8.4 LTS and MariaDB 10.11 / 11.8 LTS + Pinba Engine)

This guide explains how to run the published Pinba Engine images in production, how tags
and digests behave, and how to build, validate, and publish the images yourself.

## 1. Images, tags, and architectures

Pre-built images are published to Docker Hub:

| Database | Tag | Architectures |
|---|---|---|
| MySQL 8.0 | `xolegator/pinba-engine:8.0` | `linux/amd64` |
| MySQL 8.4 LTS | `xolegator/pinba-engine:8.4` | `linux/amd64`, `linux/arm64` |
| MariaDB 10.11 LTS | `xolegator/pinba-engine:mariadb-10.11` | `linux/amd64`, `linux/arm64` |
| MariaDB 11.8 LTS | `xolegator/pinba-engine:mariadb-11.8` | `linux/amd64`, `linux/arm64` |

`xolegator/pinba-engine:latest` is an alias for the MySQL 8.4 LTS channel.

Each channel also gets a version-suffixed tag per Pinba release, e.g.
`xolegator/pinba-engine:8.4-v2.11.3` — but see
[Tag stability and digest pinning](#3-tag-stability-and-digest-pinning) before using it
as an immutable reference.

The MySQL 8.0 channel is amd64-only because the upstream `mysql:8.0` Debian
(`-bookworm`) base image is not published for arm64.

## 2. Run in production

The images extend the official `mysql` / `mariadb` images, so all upstream
configuration mechanisms (environment variables, `/etc/mysql/conf.d`,
`/docker-entrypoint-initdb.d`) work unchanged.

**Always mount a named volume at `/var/lib/mysql`.** The base images declare an
anonymous volume there: without an explicit mount your data silently lands in an
anonymous volume that is orphaned by `docker rm`, and every container recreation
starts from scratch. A named volume preserves user accounts and grants, the Pinba
plugin registration (`mysql.plugin`), and any regular tables you create (e.g.
report snapshots aggregated from Pinba data).

Note that Pinba's own stats tables live in memory by design — metrics are
rebuilt from incoming UDP traffic and rotated after `pinba_stats_history`
seconds — so the volume does not (and cannot) preserve Pinba metrics across
restarts. The volume is for everything else that makes the instance yours.

Minimal `docker run`:

```bash
docker run -d \
  --name pinba \
  --restart unless-stopped \
  -e MYSQL_ROOT_PASSWORD=change-me \
  -p 3306:3306 \
  -p 30002:30002/udp \
  -v pinba-data:/var/lib/mysql \
  xolegator/pinba-engine:8.4
```

Equivalent Docker Compose service with a healthcheck:

```yaml
services:
  pinba:
    # For immutable deployments pin by digest instead of the channel tag,
    # see "Tag stability and digest pinning" below.
    image: xolegator/pinba-engine:8.4
    restart: unless-stopped
    environment:
      MYSQL_ROOT_PASSWORD: change-me
    ports:
      - "3306:3306"
      - "30002:30002/udp"
    volumes:
      - pinba-data:/var/lib/mysql
      # Optional: your own MySQL/Pinba settings (any pinba_* variable needs the
      # loose_ prefix so mysqld --initialize does not abort before the plugin
      # is registered):
      # - ./my-pinba.cnf:/etc/mysql/conf.d/zz-my-pinba.cnf:ro
    healthcheck:
      test: ["CMD-SHELL", "mysqladmin ping -h 127.0.0.1 -uroot -p\"$$MYSQL_ROOT_PASSWORD\" --silent"]
      interval: 10s
      timeout: 5s
      retries: 5
      start_period: 30s

volumes:
  pinba-data:
```

For the MariaDB images use `MARIADB_ROOT_PASSWORD` and the built-in healthcheck
script instead:

```yaml
    healthcheck:
      test: ["CMD", "healthcheck.sh", "--connect", "--innodb_initialized"]
```

Things to know for long-term operation:

- **First start only:** the plugin installation and the `pinba` schema are
  created by `/docker-entrypoint-initdb.d` scripts, which the official
  entrypoint runs only when the data directory is empty. With a persistent
  volume this happens exactly once; later image upgrades reuse the plugin
  registration stored in the data directory (the plugin binary itself always
  comes from the image).
- **Upgrades:** stay within one channel (e.g. `8.4`). The MySQL/MariaDB
  data-directory upgrade rules of the upstream images apply unchanged.
- **UDP ingestion:** Pinba listens on `30002/udp` inside the container. Publish
  the port (as above) or attach the container to the network your PHP/pinba
  clients use.

## 3. Tag stability and digest pinning

All published tags are **mutable**, including the version-suffixed ones:

- Channel tags (`8.0`, `8.4`, `mariadb-10.11`, `mariadb-11.8`, `latest`) move
  with every Pinba release.
- Version tags (e.g. `8.4-v2.11.3`) are re-pushed when the base MySQL/MariaDB
  image gets a patch update: an automated workflow bumps the base image and
  rebuilds, so the same tag can start pointing to a different image between
  Pinba releases.

For reproducible production deployments pin by digest:

```bash
docker pull xolegator/pinba-engine@sha256:29d1f4fa39bc1d22683e9d2ee502138d04bc438a45adc63d73484b0095b9cfe8 # 8.4 channel
```

Current digests, updated automatically by CI on every image rebuild:

| Channel | Digest |
|---|---|
| `8.0` | `sha256:f1af3b856c72f2cd06c31a7f9bd1414c42c56c5c5e26050232df8f1af83d085c` |
| `8.4` | `sha256:29d1f4fa39bc1d22683e9d2ee502138d04bc438a45adc63d73484b0095b9cfe8` |
| `mariadb-10.11` | `sha256:98a08d9085e870e57b24f5386e54082e9b16d0c8d7759b944aef7e9dd60a814e` |
| `mariadb-11.8` | `sha256:e9dbcec3d587350bafcdb03f54a69aefd4132186c2a157633806d2698d2d915f` |

To resolve a digest yourself:

```bash
docker buildx imagetools inspect xolegator/pinba-engine:<tag>
```

## 4. Build the image

From the repository root.
Use separate Dockerfiles to keep the channels isolated:

```bash
docker build -t <registry-user>/pinba-engine:8.0 -f Dockerfile.mysql80 .
docker build -t <registry-user>/pinba-engine:8.4 -f Dockerfile.mysql84 .
docker build -t <registry-user>/pinba-engine:mariadb-10.11 -f Dockerfile.mariadb1011 .
docker build -t <registry-user>/pinba-engine:mariadb-11.8 -f Dockerfile.mariadb118 .
```

Multi-arch build (amd64 + arm64; not supported for `Dockerfile.mysql80`, see
[section 1](#1-images-tags-and-architectures)):

```bash
docker buildx build --platform linux/amd64,linux/arm64 \
  -t <registry-user>/pinba-engine:8.4 -f Dockerfile.mysql84 --push .
```

Optional version-specific tag:

```bash
docker tag <registry-user>/pinba-engine:8.0 <registry-user>/pinba-engine:<mysql-version>
# example: <registry-user>/pinba-engine:8.0.45
```

## 5. Quick runtime validation

```bash
docker run -d \
  --name pinba-mysql-test \
  -e MYSQL_ROOT_PASSWORD=changeme \
  -e MYSQL_DATABASE=pinba \
  -p 3306:3306 \
  -p 30002:30002/udp \
  <registry-user>/pinba-engine:8.0
```

Validate plugin status:

```bash
docker exec -it pinba-mysql-test \
  mysql -uroot -pchangeme -e "SHOW PLUGINS LIKE 'pinba';"
```

Validate Pinba schema:

```bash
docker exec -it pinba-mysql-test \
  mysql -uroot -pchangeme -D pinba -e "SHOW TABLES;"
```

Clean up test container:

```bash
docker rm -f pinba-mysql-test
```

## 6. Publish image

Publishing to Docker Hub is automated by `.github/workflows/docker.yml` (runs on
release, on pushes touching the Docker inputs, and on manual dispatch). Manual
publishing, if ever needed:

```bash
docker login

docker push <registry-user>/pinba-engine:8.0
# optional
# docker push <registry-user>/pinba-engine:<mysql-version>
```

## 7. Troubleshooting

- Check logs: `docker logs pinba-mysql-test`.
- Check plugin directory: `SHOW VARIABLES LIKE 'plugin_dir';`.
- If plugin is not active, verify SQL init scripts in `/docker-entrypoint-initdb.d`
  and remember they only run when the data directory is empty (first start of a
  fresh volume).
