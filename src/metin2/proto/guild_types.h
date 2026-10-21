#pragma once

namespace metin2::inline proto {
    inline constexpr int GUILD_NAME_MAX_LEN = 12;
    inline constexpr int GUILD_MAX_LEVEL = 20;

    enum guild_war_type {
        GUILD_WAR_TYPE_FIELD,
        GUILD_WAR_TYPE_BATTLE,
        GUILD_WAR_TYPE_FLAG,
        GUILD_WAR_TYPE_MAX_NUM
    };

    enum guild_war_state {
        GUILD_WAR_NONE,
        GUILD_WAR_SEND_DECLARE,
        GUILD_WAR_REFUSE,
        GUILD_WAR_RECV_DECLARE,
        GUILD_WAR_WAIT_START,
        GUILD_WAR_CANCEL,
        GUILD_WAR_ON_WAR,
        GUILD_WAR_END,
        GUILD_WAR_OVER,
        GUILD_WAR_RESERVE,

        GUILD_WAR_DURATION = 30 * 60,
        GUILD_WAR_WIN_POINT = 1000,
        GUILD_WAR_LADDER_HALF_PENALTY_TIME = 12 * 60 * 60,
    };
}
