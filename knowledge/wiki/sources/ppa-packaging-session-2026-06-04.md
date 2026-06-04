---
title: "Session: Debian PPA Packaging (2026-06-04)"
type: source
sources:
  - raw/sessions/ppa-packaging-session-2026-06-04.md
related:
  - wiki/concepts/debian-ppa-packaging.md
  - wiki/concepts/mysql-postinst-patterns.md
  - wiki/concepts/mysql-vendor-headers-minimal.md
  - wiki/concepts/mysql-plugin-install.md
confidence: high
updated: 2026-06-04
---

# Source: Debian PPA Packaging Session — 2026-06-04

Живая сессия отладки и реализации Debian/PPA пакетирования pinba_engine.
Прошла через несколько итераций исправлений postinst/prerm.

## Ключевые факты

- ЭТАП 1.5 завершён: vendor headers добавлены в git (commit `df68221`)
- ЭТАП 2 завершён: `dpkg-buildpackage -us -uc -b` работает, lintian чист
- Тест установки пройден: всё полностью автоматически при `sudo dpkg -i`
- vendor/mysql-headers-* оптимизированы: 1317→162/175 файлов, 14→2.2 MB на серию

## Проблемы, обнаруженные при тестировании

| Проблема | Корневая причина | Решение |
|----------|-----------------|---------|
| postinst молчал, база не создавалась | `2>/dev/null` скрывал ошибки | Убрать подавление с рабочих команд |
| `INSTALL PLUGIN IF NOT EXISTS` → ERROR 1064 | Синтаксис MySQL 9.0+, не 8.0 | Проверка через `information_schema.PLUGINS` |
| `UNINSTALL PLUGIN IF EXISTS` → ERROR 1064 | То же | То же |
| ERROR 3883: Operation not permitted | `debian-sys-maint` лишён SYSTEM_VARIABLES_ADMIN | `plugin-load-add` конфиг как основной путь |
| `DROP DATABASE pinba` → Unknown storage engine | Плагин не загружен, таблицы ENGINE=PINBA | Загрузить плагин → дроп → выгрузить |
| postinst "MySQL not running" при живом MySQL | root@localhost защищён паролем, не auth_socket | Fallback через `/etc/mysql/debian.cnf` |

## Commits

- `df68221` — vendor headers 8.0.46 + 8.4.9
- `db66097` — fix postinst/prerm (debian.cnf, plugin-load-add, правильный синтаксис)
- `d2a19fb` — gitignore vendor/*/builddir/
- `57ed76a` — trim vendor headers (1317→162/175 файлов)

## Релевантные концепты

- [[debian-ppa-packaging]] — полный процесс PPA packaging для MySQL плагина
- [[mysql-postinst-patterns]] — паттерны и ловушки postinst/prerm для MySQL
- [[mysql-vendor-headers-minimal]] — стратегия минимизации vendor headers
