---
title: "MySQL Plugin postinst/prerm Patterns and Pitfalls"
type: concept
sources:
  - raw/sessions/ppa-packaging-session-2026-06-04.md
related:
  - wiki/concepts/debian-ppa-packaging.md
  - wiki/concepts/mysql-plugin-install.md
confidence: high
updated: 2026-06-04
---

# MySQL Plugin postinst/prerm — Patterns and Pitfalls

Справочник по написанию `postinst` и `prerm` скриптов для Debian/Ubuntu пакетов,
устанавливающих MySQL storage engine плагины. Все ловушки выявлены в ходе реальной
отладки на Ubuntu 24.04 + MySQL 8.0.46.

---

## Аутентификация в postinst (Ubuntu)

postinst запускается как `root` через `sudo dpkg -i`. Нужно подключиться к MySQL.

### Каскад методов

```sh
DEBIAN_CNF="/etc/mysql/debian.cnf"
MYSQL_SOCKET="/var/run/mysqld/mysqld.sock"

mysql_available() {
    # 1. debian-sys-maint через /etc/mysql/debian.cnf (600 root:root)
    if [ -r "$DEBIAN_CNF" ] && \
       mysql --defaults-file="$DEBIAN_CNF" -e "SELECT 1" >/dev/null 2>&1; then
        return 0
    fi
    # 2. root через auth_socket (работает если root@localhost без пароля)
    mysql --no-defaults -u root -S "$MYSQL_SOCKET" -e "SELECT 1" >/dev/null 2>&1
}

mysql_exec() {
    if [ -r "$DEBIAN_CNF" ]; then
        mysql --defaults-file="$DEBIAN_CNF" "$@"
    else
        mysql --no-defaults -u root -S "$MYSQL_SOCKET" "$@"
    fi
}
```

### /etc/mysql/debian.cnf

- Существует на всех Ubuntu MySQL установках (`mysql-server-8.0`, `mysql-community-server`)
- Содержит логин/пароль `debian-sys-maint`
- Права файла: `600 root:root` → читаем только из postinst (работает как root)
- `debian-sys-maint` имеет `ALL PRIVILEGES ON *.*` — SELECT, CREATE DATABASE, INSERT, etc.
- **НО:** в Ubuntu 24.04 MySQL 8.0 НЕ имеет `SYSTEM_VARIABLES_ADMIN` (нужен для INSTALL PLUGIN)

### Итог: что умеет debian-sys-maint

| Операция | Работает |
|----------|---------|
| SELECT, SHOW | ✓ |
| CREATE DATABASE | ✓ |
| source .sql (CREATE TABLE ENGINE=PINBA) | ✓ (если плагин загружен) |
| INSTALL PLUGIN | ✗ (нет SYSTEM_VARIABLES_ADMIN) |
| UNINSTALL PLUGIN | ✗ |

---

## Установка плагина: plugin-load-add vs INSTALL PLUGIN

### INSTALL PLUGIN

- Регистрирует плагин в `mysql.plugin` таблице (персистентно)
- Активирует немедленно
- **Требует** SYSTEM_VARIABLES_ADMIN или SUPER
- На Ubuntu 24.04 через `debian-sys-maint` **не работает**

### plugin-load-add (рекомендуется как основной метод)

```sh
cat > /etc/mysql/conf.d/pinba-engine.cnf << 'EOF'
[mysqld]
plugin-load-add = ha_pinba.so
EOF
```

- Не требует никаких MySQL привилегий — просто создать файл
- Плагин загружается при каждом старте MySQL
- **Не активирует немедленно** — нужен рестарт MySQL
- Работает совместно с INSTALL PLUGIN (если оба задействованы — дублирование, но без ошибок)

### Правильная стратегия постинста

```sh
# 1. Всегда пишем plugin-load-add (не требует привилегий, перезапуско-надёжно)
mkdir -p /etc/mysql/conf.d
printf '[mysqld]\nplugin-load-add = ha_pinba.so\n' > /etc/mysql/conf.d/pinba-engine.cnf

# 2. Пробуем INSTALL PLUGIN для немедленной активации (может не сработать)
if ! mysql_exec -e "INSTALL PLUGIN pinba SONAME 'ha_pinba.so';" >/dev/null 2>&1; then
    echo "plugin will load on next MySQL restart"
fi
```

---

## Синтаксические ловушки MySQL 8.0 vs 9.0

