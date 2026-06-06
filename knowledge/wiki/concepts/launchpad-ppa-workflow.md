---
title: "Launchpad PPA Upload Workflow"
type: concept
sources:
  - raw/sessions/ppa-upload-session-2026-06-06.md
related:
  - wiki/concepts/debian-ppa-packaging.md
  - wiki/concepts/github-actions-ppa.md
confidence: high
updated: 2026-06-06
---

# Launchpad PPA Upload Workflow

Практический процесс подписания и загрузки source package в Launchpad PPA,
включая known pitfalls.

## Требования

| Что нужно | Где |
|-----------|-----|
| GPG ключ зарегистрирован на Launchpad | https://launchpad.net/~{user}/+editpgpkeys |
| SSH ключ зарегистрирован на Launchpad | https://launchpad.net/~{user}/+editsshkeys |
| `devscripts`, `dput` установлены | `sudo apt install devscripts dput` |

## dput конфигурация

**ВАЖНО:** FTP (порт 21) часто заблокирован. Всегда использовать SFTP.

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

Проверка: `ssh -T xolegator@ppa.launchpad.net` → `No shells on this server.` ✓

## Полный цикл загрузки

```bash
# 1. Создать orig.tar.gz из git (только если изменился upstream)
git archive --prefix=pinba-engine-{VERSION}/ HEAD | gzip -9 > ../pinba-engine_{VERSION}.orig.tar.gz

# 2. Собрать source package (без компиляции, без подписи)
dpkg-buildpackage -S -sa -us -uc [-d]
#   -S   = source only
#   -sa  = include orig.tar.gz
#   -us  = unsigned .changes
#   -uc  = unsigned .dsc
#   -d   = skip dependency check (если Build-Depends не установлены локально)

# 3. Подписать (требует интерактивного терминала для GPG pinentry)
debsign -k{FINGERPRINT} ../pinba-engine_{VERSION}_source.changes

# 4. Загрузить
dput xolegator-packages ../pinba-engine_{VERSION}_source.changes
```

Ожидать письмо на email мейнтейнера: Launchpad подтверждает принятие или отклонение.

## Версионирование

Формат: `{upstream}-{deb_rev}~{ubuntu_codename}{n}`

| Случай | Версия |
|--------|--------|
| Первый upload для noble | `2.1.2-1~noble1` |
| Исправление packaging (тот же upstream) | `2.1.2-2~noble1` |
| Новый Ubuntu | `2.1.2-1~oracular1` |

Правило: Launchpad **не принимает** повторную загрузку той же версии.
При любом изменении — поднять revision (`-1` → `-2`).

## Мониторинг сборки

Страница сборок: `https://launchpad.net/~xolegator/+archive/ubuntu/packages/+builds`

Статусы: Needs building → Building → Successfully built / Failed to build

При падении: ссылка на build log в письме от Launchpad.

## Распространение ключа PPA при первой публикации

После первой загрузки Launchpad генерирует подписывающий ключ PPA.
Ключ появляется на keyserver.ubuntu.com с задержкой 5–30 минут.

До появления ключа `add-apt-repository` возвращает:
```
ServerError: HTTP Error 500
GPGKeyTemporarilyNotFoundError
```

Это временная ошибка — подождать и повторить.

## Установка на клиентской машине

```bash
# Штатный способ (ждать распространения ключа):
sudo add-apt-repository ppa:xolegator/packages
sudo apt update
sudo apt install pinba-engine-mysql-8.0

# Если add-apt-repository создал дубль sources после ручного добавления:
sudo rm /etc/apt/sources.list.d/xolegator-packages.list
```

`add-apt-repository` создаёт файл в формате deb822 (`.sources`), ручное добавление создаёт
старый формат (`.list`) — они дублируют друг друга, нужно оставить только один.

## Известные проблемы сборки на Launchpad

### rapidjson/fwd.h не найден

Симптом:
```
fatal error: rapidjson/fwd.h: No such file or directory
In file included from sql/dd/sdi_fwd.h:27
```

Причина: MySQL 8.0/8.4 plugin headers требуют rapidjson, но он не в vendor/ и не в Build-Depends.

Решение: добавить `rapidjson-dev` в Build-Depends (`debian/control`).

Launchpad строит в чистом chroot — stale cmake-кэш локальной машины маскирует проблему.

### UNRELEASED в changelog

Симптом: Launchpad отклоняет upload (rejection letter на email).

Решение: changelog должен содержать реальное кодовое имя (`noble`, `jammy`), не `UNRELEASED`.
