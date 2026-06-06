---
title: "PPA Upload Session — 2026-06-06"
type: source
sources:
  - raw/sessions/ppa-upload-session-2026-06-06.md
related:
  - wiki/concepts/debian-ppa-packaging.md
  - wiki/concepts/launchpad-ppa-workflow.md
confidence: high
updated: 2026-06-06
---

# PPA Upload Session — 2026-06-06

Сессия закрыла ЭТАП 3 (orig tarball) и ЭТАП 4 (загрузка в PPA) плана `ai_ppa_dev.log`.

## Ключевые факты

- **orig.tar.gz** создаётся вручную через `git archive`, не через dpkg-buildpackage
- **UNRELEASED** в changelog Launchpad отклоняет — нужен реальный кодовое имя (`noble`)
- **Версионный формат**: `2.1.2-1~noble1` (оба MySQL серии в одном source → нет `~mysql8.0` суффикса)
- **FTP порт 21** часто заблокирован; dput поддерживает `method = sftp` (порт 22)
- **rapidjson-dev** обязателен в Build-Depends: MySQL 8.0/8.4 vendor headers тянут `rapidjson/fwd.h`
- **GPGKeyTemporarilyNotFoundError** — нормально при первом `add-apt-repository` после создания PPA; ключ появляется через 5–30 минут
- **Успех**: `pinba-engine 2.1.2-2~noble1` собран и установлен из PPA без ручных действий

## Коммиты

- `a14f8ac` — fix(packaging): add rapidjson-dev to Build-Depends

## Ссылки

- Build: https://launchpad.net/~xolegator/+archive/ubuntu/packages/+build/32942019
- PPA: https://launchpad.net/~xolegator/+archive/ubuntu/packages
