---
title: "Minimal MySQL Vendor Headers Strategy"
type: concept
sources:
  - raw/sessions/ppa-packaging-session-2026-06-04.md
related:
  - wiki/concepts/debian-ppa-packaging.md
  - wiki/concepts/cmake-build-system.md
  - wiki/concepts/mysql-plugin-abi.md
confidence: high
updated: 2026-06-04
---

# Minimal MySQL Vendor Headers Strategy

MySQL storage engine плагин требует заголовки из полного исходника MySQL
(например `sql/handler.h`), которых нет в `libmysqlclient-dev`.

Для offline сборки (Launchpad PPA, CI без сети) нужно вендорить эти заголовки.
Стратегия: хранить **только те файлы, которые реально включаются компилятором**.

---

## Проблема с наивным подходом

Простое `tar -xf mysql.tar.gz mysql-8.0.46/include/ mysql-8.0.46/sql/ ...`
извлекает ~1300 файлов (~14 MB) на серию. Большинство — внутренности MySQL
(boost patches, GIS, join optimizer, data dictionary impl, range optimizer...),
которые pinba никогда не включает.

**Реальные цифры:**
| | До | После |
|---|---|---|
| Файлов (8.0) | 1317 | 162 |
| Файлов (8.4) | ~1317 | 175 |
| Размер на серию | ~14 MB | ~2.2 MB |
| Оба вместе в git | ~28 MB | ~4.5 MB |

---

## Как получить минимальный список

Компилятор при сборке записывает все реально включённые файлы в `.d` файлы
(Makefile dependency files). CMake создаёт их автоматически.

### Шаг 1: сделать сборку

```bash
cmake -B build-analyze \
    -DPINBA_MYSQL_SOURCE_DIR=vendor/mysql-headers-8.0 \
    -DPINBA_MYSQL_SOURCE_VERSION=8.0.46 \
    -DPINBA_WITH_TESTS=OFF
cmake --build build-analyze
```

### Шаг 2: извлечь список файлов из .d

```python
import re, glob

vendor_dir = '/path/to/vendor/mysql-headers-8.0'
pattern = rf'{re.escape(vendor_dir)}/([^\s\\\\]+)'
used = set()
for f in glob.glob('build-analyze/CMakeFiles/pinba_engine.dir/**/*.d', recursive=True):
    for line in open(f):
        for m in re.findall(pattern, line):
            p = m.strip()
            if not p.startswith('builddir/'):  # builddir/ — cmake-generated, не из tarball
                used.add(p)
print(f'{len(used)} files actually used')
```

### Шаг 3: объединить для всех серий

Некоторые пути отличаются между сериями:
- MySQL 8.0: `libbinlogevents/export/binary_log_funcs.h`
- MySQL 8.4: `libs/mysql/binlog/event/export/binary_log_funcs.h`

Нужно взять union множеств из 8.0 и 8.4 — тогда при извлечении файлы,
которых нет в конкретном tarball, просто пропускаются.

---

## Алгоритм extract-mysql-headers.sh

```bash
# 1. Скачать tarball (~300 MB)
wget "$URL" -O "$TMPDIR/mysql.tar.gz"

# 2. Получить оглавление (один проход, читает только заголовки tar)
tar -tf "$TMPDIR/mysql.tar.gz" > "$TMPDIR/archive-index.txt"

# 3. Пересечь с whitelist
for rel in $NEEDED_FILES; do
    tarpath="mysql-${VERSION}/${rel}"
    if grep -qF "$tarpath" "$TMPDIR/archive-index.txt"; then
        echo "$tarpath" >> "$TMPDIR/to-extract.txt"
    fi
done

# 4. Один раз распаковать только нужные файлы
tar -xf "$TMPDIR/mysql.tar.gz" \
    --strip-components=1 -C "$DEST" \
    -T "$TMPDIR/to-extract.txt"
```

Две операции над tarball: одна для оглавления, одна для распаковки.
Сравнивать с подходом "распаковать всё, потом удалить" — результат тот же,
но более явно и без мусора в промежуточном состоянии.

---

## Что НЕ нужно (было удалено)

| Директория | Содержимое | Причина удаления |
|------------|-----------|------------------|
| `include/boost_1_77_0/` | Boost geometry patches | MySQL внутреннее, pinba не использует |
| `sql/gis/` (кроме srid.h) | ГИС функции | MySQL внутреннее |
| `sql/join_optimizer/` | Оптимизатор JOIN | MySQL внутреннее |
| `sql/range_optimizer/` | Range scan optimizer | MySQL внутреннее |
| `sql/dd/impl/` | Data Dictionary реализация | Нужны только типы (dd/types/) |
| `sql/auth/` (кроме auth_acls.h) | Authentication internals | Нужен только acls |
| `sql/iterators/` | Row iterators | MySQL внутреннее |
| `sql/server_component/` | Server component реализация | Нужны только биты |
| `include/authentication_kerberos*` | Kerberos опции | Клиентские опции, не нужны плагину |

---

## Что нужно (162 файла для 8.0)

**include/** (111 файлов):
- `my_*.h` — базовые MySQL типы и утилиты
- `mysql.h`, `mysql_com.h`, `mysql_time.h` — клиентский API
- `mysql/plugin.h` — Plugin API (главный!)
- `mysql/psi/*.h` — Performance Schema instrumentation
- `mysql/components/services/bits/*.h` — битовые типы для компонентов
- `mysql/service_*.h` — сервисы для плагинов

**sql/** (46 файлов):
- `handler.h`, `table.h`, `field.h` — ядро storage engine API
- `key.h`, `mdl.h`, `sql_plugin.h` — зависимости handler.h
- `dd/types/*.h` — типы Data Dictionary
- `gis/srid.h` — только этот один из gis/

**libbinlogevents/** (3 файла, только в 8.0):
- `export/binary_log_funcs.h`
- `include/table_id.h`
- `include/wrapper_functions.h`

**libs/mysql/binlog/** (3 файла, только в 8.4 — замена libbinlogevents/):
- Те же три файла, другой путь

---

## Обновление whitelist при изменении кода

Если в pinba_engine добавляется новый `#include <sql/some_new.h>`:

1. Сделать сборку: `cmake --build build`
2. Запустить python-скрипт выше для 8.0 и 8.4
3. Добавить новые файлы в `NEEDED_FILES` в `tools/extract-mysql-headers.sh`
4. Перезапустить `tools/extract-mysql-headers.sh 8.0 8.0.XX` и `8.4 8.4.XX`
5. Закоммитить обновлённые `vendor/mysql-headers-*/`
