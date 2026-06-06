---
title: "Debian/PPA Packaging for MySQL Storage Engine Plugins"
type: concept
sources:
  - raw/sessions/ppa-packaging-session-2026-06-04.md
  - raw/sessions/ppa-upload-session-2026-06-06.md
related:
  - wiki/concepts/mysql-postinst-patterns.md
  - wiki/concepts/mysql-vendor-headers-minimal.md
  - wiki/concepts/mysql-plugin-install.md
  - wiki/concepts/cmake-build-system.md
  - wiki/concepts/launchpad-ppa-workflow.md
confidence: high
updated: 2026-06-06
---

# Debian/PPA Packaging for MySQL Storage Engine Plugins

Как собрать `.deb` пакет для MySQL storage engine плагина и опубликовать его
в Launchpad PPA. На примере pinba_engine v2.1.2.

## Структура debian/

```
debian/
├── control           — Source + Binary пакеты, Build-Depends
├── rules             — cmake конфигурация для каждой MySQL серии
├── changelog         — версия пакета, дистрибутив (UNRELEASED или noble)
├── copyright         — DEP-5 формат
├── source/format     — "3.0 (quilt)"
├── pinba-engine-common.install         — SQL файлы в /usr/share/pinba_engine/
├── pinba-engine-mysql-8.0.install      — ha_pinba.so в /usr/lib/mysql/plugin/
├── pinba-engine-mysql-8.4.install
├── pinba-engine-mysql-8.0.postinst     — установка плагина + создание БД
├── pinba-engine-mysql-8.0.prerm        — выгрузка плагина
├── pinba-engine-mysql-8.4.postinst
└── pinba-engine-mysql-8.4.prerm
```

## Схема версионирования

```
{upstream_version}~{ubuntu_codename}~mysql{major}-{deb_revision}

Примеры:
  2.1.2~ubuntu24.04~mysql8.0-1   (Noble + MySQL 8.0)
  2.1.2~ubuntu24.04~mysql8.4-1   (Noble + MySQL 8.4)
```

Символ `~` меньше любого суффикса: `2.1.2~... < 2.1.2` — upstream считается новее.

## debian/rules

Два отдельных cmake-конфига для одного source package:

```makefile
override_dh_auto_configure:
    cmake -B debian/build-mysql80 \
      -DPINBA_MYSQL_SERIES=8.0 \
      -DPINBA_MYSQL_SOURCE_DIR=$(CURDIR)/vendor/mysql-headers-8.0 \
      -DPINBA_MYSQL_SOURCE_VERSION=8.0.46 \
      -DPINBA_WITH_TESTS=OFF \
      -DPINBA_FETCH_DEPENDENCIES=OFF \
      -DCMAKE_INSTALL_LIBDIR=lib
    cmake -B debian/build-mysql84 ...

override_dh_auto_build:
    cmake --build debian/build-mysql80 --parallel $(NPROC)
    cmake --build debian/build-mysql84 --parallel $(NPROC)

override_dh_auto_install:
    DESTDIR=$(CURDIR)/debian/tmp-mysql80 cmake --install debian/build-mysql80
    DESTDIR=$(CURDIR)/debian/tmp-mysql84 cmake --install debian/build-mysql84

override_dh_install:
    dh_install --sourcedir=debian/tmp-mysql80 -ppinba-engine-mysql-8.0
    dh_install --sourcedir=debian/tmp-mysql84 -ppinba-engine-mysql-8.4
    dh_install --sourcedir=debian/tmp-mysql80 -ppinba-engine-common
```

## Вендоринг MySQL headers

MySQL plugin API требует `sql/handler.h` — заголовок из полного исходника MySQL
(НЕТ в `libmysqlclient-dev`). Launchpad строит в изолированном chroot без сети.

**Решение:** вендорить только нужные заголовки (162/175 файлов, ~2.2 MB на серию)
в `vendor/mysql-headers-{series}/` и коммитить в git.

Подробнее: [[mysql-vendor-headers-minimal]].

## Сборка и тест

```bash
# Собрать бинарные пакеты (без подписи):
dpkg-buildpackage -us -uc -b

# Проверить lintian:
lintian --fail-on error pinba-engine-mysql-8.0_*.deb

# Установить:
sudo dpkg -i pinba-engine-common_*.deb pinba-engine-mysql-8.0_*.deb
```

Ожидаемые lintian warnings (не критичные):
- `initial-upload-closes-no-bugs` — для PPA нормально, ITP не нужен
- `debian-changelog-has-wrong-day-of-week` — проверить вручную день недели

## Launchpad PPA

PPA: `ppa:xolegator/packages`

