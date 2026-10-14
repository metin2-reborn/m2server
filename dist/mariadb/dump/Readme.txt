These dumps were produced by MariaDB 10.6.21 and are used to initialize the
project's MariaDB 11.4 container. See ../README.md for startup, credentials,
import behavior, persistence, and backup instructions.

The first initialization creates account, common, player, log, and hotbackup
and imports each matching .sql file into its database, including existing data.
The hotbackup database is required even though its dump contains no tables.
Use sql_mode=NO_ENGINE_SUBSTITUTION to retain compatibility with the legacy
zero-date defaults and game queries. Existing volumes are not reimported.
