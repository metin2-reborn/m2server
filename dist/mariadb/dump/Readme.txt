These dumps were produced by MariaDB 10.6.21 and are used to initialize the
project's MariaDB 11.4 container. See ../README.md for startup, credentials,
import behavior, persistence, and backup instructions.

The first initialization creates account, common, player and log and imports
each matching .sql file into its database, including existing data.
Use sql_mode=NO_ENGINE_SUBSTITUTION to retain compatibility with the legacy
zero-date defaults and game queries. Existing volumes are not reimported.

All tables use utf8mb4 (collation utf8mb4_uca1400_as_ci) and the servers connect with utf8mb4.
Volumes created before that change are converted once with ../migrations/migrate-utf8mb4.sql:
  podman exec -i <mariadb container> mariadb -uroot -p<root password> < migrations/migrate-utf8mb4.sql
Stop db, auth and the channels first and back up the databases.
