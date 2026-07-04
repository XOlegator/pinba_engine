cmake_minimum_required(VERSION 3.24)

project(
  pinba_engine
  VERSION 2.11.3 # x-release-please-version
  DESCRIPTION "Pinba Engine - MySQL Storage Engine for PHP Performance Statistics"
  LANGUAGES C CXX)

include(FetchContent)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(PINBA_DEBUG "Enable debug output" OFF)
option(PINBA_WITH_TESTS "Build unit tests" ON)
option(PINBA_WITH_BENCHMARKS "Build benchmarks" OFF)
option(PINBA_FETCH_DEPENDENCIES "Download test/benchmark dependencies when not installed" ON)
option(PINBA_DOWNLOAD_MYSQL_SOURCE "Download MySQL source headers when no local source tree is found" OFF)
option(PINBA_ENABLE_SANITIZERS "Enable ASan/UBSan for debug hardening" OFF)

set(PINBA_DB_FLAVOR "mysql" CACHE STRING "Target server flavor for source headers (mysql or mariadb)")
set_property(CACHE PINBA_DB_FLAVOR PROPERTY STRINGS mysql mariadb)
if(NOT PINBA_DB_FLAVOR STREQUAL "mysql" AND NOT PINBA_DB_FLAVOR STREQUAL "mariadb")
  message(FATAL_ERROR "PINBA_DB_FLAVOR must be mysql or mariadb. Got: ${PINBA_DB_FLAVOR}")
endif()

if(PINBA_DB_FLAVOR STREQUAL "mariadb")
  set(PINBA_MYSQL_SERIES "11.8" CACHE STRING "Target MariaDB server series for source headers (10.5, 10.11 or 11.8)")
  set_property(CACHE PINBA_MYSQL_SERIES PROPERTY STRINGS 10.5 10.11 11.8)
  if(NOT PINBA_MYSQL_SERIES MATCHES "^(10\\.5|10\\.11|11\\.8)$")
    message(FATAL_ERROR "For MariaDB, PINBA_MYSQL_SERIES must be 10.5, 10.11 or 11.8. Got: ${PINBA_MYSQL_SERIES}")
  endif()
  # MariaDB source headers are downloaded from https://archive.mariadb.org; the
  # exact patch version is pinned per target by the packaging matrix (ABI must
  # match the runtime server). Hashes are left empty here — packaging vendors a
  # verified tree rather than downloading at configure time.
  if(PINBA_MYSQL_SERIES STREQUAL "11.8")
    set(PINBA_DEFAULT_MYSQL_SOURCE_VERSION "11.8.6")
  elseif(PINBA_MYSQL_SERIES STREQUAL "10.11")
    set(PINBA_DEFAULT_MYSQL_SOURCE_VERSION "10.11.14")
  else()
    set(PINBA_DEFAULT_MYSQL_SOURCE_VERSION "10.5.30")
  endif()
  set(PINBA_DEFAULT_MYSQL_SOURCE_HASH "")
else()
  set(PINBA_MYSQL_SERIES "8.0" CACHE STRING "Target MySQL server series for source headers (8.0 or 8.4)")
  set_property(CACHE PINBA_MYSQL_SERIES PROPERTY STRINGS 8.0 8.4)
  if(NOT PINBA_MYSQL_SERIES STREQUAL "8.0" AND NOT PINBA_MYSQL_SERIES STREQUAL "8.4")
    message(FATAL_ERROR "PINBA_MYSQL_SERIES must be 8.0 or 8.4. Got: ${PINBA_MYSQL_SERIES}")
  endif()

  if(PINBA_MYSQL_SERIES STREQUAL "8.4")
    set(PINBA_DEFAULT_MYSQL_SOURCE_VERSION "8.4.9")
    set(PINBA_DEFAULT_MYSQL_SOURCE_HASH "MD5=7b2591fa0b5388f604cc9b8273eab331")
  else()
    set(PINBA_DEFAULT_MYSQL_SOURCE_VERSION "8.0.46")
    set(PINBA_DEFAULT_MYSQL_SOURCE_HASH "MD5=6707beb0d46a9e08a19aa596329ca79d")
  endif()
endif()