```bash
# Source package (для загрузки в PPA):
dpkg-buildpackage -S -sa

# Подпись:
debsign -k{FINGERPRINT} pinba-engine_*_source.changes

# Загрузка:
dput ppa:xolegator/packages pinba-engine_*_source.changes
```

orig.tar.gz включает vendor/mysql-headers-* автоматически (~5 MB суммарно).

## postinst/prerm — детали

Подробно: [[mysql-postinst-patterns]].

Краткая схема:
1. postinst: пишет `/etc/mysql/conf.d/pinba-engine.cnf` (`plugin-load-add`)
2. postinst: пробует `INSTALL PLUGIN` для немедленной активации (может упасть, не фатально)
3. postinst: создаёт БД `pinba` и импортирует `default_tables.sql` (только если плагин активен)
4. prerm: удаляет конфиг, выгружает плагин если загружен

---

## Создание orig.tar.gz и source package

`dpkg-buildpackage -S -sa` **не создаёт** orig.tar.gz сам — он ожидает `../pinba-engine_{VERSION}.orig.tar.gz`.
Создавать вручную через git archive перед каждой PPA загрузкой:

```bash
git archive --prefix=pinba-engine-2.1.2/ HEAD | gzip -9 > ../pinba-engine_2.1.2.orig.tar.gz
dpkg-buildpackage -S -sa -us -uc
```

`vendor/mysql-headers-*` закоммичены → попадают в orig автоматически. Итоговый размер: ~1.1 MB.

Два файла обязательны для корректной source сборки:
- `debian/clean` — список cmake build-деревьев для удаления (иначе "unwanted binary" ошибка)
- `debian/source/options` с `--extend-diff-ignore` — исключить нетрекаемые локальные файлы

## Версионирование для PPA (уточнение)

Финальный выбор формата: `{upstream}-{deb_rev}~{codename}{n}`

Пример: `2.1.2-1~noble1` (оба MySQL серии в одном source package → нет суффикса `~mysql8.0`)

Кратность `-N` (deb revision) увеличивается при каждом повторном upload одной и той же upstream версии.
Launchpad не принимает повторную загрузку той же версии — нужно поднять revision.

## Загрузка в Launchpad (dput)

**Критично:** порт 21 (FTP) часто заблокирован провайдерами/роутерами.
Launchpad поддерживает SFTP через порт 22 — всегда настраивать `method = sftp`.

```ini
# ~/.dput.cf
[xolegator-packages]
fqdn = ppa.launchpad.net
method = sftp
incoming = ~xolegator/packages/ubuntu/
login = xolegator
allow_unsigned_uploads = 0
```

```
# ~/.ssh/config
Host ppa.launchpad.net
    User xolegator
    IdentityFile ~/.ssh/id_ed25519_launchpad
    IdentitiesOnly yes
```

Проверка SSH: `ssh -T xolegator@ppa.launchpad.net` → ответ `No shells on this server.` — это норма.

Подписание и загрузка:
```bash
debsign -k{FINGERPRINT} ../pinba-engine_*_source.changes
dput xolegator-packages ../pinba-engine_*_source.changes
```

`debsign` требует интерактивного терминала (GPG pinentry). Запускать вручную, не через скрипт без TTY.

## rapidjson-dev обязателен в Build-Depends

MySQL 8.0 и 8.4 vendor headers тянут `<rapidjson/fwd.h>` через цепочку:

```
ha_pinba.cc → sql/table.h → sql/dd/types/foreign_key.h → sql/dd/sdi_fwd.h → rapidjson/fwd.h
```

В `vendor/mysql-headers-*` этот файл отсутствует (он из `extra/rapidjson/` полного MySQL source).
Локальная сборка могла работать из-за stale cmake-кэша с полным MySQL source.
Launchpad строит в чистом chroot → `rapidjson/fwd.h` не найден → fatal error.

**Решение:** `rapidjson-dev` в `debian/control` Build-Depends.
В Ubuntu Noble: `rapidjson-dev 1.1.0+dfsg2-7.2`, доступен в universe.

## Установка из PPA — особенности

После первой публикации в PPA, `add-apt-repository` может вернуть `GPGKeyTemporarilyNotFoundError`
(Launchpad 500). Это временная ошибка — ключ PPA генерируется с задержкой 5–30 минут.

Если нужен немедленный доступ — добавить source вручную:
```bash
echo "deb https://ppa.launchpadcontent.net/xolegator/packages/ubuntu/ noble main" \
  | sudo tee /etc/apt/sources.list.d/xolegator-packages.list
sudo gpg --keyserver keyserver.ubuntu.com --recv-keys {FINGERPRINT}
sudo gpg --export {FINGERPRINT} | sudo tee /etc/apt/trusted.gpg.d/xolegator-packages.gpg > /dev/null
```

После успешного `add-apt-repository` удалить ручной `.list` файл во избежание дублирования источников.
