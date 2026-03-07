# Build Pinba Engine in a stable and ABI-compatible environment

FROM ubuntu:20.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
ENV CMAKE_BUILD_TYPE=Release

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential=* \
    cmake=* \
    git=* \
    wget=* \
    ca-certificates=* \
    libprotobuf-dev=* \
    protobuf-compiler=* \
    libmysqlclient-dev=* \
    mysql-client=* \
    mysql-source-8.0=* \
    rapidjson-dev=* \
    pkg-config=* \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY . /build/pinba_engine

WORKDIR /build/pinba_engine/build
RUN cmake \
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        .. && \
    make -j"$(nproc)" && \
    make install

FROM mysql:8.0-bookworm

RUN mkdir -p /usr/lib/mysql/plugin /usr/lib/x86_64-linux-gnu

COPY --from=builder /usr/local/lib/mysql/plugin/ha_pinba.so \
    /usr/lib/mysql/plugin/libpinba_engine.so

COPY --from=builder /usr/lib/x86_64-linux-gnu/libprotobuf.so.* \
    /usr/lib/x86_64-linux-gnu/

COPY --from=builder /build/pinba_engine/default_tables.sql \
    /usr/share/pinba_engine/default_tables.sql

COPY docker-entrypoint-initdb.d/ /docker-entrypoint-initdb.d/

RUN chmod 755 /usr/lib/mysql/plugin/libpinba_engine.so && \
    chmod 644 /usr/share/pinba_engine/default_tables.sql

EXPOSE 3306 30002/udp