set(PINBA_MYSQL_SOURCE_DIR "" CACHE PATH "Path to an extracted MySQL source tree containing sql/handler.h")
set(
  PINBA_MYSQL_SOURCE_ARCHIVE
  "/usr/src/mysql/mysql-source-${PINBA_MYSQL_SERIES}.tar.gz"
  CACHE FILEPATH "Optional local MySQL source archive")
set(
  PINBA_MYSQL_SOURCE_VERSION
  "${PINBA_DEFAULT_MYSQL_SOURCE_VERSION}"
  CACHE STRING "MySQL source version")
set(
  PINBA_MYSQL_SOURCE_URL
  "https://dev.mysql.com/get/Downloads/MySQL-${PINBA_MYSQL_SERIES}/mysql-${PINBA_MYSQL_SOURCE_VERSION}.tar.gz"
  CACHE STRING "MySQL source archive URL")
set(
  PINBA_MYSQL_SOURCE_HASH
  "${PINBA_DEFAULT_MYSQL_SOURCE_HASH}"
  CACHE STRING "Expected hash for PINBA_MYSQL_SOURCE_URL")

find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)
if(Protobuf_VERSION VERSION_LESS "3.0.0")
  message(FATAL_ERROR "Protocol Buffers 3.0.0 or higher is required")
endif()

