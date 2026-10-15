set shell := ["sh", "-eu", "-c"]

image := "localhost/metin2-server:linux"

# Clang unless the environment picks another compiler.
export CC := env("CC", "clang")
export CXX := env("CXX", "clang++")

# Configure the build directory (idempotent).
configure:
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE="${BUILD_TYPE:-Debug}"
    ln -sf build/compile_commands.json compile_commands.json

# Configure and compile the server natively into build/ (BUILD_TYPE defaults to Debug).
build: configure
    cmake --build build --parallel "${BUILD_JOBS:-4}"

# Build natively and run the unit tests.
test: build
    ctest --test-dir build --output-on-failure

# Build the image and (re)start the compose stack; volumes and the database are kept.
deploy:
    # The containers run as a subuid that only gets the "other" permission bits on the mounted data.
    chmod -R a+rX dist/share dist/config
    # Afterwards remove only what this build left behind: the replaced image and the build stage.
    start=$(date +%s); \
    old=$(podman image inspect -f '{{{{.Id}}' {{image}} 2>/dev/null || true); \
    podman build -t {{image}} .; \
    podman compose up -d --force-recreate; \
    for id in $(podman images -q --no-trunc --filter dangling=true); do \
        id=${id#sha256:}; \
        if [ "$id" = "$old" ] || [ "$(podman image inspect -f '{{{{.Created.Unix}}' "$id")" -ge "$start" ]; then \
            podman rmi "$id" >/dev/null 2>&1 || true; \
        fi; \
    done
    for i in $(seq 60); do \
        status=$(podman inspect -f '{{{{.State.Health.Status}}' "$(podman ps -q --filter label=com.docker.compose.project.working_dir={{justfile_directory()}} --filter label=com.docker.compose.service=channel1-first)" 2>/dev/null || true); \
        [ "$status" = healthy ] && { podman compose ps; exit 0; }; \
        sleep 3; \
    done; \
    echo "channel1-first did not become healthy; see podman compose logs" >&2; exit 1

# Remove the native build products.
clean:
    rm -rf -- build
    