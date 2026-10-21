FROM docker.io/library/debian:13-slim AS build

ARG SANITIZE=OFF

RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential clang cmake ninja-build git ca-certificates libssl-dev zlib1g-dev liblzo2-dev pkgconf libmariadb3 libmariadb-dev \
        $([ "$SANITIZE" = ON ] && echo libclang-rt-19-dev) \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

COPY src/ src/
COPY vendor/ vendor/
COPY CMakeLists.txt ./

RUN CC=clang CXX=clang++ cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/opt/metin2 -DBUILD_TESTING=OFF -DM2_SANITIZE=$SANITIZE \
    && cmake --build build --parallel 4

RUN cmake --install build --prefix /opt/metin2

FROM docker.io/library/debian:13-slim

ARG SANITIZE=OFF

# A sanitizer build also gets llvm-symbolizer, so reports carry file and line.
RUN apt-get update \
    && apt-get install -y --no-install-recommends libssl3t64 zlib1g liblzo2-2 libmariadb3 ca-certificates \
        $([ "$SANITIZE" = ON ] && echo llvm) \
    && rm -rf /var/lib/apt/lists/* \
    && groupadd --gid 10001 metin2 \
    && useradd --uid 10001 --gid metin2 --no-create-home --home-dir /var/lib/metin2 metin2 \
    && mkdir -p /var/lib/metin2/log /var/lib/metin2/mark \
    && chown -R metin2:metin2 /var/lib/metin2

COPY --from=build /opt/metin2 /opt/metin2

# Read only by sanitizer builds: reports of both sanitizers go to log/sanitizer.<pid>.
ENV ASAN_OPTIONS=log_path=/var/lib/metin2/log/sanitizer:detect_leaks=1 \
    UBSAN_OPTIONS=print_stacktrace=1

USER metin2
WORKDIR /var/lib/metin2

ENTRYPOINT ["/opt/metin2/bin/server"]