find_program(MYSQL_CONFIG_EXECUTABLE mysql_config)
if(MYSQL_CONFIG_EXECUTABLE)
  execute_process(
    COMMAND ${MYSQL_CONFIG_EXECUTABLE} --variable=pkgincludedir
    OUTPUT_VARIABLE PINBA_MYSQL_CONFIG_INCLUDE
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  execute_process(
    COMMAND ${MYSQL_CONFIG_EXECUTABLE} --variable=pkglibdir
    OUTPUT_VARIABLE PINBA_MYSQL_CONFIG_LIBDIR
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()

find_path(
  MYSQL_INCLUDE_DIR
  NAMES mysql.h
  HINTS ${PINBA_MYSQL_CONFIG_INCLUDE}
  PATHS /usr/include/mysql /usr/local/include/mysql /usr/local/mysql/include
        /opt/mysql/include
  PATH_SUFFIXES mysql)

if(NOT MYSQL_INCLUDE_DIR)
  message(
    FATAL_ERROR
      "MySQL client headers not found. Install libmysqlclient development "
      "files or set MYSQL_INCLUDE_DIR.")
endif()

set(PINBA_MYSQL_SOURCE_EXTRACT_BASE "${CMAKE_BINARY_DIR}/_deps/mysql-source")
set(PINBA_MYSQL_BUILDDIR "")
set(PINBA_MYSQL_BUILDDIR_INCLUDE "")

if(NOT PINBA_MYSQL_SOURCE_DIR AND EXISTS "${PINBA_MYSQL_SOURCE_ARCHIVE}")
  file(MAKE_DIRECTORY "${PINBA_MYSQL_SOURCE_EXTRACT_BASE}")
  if(NOT EXISTS "${PINBA_MYSQL_SOURCE_EXTRACT_BASE}/mysql-${PINBA_MYSQL_SERIES}/sql/handler.h")
    execute_process(
      COMMAND ${CMAKE_COMMAND} -E tar xf "${PINBA_MYSQL_SOURCE_ARCHIVE}"
      WORKING_DIRECTORY "${PINBA_MYSQL_SOURCE_EXTRACT_BASE}"
      RESULT_VARIABLE PINBA_MYSQL_EXTRACT_RESULT)
    if(NOT PINBA_MYSQL_EXTRACT_RESULT EQUAL 0)
      message(FATAL_ERROR "Failed to extract ${PINBA_MYSQL_SOURCE_ARCHIVE}")
    endif()
  endif()
  set(PINBA_MYSQL_SOURCE_DIR "${PINBA_MYSQL_SOURCE_EXTRACT_BASE}/mysql-${PINBA_MYSQL_SERIES}")
endif()

if(NOT PINBA_MYSQL_SOURCE_DIR AND PINBA_DOWNLOAD_MYSQL_SOURCE)
  set(PINBA_MYSQL_DOWNLOAD_ARCHIVE "${CMAKE_BINARY_DIR}/_deps/mysql-${PINBA_MYSQL_SOURCE_VERSION}.tar.gz")
  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/_deps" "${PINBA_MYSQL_SOURCE_EXTRACT_BASE}")
  if(NOT EXISTS "${PINBA_MYSQL_DOWNLOAD_ARCHIVE}")
    message(STATUS "Downloading MySQL source headers from ${PINBA_MYSQL_SOURCE_URL}")
    if(PINBA_MYSQL_SOURCE_HASH)
      file(
        DOWNLOAD "${PINBA_MYSQL_SOURCE_URL}" "${PINBA_MYSQL_DOWNLOAD_ARCHIVE}"
        EXPECTED_HASH "${PINBA_MYSQL_SOURCE_HASH}"
        SHOW_PROGRESS
        TLS_VERIFY ON)
    else()
      message(WARNING "PINBA_MYSQL_SOURCE_HASH is empty; downloading MySQL source without hash verification.")
      file(
        DOWNLOAD "${PINBA_MYSQL_SOURCE_URL}" "${PINBA_MYSQL_DOWNLOAD_ARCHIVE}"
        SHOW_PROGRESS
        TLS_VERIFY ON)
    endif()
  endif()
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar xf "${PINBA_MYSQL_DOWNLOAD_ARCHIVE}"
    WORKING_DIRECTORY "${PINBA_MYSQL_SOURCE_EXTRACT_BASE}"
    RESULT_VARIABLE PINBA_MYSQL_DOWNLOAD_EXTRACT_RESULT)
  if(NOT PINBA_MYSQL_DOWNLOAD_EXTRACT_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to extract ${PINBA_MYSQL_DOWNLOAD_ARCHIVE}")
  endif()
  file(GLOB PINBA_MYSQL_DOWNLOADED_SOURCES "${PINBA_MYSQL_SOURCE_EXTRACT_BASE}/mysql-*/sql/handler.h")
  list(LENGTH PINBA_MYSQL_DOWNLOADED_SOURCES PINBA_MYSQL_DOWNLOADED_SOURCE_COUNT)
  if(PINBA_MYSQL_DOWNLOADED_SOURCE_COUNT GREATER 0)
    list(GET PINBA_MYSQL_DOWNLOADED_SOURCES 0 PINBA_MYSQL_DOWNLOADED_HANDLER)
    get_filename_component(PINBA_MYSQL_DOWNLOADED_SQL_DIR "${PINBA_MYSQL_DOWNLOADED_HANDLER}" DIRECTORY)
    get_filename_component(PINBA_MYSQL_SOURCE_DIR "${PINBA_MYSQL_DOWNLOADED_SQL_DIR}" DIRECTORY)
  endif()
endif()

if(PINBA_MYSQL_SOURCE_DIR)
  set(PINBA_MYSQL_BUILDDIR "${PINBA_MYSQL_SOURCE_DIR}/builddir")
  set(PINBA_MYSQL_BUILDDIR_INCLUDE "${PINBA_MYSQL_SOURCE_DIR}/builddir/include")
endif()

find_path(
  MYSQL_SQL_INCLUDE_DIR
  NAMES sql/handler.h
  HINTS ${PINBA_MYSQL_SOURCE_DIR}
  PATHS /usr/src/mysql/mysql-${PINBA_MYSQL_SERIES}
        /usr/src/mysql/mysql-source-${PINBA_MYSQL_SERIES}
        /usr/include/mysql /usr/local/mysql/include /opt/mysql/include
  NO_DEFAULT_PATH)

if(NOT MYSQL_SQL_INCLUDE_DIR)
  message(
    FATAL_ERROR
      "MySQL server headers not found for series ${PINBA_MYSQL_SERIES}. "
      "Set PINBA_MYSQL_SOURCE_DIR to an extracted MySQL source tree, "
      "provide PINBA_MYSQL_SOURCE_ARCHIVE, or configure with -DPINBA_DOWNLOAD_MYSQL_SOURCE=ON.")
endif()
message(STATUS "Found MySQL SQL headers: ${MYSQL_SQL_INCLUDE_DIR}")

if(PINBA_DB_FLAVOR STREQUAL "mariadb")
  # Deliberately excludes mysqlclient: linking MySQL's client into a MariaDB plugin
  # would add a wrong libmysqlclient NEEDED. If no MariaDB client is present the
  # MODULE links without one (the server resolves the symbols at load time).
  set(PINBA_CLIENT_LIB_NAMES mariadbclient mariadb)
else()
  set(PINBA_CLIENT_LIB_NAMES mysqlclient)
endif()
find_library(
  MYSQL_LIBRARY
  NAMES ${PINBA_CLIENT_LIB_NAMES}
  HINTS ${PINBA_MYSQL_CONFIG_LIBDIR}
  PATHS /usr/lib/x86_64-linux-gnu /usr/lib /usr/local/lib /usr/local/mysql/lib
        /usr/lib/x86_64-linux-gnu/mariadb19 /opt/mysql/lib
  PATH_SUFFIXES mysql mariadb)

# The plugin is a MODULE loaded by the server, which supplies these symbols at
# load time, so the client library is only needed for standalone linking. Require
# it for MySQL (matches the historical behaviour); make it optional for MariaDB,
# where the -devel client package is often absent on build hosts.
if(NOT MYSQL_LIBRARY AND NOT PINBA_DB_FLAVOR STREQUAL "mariadb")
  message(FATAL_ERROR "MySQL client library not found. Install libmysqlclient development files or set MYSQL_LIBRARY.")
endif()
if(NOT MYSQL_LIBRARY)
  set(MYSQL_LIBRARY "")
  message(STATUS "MariaDB client library not found; linking the plugin without it "
                 "(the server resolves the symbols at load time).")
endif()

# Derive MYSQL_VERSION_ID from the pinned source version when available — this is the
# canonical method. The MySQL source tarball only ships mysql_version.h.in (a CMake
# template), so MYSQL_VERSION_ID is never defined unless we generate the .h ourselves.
# Without it, MYSQL_HANDLERTON_INTERFACE_VERSION = (undefined << 8) = 0, which causes
# "API version for STORAGE ENGINE plugin is too different" at plugin load time even when
# the source and runtime MySQL patch versions appear to match.
if(PINBA_MYSQL_SOURCE_VERSION)
  string(REPLACE "." ";" _pinba_ver_parts "${PINBA_MYSQL_SOURCE_VERSION}")
  list(GET _pinba_ver_parts 0 _pinba_ver_major)
  list(GET _pinba_ver_parts 1 _pinba_ver_minor)
  list(GET _pinba_ver_parts 2 _pinba_ver_patch)
  math(EXPR MYSQL_VERSION_ID
       "${_pinba_ver_major} * 10000 + ${_pinba_ver_minor} * 100 + ${_pinba_ver_patch}")
else()
  # User-provided source tree (PINBA_MYSQL_SOURCE_DIR set manually): try to read the
  # version from pre-generated headers if they exist.
  set(PINBA_VERSION_HEADER_CANDIDATES
      "${PINBA_MYSQL_BUILDDIR_INCLUDE}/mysql_version.h"
      "${MYSQL_SQL_INCLUDE_DIR}/include/mysql_version.h"
      "${MYSQL_INCLUDE_DIR}/mysql_version.h")
  set(MYSQL_VERSION_ID "")
  foreach(PINBA_VERSION_HEADER IN LISTS PINBA_VERSION_HEADER_CANDIDATES)
    if(EXISTS "${PINBA_VERSION_HEADER}")
      file(READ "${PINBA_VERSION_HEADER}" MYSQL_VERSION_H)
      string(REGEX MATCH "#define MYSQL_VERSION_ID[ \t]+([0-9]+)" MYSQL_VERSION_MATCH "${MYSQL_VERSION_H}")
      if(MYSQL_VERSION_MATCH AND CMAKE_MATCH_1 GREATER_EQUAL 80000)
        set(MYSQL_VERSION_ID ${CMAKE_MATCH_1})
        message(STATUS "MySQL version from ${PINBA_VERSION_HEADER}: ${MYSQL_VERSION_ID}")
        break()
      endif()
    endif()
  endforeach()
  if(NOT MYSQL_VERSION_ID)
    if(PINBA_MYSQL_SERIES STREQUAL "8.4")
      set(MYSQL_VERSION_ID 80400)
    else()
      set(MYSQL_VERSION_ID 80000)
    endif()
    message(
      WARNING
        "Could not determine exact MySQL version. "
        "Falling back to MYSQL_VERSION_ID=${MYSQL_VERSION_ID}. "
        "Set PINBA_MYSQL_SOURCE_VERSION to the exact patch version (e.g. 8.0.46) to fix this.")
  endif()
endif()

if(MYSQL_VERSION_ID LESS 80000)
  message(FATAL_ERROR "MySQL 8.0 or higher is required. Found version: ${MYSQL_VERSION_ID}")
endif()
message(STATUS "MySQL version ID: ${MYSQL_VERSION_ID}")

# Generate mysql_version.h from the computed MYSQL_VERSION_ID.
#
# Two paths are generated because ha_pinba.cc uses two different include forms:
#   #include <mysql_version.h>       → found via ${PINBA_MYSQL_BUILDDIR_INCLUDE}
#   #include <mysql/mysql_version.h> → found via ${PINBA_MYSQL_BUILDDIR_INCLUDE}/mysql/
#
# Both directories appear before ${MYSQL_INCLUDE_DIR} (the MariaDB-compat system path)
# in the SYSTEM INTERFACE include list, so our generated file takes precedence.
# Without this, ha_pinba.cc's explicit #include <mysql/mysql_version.h> picks up the
# MariaDB MYSQL_VERSION_ID, making MYSQL_HANDLERTON_INTERFACE_VERSION wrong and causing
# "API version for STORAGE ENGINE plugin is too different" at plugin load time.
if(PINBA_MYSQL_SOURCE_DIR AND PINBA_DB_FLAVOR STREQUAL "mysql")
  set(_pinba_version_h_content
    "/* Generated by pinba_engine CMakeLists.txt — DO NOT EDIT */\n"
    "#ifndef _mysql_version_h\n"
    "#define _mysql_version_h\n"
    "#define PROTOCOL_VERSION            10\n"
    "#define MYSQL_SERVER_VERSION       \"${PINBA_MYSQL_SOURCE_VERSION}\"\n"
    "#define MYSQL_BASE_VERSION         \"mysqld-${PINBA_MYSQL_SERIES}\"\n"
    "#define MYSQL_SERVER_SUFFIX_DEF    \"\"\n"
    "#define MYSQL_VERSION_ID            ${MYSQL_VERSION_ID}\n"
    "#define MYSQL_VERSION_STABILITY     \"\"\n"
    "#define MYSQL_PORT                  3306\n"
    "#define MYSQL_ADMIN_PORT            33062\n"
    "#define MYSQL_PORT_DEFAULT          0\n"
    "#define MYSQL_UNIX_ADDR            \"/tmp/mysql.sock\"\n"
    "#define MYSQL_CONFIG_NAME          \"my\"\n"
    "#define MYSQL_PERSIST_CONFIG_NAME  \"mysqld-auto\"\n"
    "#define MYSQL_COMPILATION_COMMENT  \"MySQL Community Server\"\n"
    "#define MYSQL_COMPILATION_COMMENT_SERVER  \"MySQL Community Server - GPL\"\n"
    "#define LIBMYSQL_VERSION           \"${PINBA_MYSQL_SOURCE_VERSION}\"\n"
    "#define LIBMYSQL_VERSION_ID         ${MYSQL_VERSION_ID}\n"
    "#ifndef LICENSE\n"
    "#define LICENSE                     GPL\n"
    "#endif\n"
    "#endif /* _mysql_version_h */\n")
  file(MAKE_DIRECTORY "${PINBA_MYSQL_BUILDDIR_INCLUDE}")
  file(WRITE "${PINBA_MYSQL_BUILDDIR_INCLUDE}/mysql_version.h" ${_pinba_version_h_content})
  file(MAKE_DIRECTORY "${PINBA_MYSQL_BUILDDIR_INCLUDE}/mysql")
  file(WRITE "${PINBA_MYSQL_BUILDDIR_INCLUDE}/mysql/mysql_version.h" ${_pinba_version_h_content})
  message(STATUS "Generated mysql_version.h: MYSQL_VERSION_ID=${MYSQL_VERSION_ID}")
elseif(PINBA_MYSQL_SOURCE_DIR AND PINBA_DB_FLAVOR STREQUAL "mariadb")
  # MariaDB ships a real generated mysql_version.h that defines MARIADB_BASE_VERSION
  # (our compile-time switch) and the correct MYSQL_VERSION_ID. Never synthesise one —
  # a MySQL-style header would drop MARIADB_BASE_VERSION and disable every compat guard.
  # ha_pinba.cc includes <mysql/mysql_version.h> (MySQL layout), while MariaDB installs
  # it as <mysql_version.h>, so mirror the real header into a mysql/ subdir on the path.
  set(_pinba_mariadb_version_h "")
  foreach(_pinba_cand
      "${PINBA_MYSQL_BUILDDIR_INCLUDE}/mysql_version.h"
      "${MYSQL_SQL_INCLUDE_DIR}/include/mysql_version.h"
      "${MYSQL_INCLUDE_DIR}/mysql_version.h")
    if(EXISTS "${_pinba_cand}")
      set(_pinba_mariadb_version_h "${_pinba_cand}")
      break()
    endif()
  endforeach()
  if(NOT _pinba_mariadb_version_h)
    message(FATAL_ERROR
      "MariaDB build: could not find a generated mysql_version.h. Provide a MariaDB "
      "source tree whose headers were generated (cmake configure + GenError), exposing "
      "mysql_version.h under <source>/builddir/include/ or <source>/include/.")
  endif()
  # Locate MariaDB's generated my_config.h (defines STACK_DIRECTION, HAVE_*, sizes).
  set(_pinba_mariadb_my_config_h "")
  foreach(_pinba_cand
      "${PINBA_MYSQL_BUILDDIR_INCLUDE}/my_config.h"
      "${MYSQL_SQL_INCLUDE_DIR}/include/my_config.h"
      "${MYSQL_INCLUDE_DIR}/my_config.h")
    if(EXISTS "${_pinba_cand}")
      set(_pinba_mariadb_my_config_h "${_pinba_cand}")
      break()
    endif()
  endforeach()
  if(NOT _pinba_mariadb_my_config_h)
    message(FATAL_ERROR "MariaDB build: could not find a generated my_config.h in the source tree.")
  endif()
  # Stage MariaDB's my_config.h and mysql_version.h in a DEDICATED dir that is added
  # as a plain -I ahead of the project's src/ (which ships MySQL's my_config.h). It
  # must be a distinct path from the SYSTEM include dirs, otherwise CMake de-dups it
  # into the -isystem group, which the compiler searches only after every -I.
  set(PINBA_MARIADB_COMPAT_INC "${CMAKE_BINARY_DIR}/pinba-maria-headers")
  file(MAKE_DIRECTORY "${PINBA_MARIADB_COMPAT_INC}/mysql")
  configure_file("${_pinba_mariadb_my_config_h}" "${PINBA_MARIADB_COMPAT_INC}/my_config.h" COPYONLY)
  configure_file("${_pinba_mariadb_version_h}" "${PINBA_MARIADB_COMPAT_INC}/mysql_version.h" COPYONLY)
  configure_file("${_pinba_mariadb_version_h}" "${PINBA_MARIADB_COMPAT_INC}/mysql/mysql_version.h" COPYONLY)
  message(STATUS "MariaDB headers staged in ${PINBA_MARIADB_COMPAT_INC} (my_config.h, mysql_version.h)")
endif()

find_library(M_LIBRARY m)

set(PINBA_PROTO ${CMAKE_CURRENT_SOURCE_DIR}/pinba.proto)
protobuf_generate_cpp(PINBA_PROTO_SRCS PINBA_PROTO_HDRS ${PINBA_PROTO})
set_source_files_properties(${PINBA_PROTO_SRCS} ${PINBA_PROTO_HDRS} PROPERTIES GENERATED TRUE)

# Order-only target so any consumer (e.g. the unit-test binary, which includes
# pinba.pb.h) can depend on protobuf code generation having completed before it
# starts compiling, without racing the custom command on a clean parallel build.
add_custom_target(pinba_proto_generated DEPENDS ${PINBA_PROTO_SRCS} ${PINBA_PROTO_HDRS})

add_library(pinba_project_options INTERFACE)
target_compile_features(pinba_project_options INTERFACE cxx_std_23)
target_compile_options(
  pinba_project_options
  INTERFACE
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<COMPILE_LANG_AND_ID:C,GNU,Clang>:-Wall -Wextra>)

target_compile_definitions(
  pinba_project_options
  INTERFACE MYSQL_DYNAMIC_PLUGIN HAVE_CONFIG_H
            STANDALONE_BINLOG
            $<$<BOOL:${PINBA_DEBUG}>:PINBA_DEBUG;DEBUG_ON>
            $<$<NOT:$<BOOL:${PINBA_DEBUG}>>:NDEBUG;DEBUG_OFF>)

if(PINBA_ENABLE_SANITIZERS)
  target_compile_options(pinba_project_options INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(pinba_project_options INTERFACE -fsanitize=address,undefined)
endif()

option(PINBA_ENABLE_COVERAGE "Enable gcov coverage instrumentation" OFF)
if(PINBA_ENABLE_COVERAGE)
  target_compile_options(pinba_project_options INTERFACE --coverage -fno-inline -fprofile-update=atomic)
  target_link_options(pinba_project_options INTERFACE --coverage)
endif()

add_library(pinba_includes INTERFACE)
# Project headers: regular includes (warnings from our code are visible)
target_include_directories(
  pinba_includes
  INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src)
# Third-party headers: SYSTEM includes suppress warnings from external code.
# On Debian/Ubuntu the MySQL client package may provide MariaDB-compat headers
# that emit #warning directives — SYSTEM silences them without hiding our own code.
target_include_directories(
  pinba_includes
  SYSTEM INTERFACE
    ${MYSQL_SQL_INCLUDE_DIR}
    ${MYSQL_SQL_INCLUDE_DIR}/include
    ${MYSQL_SQL_INCLUDE_DIR}/libs
    ${PINBA_MYSQL_BUILDDIR}
    ${PINBA_MYSQL_BUILDDIR_INCLUDE}
    ${MYSQL_SQL_INCLUDE_DIR}/sql
    ${MYSQL_INCLUDE_DIR}
    ${Protobuf_INCLUDE_DIRS})

set(PINBA_SOURCES
    src/xxhash.c
    src/pinba.pb-c.cc
    ${PINBA_PROTO_SRCS}
    src/ha_pinba.cc
    src/main.cc
    src/data.cc
    src/pool.cc
    src/tags.cc
    src/threadpool.cc
    src/pinba_map.cc
    src/pinba_lmap.cc)

add_library(pinba_engine MODULE ${PINBA_SOURCES})

# MySQL's plugin macros tag the exported symbols with visibility("default"), so the
# plugin loads even with hidden global visibility. MariaDB's MYSQL_PLUGIN_EXPORT is
# empty on ELF, so hidden visibility would strip _maria_plugin_* and the server would
# not recognise the module as a plugin — build the MariaDB flavor with default
# visibility (how MariaDB's own storage-engine plugins are built).
if(PINBA_DB_FLAVOR STREQUAL "mariadb")
  set(PINBA_VISIBILITY_PRESET default)
  set(PINBA_VISIBILITY_INLINES_HIDDEN OFF)
else()
  set(PINBA_VISIBILITY_PRESET hidden)
  set(PINBA_VISIBILITY_INLINES_HIDDEN ON)
endif()

set_target_properties(
  pinba_engine
  PROPERTIES OUTPUT_NAME "ha_pinba"
             PREFIX ""
             SUFFIX ".so"
             VERSION ${PROJECT_VERSION}
             SOVERSION ${PROJECT_VERSION_MAJOR}
             CXX_VISIBILITY_PRESET ${PINBA_VISIBILITY_PRESET}
             VISIBILITY_INLINES_HIDDEN ${PINBA_VISIBILITY_INLINES_HIDDEN})
target_link_libraries(
  pinba_engine
  PRIVATE pinba_project_options pinba_includes ${MYSQL_LIBRARY} ${M_LIBRARY}
          Threads::Threads ${Protobuf_LIBRARIES})
target_include_directories(pinba_engine PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

if(PINBA_DB_FLAVOR STREQUAL "mariadb")
  # src/ ships MySQL's my_config.h; MariaDB's my_global.h pulls <my_config.h> and
  # <mysql_version.h>, which must resolve to MariaDB's generated copies instead.
  # SYSTEM (-isystem) dirs are searched after every -I, so add the dedicated staging
  # dir as a plain BEFORE include that wins over the project's src/ include.
  target_include_directories(pinba_engine BEFORE PRIVATE ${PINBA_MARIADB_COMPAT_INC})

  # Production MariaDB packages are built with DBUG disabled and do not export the
  # dbug runtime symbols (_db_my_assert, _db_keyword_, ...). Without DBUG_OFF the
  # plugin references them and fails to load ("undefined symbol: _db_my_assert"), so
  # compile DBUG_ASSERT/DBUG_ENTER out to match the server (non-debug builds only).
  if(NOT PINBA_DEBUG)
    target_compile_definitions(pinba_engine PRIVATE DBUG_OFF)
  endif()
endif()

if(PINBA_WITH_TESTS)
  add_executable(pinba_test tests/test_simple.cc)
  set_target_properties(pinba_test PROPERTIES OUTPUT_NAME "pinba_test")
  target_link_libraries(
    pinba_test PRIVATE pinba_project_options pinba_includes ${M_LIBRARY}
                       Threads::Threads ${Protobuf_LIBRARIES})
  target_include_directories(pinba_test PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
endif()

set(PINBA_BUILD_STRING "${PROJECT_VERSION}")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/src/config.h @ONLY)

find_package(Git QUIET)
if(GIT_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_SHA1
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  if(GIT_SHA1)
    string(SUBSTRING ${GIT_SHA1} 0 7 GIT_SHORT_SHA1)
    target_compile_definitions(
      pinba_engine PRIVATE PINBA_GIT_SHA1="${GIT_SHA1}"
                           PINBA_GIT_SHORT_SHA1="${GIT_SHORT_SHA1}")
  endif()
endif()

if(PINBA_WITH_TESTS)
  enable_testing()
  find_package(GTest QUIET)
  if(NOT GTest_FOUND AND PINBA_FETCH_DEPENDENCIES)
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
      URL_HASH SHA256=7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
  elseif(NOT GTest_FOUND)
    find_package(GTest REQUIRED)
  endif()
  add_subdirectory(tests)
endif()

if(PINBA_WITH_BENCHMARKS)
  find_package(benchmark QUIET)
  if(NOT benchmark_FOUND AND PINBA_FETCH_DEPENDENCIES)
    FetchContent_Declare(
      googlebenchmark
      URL https://github.com/google/benchmark/archive/refs/tags/v1.9.1.tar.gz
      URL_HASH SHA256=32131c08ee31eeff2c8968d7e874f3cb648034377dfc32a4c377fa8796d84981
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googlebenchmark)
  elseif(NOT benchmark_FOUND)
    find_package(benchmark REQUIRED)
  endif()
  add_subdirectory(benchmarks)
endif()

install(TARGETS pinba_engine LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/mysql/plugin COMPONENT Runtime)
if(PINBA_WITH_TESTS)
  install(TARGETS pinba_test RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
install(FILES README.md default_tables.sql DESTINATION ${CMAKE_INSTALL_DATADIR}/pinba_engine)
install(FILES default_tables.sql DESTINATION ${CMAKE_INSTALL_DATADIR}/pinba_engine RENAME pinba_tables.sql)

write_basic_package_version_file(
  PinbaEngineConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion)
configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/PinbaEngineConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/PinbaEngineConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/PinbaEngine)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/PinbaEngineConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/PinbaEngineConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/PinbaEngine)

set(CPACK_PACKAGE_NAME "pinba-engine")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
set(CPACK_PACKAGE_VENDOR "Pinba Engine Team")
set(CPACK_PACKAGE_CONTACT "pinba-engine@example.com")
include(CPack)

message(STATUS "")
message(STATUS "Pinba Engine Configuration Summary:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  MySQL include: ${MYSQL_INCLUDE_DIR}")
message(STATUS "  MySQL SQL headers: ${MYSQL_SQL_INCLUDE_DIR}")
message(STATUS "  MySQL library: ${MYSQL_LIBRARY}")
message(STATUS "  MySQL version: ${MYSQL_VERSION_ID}")
message(STATUS "  MySQL target series: ${PINBA_MYSQL_SERIES}")
message(STATUS "  Protocol Buffers: ${Protobuf_VERSION}")
message(STATUS "  Tests: ${PINBA_WITH_TESTS}")
message(STATUS "  Benchmarks: ${PINBA_WITH_BENCHMARKS}")
message(STATUS "  Fetch dependencies: ${PINBA_FETCH_DEPENDENCIES}")
message(STATUS "  Download MySQL source: ${PINBA_DOWNLOAD_MYSQL_SOURCE}")
message(STATUS "")
