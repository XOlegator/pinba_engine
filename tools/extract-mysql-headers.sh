#!/usr/bin/env bash
#
# Extract the minimal set of MySQL server plugin headers needed to build
# ha_pinba.so.  Extracts ~179 files (~1.5 MB) instead of whole directories
# (~1300 files, ~14 MB).
#
# The file list below was derived from compiler dependency files (.d) of a
# successful build against MySQL 8.0.46 and 8.4.9.  It is the union of both
# series: files absent from a given tarball are silently skipped (e.g.
# libbinlogevents/ exists in 8.0 but moved to libs/mysql/binlog/ in 8.4).
#
# Usage:
#   bash tools/extract-mysql-headers.sh 8.0 8.0.46
#   bash tools/extract-mysql-headers.sh 8.4 8.4.9
#   git add vendor/mysql-headers-8.0 vendor/mysql-headers-8.4
#   git commit -m "chore: update MySQL headers 8.0.XX / 8.4.XX"
#
# To refresh after a pinba source change that adds new MySQL includes:
#   1. Run a full build:  cmake -B build ... && cmake --build build
#   2. Extract used paths from .d files:
#        grep -h 'vendor/mysql-headers' build/CMakeFiles/pinba_engine.dir/**/*.d \
#          | tr ' \\' '\n' | grep vendor | sed 's|.*/vendor/mysql-headers-8\.0/||' \
#          | sort -u
#   3. Update the NEEDED_FILES list below.

set -euo pipefail

SERIES="${1:-}"
VERSION="${2:-}"

if [ -z "$SERIES" ] || [ -z "$VERSION" ]; then
    echo "Usage: $0 SERIES VERSION  (e.g. $0 8.0 8.0.46)" >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DEST="$REPO_ROOT/vendor/mysql-headers-$SERIES"
URL="https://dev.mysql.com/get/Downloads/MySQL-${SERIES}/mysql-${VERSION}.tar.gz"

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

echo "==> Downloading MySQL ${VERSION} source (~300 MB, only headers will be kept)..."
wget --quiet --show-progress "$URL" -O "$TMPDIR/mysql.tar.gz"

