FROM docker.io/library/debian:13-slim AS build

RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential clang cmake ninja-build git ca-certificates python3 python3-yaml libssl-dev zlib1g-dev liblzo2-dev pkgconf libmariadb3 libmariadb-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

COPY src/ src/
COPY vendor/ vendor/
COPY CMakeLists.txt ./

RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/opt/metin2 -DBUILD_TESTING=OFF \
    && cmake --build build --parallel 4

COPY dist/metin2-runtime dist/

RUN cmake --install build --prefix /opt/metin2

FROM docker.io/library/debian:13-slim

RUN apt-get update && apt-get install -y --no-install-recommends python3 libssl3t64 zlib1g liblzo2-2 libmariadb3 ca-certificates \
    && rm -rf /var/lib/apt/lists/* \
    && groupadd --gid 10001 metin2 \
    && useradd --uid 10001 --gid metin2 --no-create-home --home-dir /var/lib/metin2 metin2 \
    && mkdir -p /var/lib/metin2 \
    && chown metin2:metin2 /var/lib/metin2

COPY --from=build /opt/metin2 /opt/metin2

USER metin2
WORKDIR /var/lib/metin2

ENTRYPOINT ["/opt/metin2/bin/metin2-runtime", "container"]
