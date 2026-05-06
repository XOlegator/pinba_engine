# syntax=docker/dockerfile:1.7

ARG MYSQL_IMAGE=mysql:8.0-bookworm@sha256:9382e4f6f7f013231049ab854de9d0810594d028f4237b67102acc7ade8e3a99

FROM ${MYSQL_IMAGE} AS builder

ENV DEBIAN_FRONTEND=noninteractive
ARG CMAKE_BUILD_TYPE=Release
ARG CMAKE_VERSION=3.31.6

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential=12.9 \
    g++=4:12.2.0-3 \
    wget=1.21.3-1+deb12u1 \
    ca-certificates=20230311+deb12u1 \
    python3-pip=23.0.1+dfsg-1 \
    libprotobuf-dev=3.21.12-3 \
    protobuf-compiler=3.21.12-3 \
    rapidjson-dev=1.1.0+dfsg2-7.1 \
    pkg-config=1.8.1-1 \
    libmysqlclient-dev=8.0.46-1debian12 && \
    rm -rf /var/lib/apt/lists/*

RUN pip3 install --break-system-packages --no-cache-dir "cmake==${CMAKE_VERSION}"

WORKDIR /build/pinba_engine
COPY . .

# Build against MySQL 8.0 source headers downloaded by CMake.
# Builder and runtime use the same MySQL base, which keeps GLIBC/GLIBCXX ABI compatible.
RUN cmake -S . -B build \
      -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} \
      -DCMAKE_INSTALL_PREFIX=/usr/local \
      -DPINBA_WITH_TESTS=OFF \
      -DPINBA_WITH_BENCHMARKS=OFF \
      -DPINBA_DOWNLOAD_MYSQL_SOURCE=ON && \
    cmake --build build -j"$(nproc)" && \
    cmake --install build

FROM ${MYSQL_IMAGE}

RUN mkdir -p /usr/lib/mysql/plugin

COPY --from=builder /usr/local/lib/mysql/plugin/ha_pinba.so /usr/lib/mysql/plugin/libpinba_engine.so
COPY --from=builder /usr/lib/x86_64-linux-gnu/libprotobuf.so.* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /build/pinba_engine/default_tables.sql /usr/share/pinba_engine/default_tables.sql
COPY docker-entrypoint-initdb.d/ /docker-entrypoint-initdb.d/

RUN chmod 755 /usr/lib/mysql/plugin/libpinba_engine.so && \
    chmod 644 /usr/share/pinba_engine/default_tables.sql

EXPOSE 3306 30002/udp