# Minimal list of headers actually required to build ha_pinba.so.
# Union of 8.0 and 8.4; files missing from the tarball are silently skipped.
NEEDED_FILES="
include/decimal.h
include/dig_vec.h
include/errmsg.h
include/field_types.h
include/ft_global.h
include/lex_string.h
include/little_endian.h
include/m_ctype.h
include/m_string.h
include/map_helpers.h
include/mem_root_deque.h
include/memory_debugging.h
include/my_alloc.h
include/my_base.h
include/my_bitmap.h
include/my_byteorder.h
include/my_char_traits.h
include/my_checksum.h
include/my_command.h
include/my_compiler.h
include/my_compress.h
include/my_dbug.h
include/my_dir.h
include/my_double2ulonglong.h
include/my_inttypes.h
include/my_io.h
include/my_list.h
include/my_loglevel.h
include/my_macros.h
include/my_pointer_arithmetic.h
include/my_psi_config.h
include/my_sharedlib.h
include/my_sqlcommand.h
include/my_sys.h
include/my_systime.h
include/my_table_map.h
include/my_thread.h
include/my_thread_local.h
include/my_time.h
include/my_time_t.h
include/mysql.h
include/mysql/attribute.h
include/mysql/client_plugin.h
include/mysql/com_data.h
include/mysql/components/service.h
include/mysql/components/services/bits/my_io_bits.h
include/mysql/components/services/bits/my_thread_bits.h
include/mysql/components/services/bits/mysql_cond_bits.h
include/mysql/components/services/bits/mysql_mutex_bits.h
include/mysql/components/services/bits/mysql_rwlock_bits.h
include/mysql/components/services/bits/mysql_socket_bits.h
include/mysql/components/services/bits/psi_bits.h
include/mysql/components/services/bits/psi_cond_bits.h
include/mysql/components/services/bits/psi_file_bits.h
include/mysql/components/services/bits/psi_mdl_bits.h
include/mysql/components/services/bits/psi_memory_bits.h
include/mysql/components/services/bits/psi_metric_bits.h
include/mysql/components/services/bits/psi_mutex_bits.h
include/mysql/components/services/bits/psi_rwlock_bits.h
include/mysql/components/services/bits/psi_socket_bits.h
include/mysql/components/services/bits/psi_stage_bits.h
include/mysql/components/services/bits/psi_table_bits.h
include/mysql/components/services/bits/server_telemetry_attribute_bits.h
include/mysql/components/services/bits/server_telemetry_logs_client_bits.h
include/mysql/components/services/bits/system_variables_bits.h
include/mysql/components/services/bits/thr_cond_bits.h
include/mysql/components/services/bits/thr_mutex_bits.h
include/mysql/components/services/bits/thr_rwlock_bits.h
include/mysql/components/services/bulk_data_service.h
include/mysql/components/services/page_track_service.h
include/mysql/components/services/registry.h
include/mysql/my_loglevel.h
include/mysql/mysql_lex_string.h
include/mysql/plugin.h
include/mysql/plugin_auth_common.h
include/mysql/psi/mysql_cond.h
include/mysql/psi/mysql_mutex.h
include/mysql/psi/mysql_rwlock.h
include/mysql/psi/mysql_socket.h
include/mysql/psi/psi_cond.h
include/mysql/psi/psi_memory.h
include/mysql/psi/psi_mutex.h
include/mysql/psi/psi_rwlock.h
include/mysql/psi/psi_socket.h
include/mysql/service_command.h
include/mysql/service_locking.h
include/mysql/service_my_plugin_log.h
include/mysql/service_mysql_alloc.h
include/mysql/service_mysql_keyring.h
include/mysql/service_mysql_password_policy.h
include/mysql/service_mysql_string.h
include/mysql/service_parser.h
include/mysql/service_plugin_registry.h
include/mysql/service_rpl_transaction_ctx.h
include/mysql/service_rpl_transaction_write_set.h
include/mysql/service_rules_table.h
include/mysql/service_security_context.h
include/mysql/service_srv_session.h
include/mysql/service_srv_session_bits.h
include/mysql/service_srv_session_info.h
include/mysql/service_thd_alloc.h
include/mysql/service_thd_wait.h
include/mysql/service_thread_scheduler.h
include/mysql/services.h
include/mysql/status_var.h
include/mysql/strings/api.h
include/mysql/strings/dtoa.h
include/mysql/strings/int2str.h
include/mysql/strings/m_ctype.h
include/mysql/udf_registration_types.h
include/mysql_com.h
include/mysql_time.h
include/nulls.h
include/pfs_socket_provider.h
include/prealloced_array.h
include/sql_string.h
include/string_with_len.h
include/strmake.h
include/template_utils.h
include/thr_cond.h
include/thr_lock.h
include/thr_mutex.h
include/thr_rwlock.h
include/typelib.h
include/violite.h
libbinlogevents/export/binary_log_funcs.h
libbinlogevents/include/table_id.h
libbinlogevents/include/wrapper_functions.h
libs/mysql/binlog/event/export/binary_log_funcs.h
libs/mysql/binlog/event/table_id.h
libs/mysql/binlog/event/wrapper_functions.h
sql/auth/auth_acls.h
sql/dd/collection.h
sql/dd/object_id.h
sql/dd/sdi_fwd.h
sql/dd/string_type.h
sql/dd/types/column.h
sql/dd/types/entity_object.h
sql/dd/types/foreign_key.h
sql/dd/types/object_table.h
sql/dd/types/weak_object.h
sql/discrete_interval.h
sql/enum_query_type.h
sql/field.h
sql/field_common_properties.h
sql/filesort_utils.h
sql/gis/srid.h
sql/handler.h
sql/key.h
sql/key_spec.h
sql/lock.h
sql/malloc_allocator.h
sql/mdl.h
sql/mem_root_allocator.h
sql/mem_root_array.h
sql/mysqld_cs.h
sql/opt_costconstants.h
sql/opt_costmodel.h
sql/partition_element.h
sql/partition_info.h
sql/psi_memory_key.h
sql/record_buffer.h
sql/select_lex_visitor.h
sql/sql_array.h
sql/sql_bitmap.h
sql/sql_cmd.h
sql/sql_const.h
sql/sql_data_change.h
sql/sql_error.h
sql/sql_list.h
sql/sql_plist.h
sql/sql_plugin.h
sql/sql_plugin_ref.h
sql/sql_sort.h
sql/stateless_allocator.h
sql/stream_cipher.h
sql/table.h
sql/tablesample.h
sql/thr_malloc.h
"

echo "==> Building extract list..."
PREFIX="mysql-${VERSION}"

# List tarball contents once (decompresses the archive once, reads only headers).
echo "    listing archive contents..."
tar -tf "$TMPDIR/mysql.tar.gz" > "$TMPDIR/archive-index.txt"

# Match needed files against what is actually in this tarball.
> "$TMPDIR/to-extract.txt"
FOUND=0
MISSING=0
for rel in $NEEDED_FILES; do
    tarpath="${PREFIX}/${rel}"
    if grep -qF "${tarpath}" "$TMPDIR/archive-index.txt"; then
        echo "$tarpath" >> "$TMPDIR/to-extract.txt"
        FOUND=$((FOUND + 1))
    else
        MISSING=$((MISSING + 1))
    fi
done
echo "    found: ${FOUND}  not in this tarball: ${MISSING}"

echo "==> Extracting to $DEST..."
rm -rf "$DEST"
mkdir -p "$DEST"

tar -xf "$TMPDIR/mysql.tar.gz" \
    --strip-components=1 \
    -C "$DEST" \
    -T "$TMPDIR/to-extract.txt"

echo "$VERSION" > "$DEST/.mysql-version"

echo "==> Done."
echo ""
du -sh "$DEST"
echo "Committed file count: $(find "$DEST" -type f | wc -l)"
echo ""
echo "Next steps:"
echo "  git add vendor/mysql-headers-${SERIES}"
echo "  git commit -m \"chore: update MySQL ${SERIES} headers to ${VERSION}\""
