#!/bin/bash
# Sourced by the MariaDB entrypoint (docker-entrypoint-initdb.d). The body runs in a subshell so its
# shell options do not leak into the entrypoint: with `set -u` the entrypoint's own
# docker_process_sql failed on "$1: unbound variable" and the metin2 user was never created.
(
set -eo pipefail
: "${DB_USER:?DB_USER must be set}" "${DB_PASSWORD:?DB_PASSWORD must be set}"
# NO_BACKSLASH_ESCAPES and doubled quotes make SQL literals independent of password contents.
[ "${DB_USER,,}" != root ] || { echo "DB_USER must not be root" >&2; exit 1; }
sql_quote() { local value="${1//\'/\'\'}"; printf "'%s'" "$value"; }
user=$(sql_quote "$DB_USER")
password=$(sql_quote "$DB_PASSWORD")
docker_process_sql <<SQL
SET SESSION sql_mode='NO_BACKSLASH_ESCAPES';
CREATE USER $user@'%' IDENTIFIED BY $password;
CREATE DATABASE account;
CREATE DATABASE common;
CREATE DATABASE player;
CREATE DATABASE log;
CREATE DATABASE hotbackup;
GRANT ALL PRIVILEGES ON account.* TO $user@'%';
GRANT ALL PRIVILEGES ON common.* TO $user@'%';
GRANT ALL PRIVILEGES ON player.* TO $user@'%';
GRANT ALL PRIVILEGES ON log.* TO $user@'%';
GRANT ALL PRIVILEGES ON hotbackup.* TO $user@'%';
SQL
for database in account common player log hotbackup; do
    docker_process_sql --database="$database" < "${METIN2_DUMP_DIR:-/metin2-dumps}/$database.sql"
done
)
