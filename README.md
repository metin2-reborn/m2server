# Metin2 Server

![C++23](https://img.shields.io/badge/C%2B%2B-23-00599C?logo=cplusplus)
![Platform](https://img.shields.io/badge/platform-Linux-FCC624?logo=linux&logoColor=black)
![Podman](https://img.shields.io/badge/deploy-Podman%20Compose-892CA0?logo=podman)
[![Last commit](https://img.shields.io/github/last-commit/metin2-reborn/m2server)](https://github.com/metin2-reborn/m2server/commits)
[![Issues](https://img.shields.io/github/issues/metin2-reborn/m2server)](https://github.com/metin2-reborn/m2server/issues)

A cleaned-up Metin2 server for Linux, written in modern C++. It builds with Clang and CMake and runs as a
small container stack next to MariaDB.

- **Linux first:** built and deployed on Linux, with self-contained headers and no precompiled headers.
- **Modern C++:** C++23, standalone Asio for networking, X25519 + ChaCha20 (Monocypher) for connection
  encryption.
- **UTF-8 end to end:** text in the code, the game data and the database is UTF-8.
- **Simple deployment:** one image with the binaries; configuration and game data are mounted, not baked in.

## Components

| Process          | Description                                                    | Port             |
|------------------|----------------------------------------------------------------|------------------|
| `db`             | Database cache between the game processes and MariaDB          | 15000 (internal) |
| `auth`           | Login server (`server` binary in auth mode)                    | 11000            |
| `channel1-first` | Game server for channel 1, serving every map (`server` binary) | 13000, P2P 14000 |

Source layout:

```
src/
  game/        game server (the `server` executable)
  db/          database cache (the `db` executable)
  common/      shared tables and packet definitions
  libgame/     game helpers (grids, attributes)
  libnet/      networking on top of Asio
  libpoly/     polygon/expression library
  libsql/      asynchronous MariaDB access
  libthecore/  logging, timing, main loop
vendor/        third-party dependencies (fetched by CMake)
dist/          runtime: launcher, configuration, game data, database dumps
```

## Requirements

To build natively:

- Linux
- Clang with C++23 support (Clang 19 or newer)
- CMake 3.25+ and Ninja
- Development packages for OpenSSL, zlib, LZO 2, MariaDB Connector/C (3.4+) and pkg-config

Asio, Lua 5.0, Monocypher and stb are downloaded by CMake during configuration.

On Debian or Ubuntu:

```sh
sudo apt install build-essential clang cmake ninja-build pkgconf \
    libssl-dev zlib1g-dev liblzo2-dev libmariadb-dev
```

To deploy:

- [Podman](https://podman.io/) with `podman compose`
- [just](https://github.com/casey/just) (optional, for the recipes below)

## Getting started

### 1. Configure

```sh
git clone https://github.com/metin2-reborn/m2server.git
cd m2server
cp .env.example .env
```

Edit `.env`:

| Variable                | Purpose                                                  |
|-------------------------|----------------------------------------------------------|
| `PUBLIC_IP`             | IPv4 address clients use to reach this host              |
| `DB_USER`, `DB_PASSWORD`| MariaDB account used by the servers                      |
| `MARIADB_ROOT_PASSWORD` | MariaDB root password                                    |
| `ADMIN_PASSWORD`        | Password for the in-game admin account                   |

### 2. Deploy

```sh
just deploy
```

This builds the image, starts MariaDB, `db`, `auth` and `channel1-first`, and waits until the channel is
healthy. On first start MariaDB imports the dumps in `dist/mariadb/dump`. Database contents and server
state are kept in named volumes across redeploys.

Without `just`:

```sh
podman build -t localhost/metin2-server:linux .
podman compose up -d
```

Point the client at `PUBLIC_IP`, port `11000`.

### 3. Operate

```sh
podman compose ps                     # status and health
podman compose logs -f channel1-first # follow a service
podman compose down                   # stop (volumes are kept)
```

## Building natively

```sh
just build                  # Debug build into build/
BUILD_TYPE=Release just build
just clean
```

Or with CMake directly:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The binaries end up in `build/bin/` (`server` and `db`). `just build` also links `compile_commands.json`
into the project root for editors and `clang-tidy`.

## Runtime layout

| Path on the host           | Mounted at                | Contents                               |
|----------------------------|---------------------------|----------------------------------------|
| `dist/share`               | `/srv/metin2/share`       | Game data, locale and quests           |
| `dist/config/<service>`    | `/srv/metin2/config`      | Per-service configuration              |
| `dist/mariadb`             | MariaDB container         | Server settings, init script and dumps |

`dist/metin2-runtime` is the container entrypoint: it fills in the configuration from the environment,
waits for its dependencies and starts the right binary. Files under `dist/share` and `dist/config` must be
world-readable, since rootless containers access them as an unprivileged user (`just deploy` takes care
of this).

## Contributing

Code style is defined by `.clang-format` and `.clang-tidy` in the repository root. 

Please format new code with `clang-format` and keep headers self-contained.

## Third-party software

Licenses of bundled and fetched dependencies are in [`vendor/licenses`](vendor/licenses).
