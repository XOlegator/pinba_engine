# Docker Guide (MySQL 8 + Pinba Engine)

This guide explains how to build, validate, and publish a Docker image with Pinba Engine for MySQL 8.

## 1. Build the image

From the repository root:

```bash
docker build -t <registry-user>/pinba-engine:8.0 -f Dockerfile .
```

Optional version-specific tag:

```bash
docker tag <registry-user>/pinba-engine:8.0 <registry-user>/pinba-engine:<mysql-version>
# example: <registry-user>/pinba-engine:8.0.45
```

## 2. Quick runtime validation

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

## 3. Publish image

```bash
docker login

docker push <registry-user>/pinba-engine:8.0
# optional
# docker push <registry-user>/pinba-engine:<mysql-version>
```

## 4. Tagging strategy

Recommended tags:

- `<registry-user>/pinba-engine:8.0` as stable MySQL 8 channel.
- `<registry-user>/pinba-engine:<mysql-version>` for exact pinning.

## 5. Troubleshooting

- Check logs: `docker logs pinba-mysql-test`.
- Check plugin directory: `SHOW VARIABLES LIKE 'plugin_dir';`.
- If plugin is not active, verify SQL init scripts in `/docker-entrypoint-initdb.d`.
