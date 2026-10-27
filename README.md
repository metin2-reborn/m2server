# Metin2 Server

[![CI](https://github.com/metin2-reborn/m2server/actions/workflows/ci.yml/badge.svg)](https://github.com/metin2-reborn/m2server/actions/workflows/ci.yml)
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
- **One binary:** a single `server` executable runs as the database cache, the login server or a game
  channel, chosen by its TOML configuration.
- **Simple deployment:** one image with the binary; configuration and game data are mounted, not baked in.

## Components

Every process runs the same `server` binary; the `role` in its configuration decides what it does.

| Service          | Role   | Description                                           | Port             |
|------------------|--------|-------------------------------------------------------|------------------|
| `db`             | `db`   | Database cache between the game processes and MariaDB | 15000 (internal) |
| `auth`           | `auth` | Login server                                          | 11000            |
| `channel1-first` | `game` | Game server for channel 1, serving every map          | 13000, P2P 14000 |
| `web`            | —      | Account API from [`../m2web`](../m2web)                 | 8080             |
| `site`           | —      | Website from `../m2web`: home, download, registration, account pages | 8081 |

Source layout: folders match namespaces, all under `metin2`, and headers are included as
`<metin2/...>`.

```
src/metin2/
  main.cpp     entry point: loads the configuration and starts the role
  config/      TOML configuration (metin2::config)
  log/         logging (metin2::log)
  core/        main loop, timing, utilities
  net/         networking on top of Asio
  sql/         asynchronous MariaDB access
  poly/        expression evaluator
  common/      shared helpers (grids, caches)
  proto/       tables and packets shared between processes
  db/          database cache (metin2::db): clients, caches, login, guilds, ...
  game/        game and login server (metin2::game): characters, items, quests, protocol, ...
tests/         unit tests (CTest)
vendor/        third-party dependencies (fetched by CMake)
dist/          runtime: configuration, game data, database dumps
```

## Requirements

To build natively:

- Linux
- Clang with C++23 support (Clang 19 or newer)
- CMake 3.25+ and Ninja
- Development packages for OpenSSL, zlib, LZO 2, MariaDB Connector/C (3.4+) and pkg-config

Asio, Lua 5.0, Monocypher, stb and toml++ are downloaded by CMake during configuration.

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

`SANITIZE=1 just deploy` builds the servers with AddressSanitizer and UndefinedBehaviorSanitizer instead;
reports go to `log/sanitizer.<pid>` in each service's log volume. Deploy again without it to go back.

Without `just`:

```sh
podman build -t localhost/metin2-server:linux .
podman compose up -d
```

Point the client at `PUBLIC_IP`, port `11000`.

The `web` (API) and `site` (website) services share one image built from the sibling `m2web` checkout.
Their variables are listed in `.env.example` (`SITE_URL`, `SITE_API_KEY`, `JWT_SECRET`, `MAIL_MODE`,
`SMTP_*`). `podman compose build web && podman compose up -d --no-deps --force-recreate web site`
updates only these two.

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
just test                   # build, then run the unit tests with CTest
just clean
```

Or with CMake directly:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The binary ends up in `build/bin/server`. `just build` also links `compile_commands.json` into the project
root for editors and `clang-tidy`.

## Runtime layout

Every service runs `/opt/metin2/bin/server` with `/var/lib/metin2` as its working directory. Compose
mounts what each one needs there:

| Host path                              | Mounted at (in `/var/lib/metin2`)     | Services       |
|----------------------------------------|---------------------------------------|----------------|
| `dist/config/<service>/server.toml`    | `server.toml`                         | all            |
| `dist/share/conf/{item,mob}_*.txt`     | `item_proto.txt`, `mob_names.txt`…    | db             |
| `dist/share/data`, `dist/share/locale` | `data`, `locale`                      | auth, game     |
| `dist/share/conf/CMD`                  | `CMD`                                 | auth, game     |
| volume `<service>-log`                 | `log`                                 | all            |
| volume `channel1-first-mark`           | `mark` (guild marks)                  | channel1-first |
| `dist/share/mark`                      | `mark_seed` (copied once into `mark`) | channel1-first |
| `dist/mariadb`                         | MariaDB container                     | mariadb        |

A server writes `ready` into its working directory once it accepts connections; the health checks test
for that file. The auth server also reads an optional `BANIP` file there (one address, or a tab-separated
`start end` range, per line); mount one into the `auth` service to block addresses. Logs go to `log/syslog` and `log/syserr` and are rotated hourly into `log/YYYYMMDD/`.
Files under `dist/share` and `dist/config` must be world-readable, since rootless containers access them
as an unprivileged user (`just deploy` takes care of this).

## Configuration

Each process reads `server.toml` from its working directory. Strings may contain `${NAME}`, which is
replaced by the environment variable `NAME`; the shipped files take the database credentials, the public
address and the admin password from `.env` that way. A minimal game channel:

```toml
role = "game"            # "db", "auth" or "game"

[log]
level = "info"           # "debug", "info", "warn" or "error"

[server]
hostname = "first"
channel = 1
port = 13000
p2p_port = 14000
db_host = "127.0.0.1"
db_port = 15000

[game]
map_allow = [1, 3, 4]
```

Unknown keys, missing required keys and values of the wrong type stop the server at startup, with the
file and line of the problem. The files in `dist/config` show every section in use;
`src/metin2/config/settings.h` lists all options.

## Quests

Quests are `.quest` files in `dist/share/locale/english/quest`. Game servers read every `.quest` file in
that folder when they start and on `/reload q`; there is no compile step. Files in `quest/disabled/` are
not loaded, so a quest is switched off or on by moving it. A syntax error stops the server with the file
and line of the problem.

## Contributing

Code style is defined by `.clang-format` and `.clang-tidy` in the repository root. Names follow the
standard library: `snake_case` for namespaces, types, functions and variables, a trailing `_` for private
and protected data members, `UPPER_CASE` for constants and enumerators. `clang-tidy` checks this.

Please format new code with `clang-format` and keep headers self-contained.

## Third-party software

Licenses of bundled and fetched dependencies are in [`vendor/licenses`](vendor/licenses).
