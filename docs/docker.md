# Docker Guide

## Build Image

```bash
docker build -t pinba-engine:latest -f Dockerfile .
```

## Run Container

```bash
docker run -d \
  --name pinba-mysql \
  -e MYSQL_ROOT_PASSWORD=yourpassword \
  -e MYSQL_DATABASE=pinba \
  -p 3306:3306 \
  -p 30002:30002/udp \
  pinba-engine:latest
```

## Verify Plugin

```bash
docker exec -it pinba-mysql mysql -uroot -pyourpassword -e "SHOW PLUGINS;"
```

The output must include `pinba` with status `ACTIVE`.

## Persistent Data

```bash
docker volume create pinba-data

docker run -d \
  --name pinba-mysql \
  -e MYSQL_ROOT_PASSWORD=yourpassword \
  -e MYSQL_DATABASE=pinba \
  -v pinba-data:/var/lib/mysql \
  -p 3306:3306 \
  -p 30002:30002/udp \
  pinba-engine:latest
```

## Troubleshooting

- Check container logs with `docker logs pinba-mysql`.
- Check plugin directory in MySQL with `SHOW VARIABLES LIKE 'plugin_dir';`.
- Check the MySQL error log for plugin load failures.
