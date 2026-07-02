# RPM spec for the Pinba storage engine (MySQL/MariaDB plugin).
#
# Unlike the PHP extension, the engine is a storage-engine plugin compiled against
# the database server's *source* headers (sql/handler.h ...), which no -devel
# package ships. Those headers are vendored in-tree (vendor/mysql-headers-8.x) and
# travel in the source tarball, so the build is fully offline in mock/Copr and needs
# no server package or network at build time. The plugin is ABI-bound to the
# server's minor version, so the vendored-header version must match the runtime
# server in the target chroot.
#
# This first spec covers the MySQL flavor. The MariaDB flavor (pinba-engine-mariadb)
# is added once its server headers are vendored the same way.
#
# Plugin layout on Fedora/EL:
#   plugin dir : %%{_libdir}/mysql/plugin   (/usr/lib64/mysql/plugin)
#   schema     : %%{_datadir}/pinba_engine/default_tables.sql

# Which MySQL series to build the plugin against. EL9's default MySQL module is
# 8.0; Fedora and EL10 ship 8.4. Override with --define "mysql_series 8.0".
%if 0%{?rhel} == 9
%global mysql_series 8.0
%global mysql_version 8.0.46
%else
%global mysql_series 8.4
%global mysql_version 8.4.9
%endif
%global vendored_headers vendor/mysql-headers-%{mysql_series}

Name:           pinba-engine
Version:        2.7.0
Release:        1%{?dist}
Summary:        Pinba storage engine plugin for MySQL

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
# Client headers (mysql.h) for the plugin's own <mysql.h> include. The server
# *source* headers (sql/handler.h) come from the vendored tree, not this package.
BuildRequires:  mysql-devel

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

%package mysql
Summary:        Pinba storage engine plugin for MySQL %{mysql_series}
Requires:       %{name}-common = %{version}-%{release}
Requires:       mysql-server
# The MySQL and MariaDB plugin builds install the same ha_pinba.so path and are
# mutually exclusive (a host runs one server), so the flavors conflict.
Conflicts:      %{name}-mariadb

%description mysql
The Pinba storage engine compiled as a MySQL %{mysql_series} plugin
(ha_pinba.so). Load it with INSTALL PLUGIN pinba SONAME 'ha_pinba.so' and
initialize the schema from %{_datadir}/pinba_engine/default_tables.sql.

%prep
%autosetup -n pinba-engine-%{version}

%build
%cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DPINBA_MYSQL_SERIES=%{mysql_series} \
    -DPINBA_MYSQL_SOURCE_DIR=%{_builddir}/pinba-engine-%{version}/%{vendored_headers} \
    -DPINBA_MYSQL_SOURCE_VERSION=%{mysql_version} \
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
# MySQL plugin declaration symbol instead of doing a live INSTALL PLUGIN (that is
# exercised by the CI gate against a real mysqld).
nm -D "%{buildroot}%{_libdir}/mysql/plugin/ha_pinba.so" | grep -q _mysql_plugin_declarations_

%files common
%license COPYING
%doc README.md
%dir %{_datadir}/pinba_engine
%{_datadir}/pinba_engine/default_tables.sql
%{_datadir}/pinba_engine/pinba_tables.sql

%files mysql
%{_libdir}/mysql/plugin/ha_pinba.so

%changelog
* Thu Jul 02 2026 Oleg Ekhlakov <o.ekhlakov@protonmail.com> - 2.7.0-1
- Initial RPM packaging (MySQL flavor) for Fedora/EL via Copr.