| Синтаксис | MySQL 8.0 | MySQL 9.0+ |
|-----------|-----------|------------|
| `INSTALL PLUGIN IF NOT EXISTS name SONAME '...'` | ❌ ERROR 1064 | ✓ |
| `UNINSTALL PLUGIN IF EXISTS name` | ❌ ERROR 1064 | ✓ |

**Правильно для MySQL 8.0** — проверка через information_schema:

```sh
# Проверить активен ли плагин
PLUGIN_ACTIVE=$(mysql_exec --skip-column-names \
    -e "SELECT COUNT(*) FROM information_schema.PLUGINS \
        WHERE PLUGIN_NAME='pinba' AND PLUGIN_STATUS='ACTIVE';" \
    2>/dev/null || echo "0")

# Установить только если не активен
if [ "${PLUGIN_ACTIVE:-0}" = "0" ]; then
    mysql_exec -e "INSTALL PLUGIN pinba SONAME 'ha_pinba.so';"
fi

# Выгрузить только если активен
if [ "${PLUGIN_ACTIVE:-0}" != "0" ]; then
    mysql_exec -e "UNINSTALL PLUGIN pinba;" || true
fi
```

---

## Создание базы и таблиц pinba

```sh
# Проверить существование базы (не ломается при отсутствии плагина)
DB_EXISTS=$(mysql_exec --skip-column-names \
    -e "SELECT COUNT(*) FROM information_schema.SCHEMATA \
        WHERE SCHEMA_NAME='pinba';" 2>/dev/null || echo "0")

if [ "${DB_EXISTS:-0}" = "0" ]; then
    mysql_exec -e "CREATE DATABASE pinba CHARACTER SET latin1;"
    # Импортировать таблицы — только если плагин АКТИВЕН прямо сейчас
    # (CREATE TABLE ENGINE=PINBA требует загруженного плагина)
    if plugin_active; then
        mysql_exec pinba < /usr/share/pinba_engine/default_tables.sql
    fi
fi
```

**Важно:** `CREATE TABLE ... ENGINE=PINBA` падает если плагин не загружен.
Если `INSTALL PLUGIN` не сработал (нет привилегий) — импортировать нельзя,
нужно инструктировать пользователя сделать это после рестарта MySQL.

---

## DROP DATABASE с ENGINE=PINBA таблицами

При удалении пакета (`dpkg -r`) `ha_pinba.so` удаляется с диска.
Если плагин был загружен через `INSTALL PLUGIN` — запись остаётся в `mysql.plugin`.

**Ловушка:** если попытаться `DROP DATABASE pinba` без загруженного плагина:
```
ERROR 1286 (42000): Unknown storage engine 'PINBA'
```
MySQL вызывает хендлер движка даже при DROP — если плагина нет, падает.

**Порядок правильной ручной очистки:**
```sql
-- 1. Сначала загрузить плагин
INSTALL PLUGIN pinba SONAME 'ha_pinba.so';
-- 2. Теперь можно дропнуть базу
DROP DATABASE pinba;
-- 3. Выгрузить плагин
UNINSTALL PLUGIN pinba;
```

---

## prerm: удаление конфига и выгрузка плагина

```sh
case "$1" in
    remove|purge)
        # Удалить plugin-load-add конфиг (чтобы плагин не загружался после удаления пакета)
        rm -f /etc/mysql/conf.d/pinba-engine.cnf

        # Выгрузить плагин если он сейчас активен
        if mysql_available 2>/dev/null; then
            ACTIVE=$(mysql_exec --skip-column-names \
                -e "SELECT COUNT(*) FROM information_schema.PLUGINS \
                    WHERE PLUGIN_NAME='pinba' AND PLUGIN_STATUS='ACTIVE';" \
                2>/dev/null || echo "0")
            if [ "${ACTIVE:-0}" != "0" ]; then
                mysql_exec -e "UNINSTALL PLUGIN pinba;" 2>&1 || true
            fi
        fi
        ;;
esac
```

---

## Output UX: [OK] / [--] / [!!]

Рекомендуется явно маркировать статус каждого шага:
- `[OK]` — выполнено успешно
- `[--]` — пропущено (ожидаемо, не ошибка)
- `[!!]` — предупреждение (выполнено частично)

Сырые MySQL ошибки нужно скрывать (`>/dev/null 2>&1`) на операциях, которые
могут ожидаемо упасть (INSTALL PLUGIN), и показывать только свои сообщения.

В конце — итоговый блок "Action required" если что-то нужно сделать вручную.
