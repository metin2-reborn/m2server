#!/bin/bash
# Prints the SQL for the website's database user (m2portal): only the tables the account API
# works on. Used by init-databases.sh and by `just db-web-user` for existing volumes.
set -eo pipefail
: "${WEB_DB_USER:?WEB_DB_USER must be set}" "${WEB_DB_PASSWORD:?WEB_DB_PASSWORD must be set}"
sql_quote() { local value="${1//\'/\'\'}"; printf "'%s'" "$value"; }
user="$(sql_quote "$WEB_DB_USER")@'%'"
password=$(sql_quote "$WEB_DB_PASSWORD")
cat <<SQL
SET SESSION sql_mode='NO_BACKSLASH_ESCAPES';
CREATE USER IF NOT EXISTS $user IDENTIFIED BY $password;
ALTER USER $user IDENTIFIED BY $password REQUIRE NONE;
GRANT SELECT, INSERT, UPDATE, DELETE ON account.account TO $user;
GRANT SELECT, INSERT, DELETE ON account.account_token TO $user;
GRANT SELECT ON player.player TO $user;
SQL
