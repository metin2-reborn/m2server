# Preserved vendor modifications

`lua/llex.c` and `lua/llex.h` are the quest-aware lexer supplied with this server's Lua 5.0.3. They add quest/state/when/with/begin tokens and preserve the original multibyte string handling. FetchContent installs these replacement source files over the pinned upstream release before compiling both the runtime and `qc`.

Licenses are preserved under `vendor/licenses` and included in installed server images.
