# RPM spec for the Pinba storage engine (MySQL/MariaDB plugin).
#
# Unlike the PHP extension, the engine is a storage-engine plugin compiled against
# the database server's *source* headers (sql/handler.h ...), which no -devel
# package ships. Those headers are vendored in-tree (vendor/mysql-headers-8.x for
# MySQL, vendor/mariadb-headers-* for MariaDB) and travel in the source tarball,
# so the build is fully offline in mock/Copr and needs no server package or network
# at build time. The plugin is ABI-bound to the server's minor version, so the
# vendored-header series must match the runtime server in the target chroot.
#
# One flavor is built per invocation, selected with --define "flavor mariadb"
# (default: mysql). MySQL and MariaDB plugins install the same ha_pinba.so path and
# a host runs one server, so the two flavor packages Conflict. The Copr publish step
# submits one SRPM per flavor.
#
# Plugin layout on Fedora/EL:
#   plugin dir : %%{_libdir}/mysql/plugin   (/usr/lib64/mysql/plugin)
#   schema     : %%{_datadir}/pinba_engine/default_tables.sql

# Flavor: mysql (default) or mariadb.
%{!?flavor: %global flavor mysql}

%if "%{flavor}" == "mariadb"
# MariaDB flavor. Series pinned to each chroot's native MariaDB (ABI must match):
# Fedora ships 11.8; EL9/EL10 ship 10.11 (module / AppStream).
%if 0%{?rhel}
%global db_series 10.11
%global db_version 10.11.14
%else
%global db_series 11.8
%global db_version 11.8.6
%endif
%global db_source_dir vendor/mariadb-headers-%{db_series}
%global cmake_flavor mariadb
%global plugin_symbol _maria_plugin_declarations_
%global server_req mariadb-server
%global other_flavor mysql
%else
# MySQL flavor. EL9's default MySQL module is 8.0; Fedora and EL10 ship 8.4.
%if 0%{?rhel} == 9
%global db_series 8.0
%global db_version 8.0.46
%else
%global db_series 8.4
%global db_version 8.4.9
%endif
%global db_source_dir vendor/mysql-headers-%{db_series}
%global cmake_flavor mysql
%global plugin_symbol _mysql_plugin_declarations_
%global server_req mysql-server
%global other_flavor mariadb
%endif

Name:           pinba-engine
Version:        2.7.0
Release:        1%{?dist}
Summary:        Pinba storage engine plugin for %{cmake_flavor}

License:        GPL-2.0-only
URL:            https://github.com/XOlegator/pinba_engine
Source0:        %{url}/releases/download/v%{version}/pinba-engine-%{version}.tar.gz

BuildRequires:  gcc-c++
BuildRequires:  cmake
BuildRequires:  make
BuildRequires:  protobuf-devel
BuildRequires:  protobuf-compiler
BuildRequires:  rapidjson-devel
BuildRequires:  openssl-devel
BuildRequires:  pkgconfig
%if "%{flavor}" == "mysql"
# Client headers (mysql.h) for the plugin's own <mysql.h> include. The server
# *source* headers (sql/handler.h) come from the vendored tree, not this package.
# The MariaDB flavor takes mysql.h from its vendored source include/, so it needs
# no DB dev package at all.
BuildRequires:  mysql-devel
%endif

%description
Pinba Engine is a MySQL/MariaDB storage engine that collects and analyzes PHP
runtime statistics sent over UDP (protobuf) by the Pinba PHP extension, exposing
live aggregated reports as SQL tables. This source package builds the server
plugin and its shared data.

%package common
Summary:        Shared data for the Pinba storage engine (schema, metadata)
BuildArch:      noarch

%description common
Architecture-independent files shared by the Pinba engine plugin packages: the
report-table schema (default_tables.sql) and documentation.

%package %{flavor}
Summary:        Pinba storage engine plugin for %{cmake_flavor} %{db_series}
Requires:       %{name}-common = %{version}-%{release}
Requires:       %{server_req}
# The MySQL and MariaDB plugin builds install the same ha_pinba.so path and are
# mutually exclusive (a host runs one server), so the flavors conflict.
Conflicts:      %{name}-%{other_flavor}

%description %{flavor}
The Pinba storage engine compiled as a %{cmake_flavor} %{db_series} plugin
(ha_pinba.so). Load it with INSTALL PLUGIN pinba SONAME 'ha_pinba.so' and
initialize the schema from %{_datadir}/pinba_engine/default_tables.sql.

%prep
%autosetup -n pinba-engine-%{version}

%build
%cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DPINBA_DB_FLAVOR=%{cmake_flavor} \
    -DPINBA_MYSQL_SERIES=%{db_series} \
    -DPINBA_MYSQL_SOURCE_DIR=%{_builddir}/pinba-engine-%{version}/%{db_source_dir} \
    -DPINBA_MYSQL_SOURCE_VERSION=%{db_version} \
%if "%{flavor}" == "mariadb"
    -DMYSQL_INCLUDE_DIR=%{_builddir}/pinba-engine-%{version}/%{db_source_dir}/include \
%endif
    -DPINBA_FETCH_DEPENDENCIES=OFF \
    -DPINBA_WITH_TESTS=OFF
%cmake_build

%install
%cmake_install
# Drop developer/consumer artifacts the runtime plugin packages do not need: the
# CMake package-config files and the duplicate README under the data dir (README
# is shipped as %doc in -common instead).
rm -rf %{buildroot}%{_libdir}/cmake/PinbaEngine
rm -f %{buildroot}%{_datadir}/pinba_engine/README.md

%check
# The server isn't running at build time, so verify the plugin object exports the
# expected plugin declaration symbol instead of doing a live INSTALL PLUGIN (that
# is exercised by the CI gate against a real server).
nm -D "%{buildroot}%{_libdir}/mysql/plugin/ha_pinba.so" | grep -q %{plugin_symbol}

%files common
%license COPYING
%doc README.md
%dir %{_datadir}/pinba_engine
%{_datadir}/pinba_engine/default_tables.sql
%{_datadir}/pinba_engine/pinba_tables.sql

%files %{flavor}
%{_libdir}/mysql/plugin/ha_pinba.so

%changelog
* Thu Jul 02 2026 Oleg Ekhlakov <o.ekhlakov@protonmail.com> - 2.7.0-1
- Initial RPM packaging (MySQL and MariaDB flavors) for Fedora/EL via Copr.
