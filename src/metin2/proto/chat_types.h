#pragma once

namespace metin2::inline proto {
    inline constexpr int CHAT_MAX_LEN = 512;

    enum chat_type {
        CHAT_TYPE_TALKING,
        CHAT_TYPE_INFO,
        CHAT_TYPE_NOTICE,
        CHAT_TYPE_PARTY,
        CHAT_TYPE_GUILD,
        CHAT_TYPE_COMMAND,
        CHAT_TYPE_SHOUT,
        CHAT_TYPE_WHISPER,
        CHAT_TYPE_BIG_NOTICE,
        CHAT_TYPE_MONARCH_NOTICE,
        CHAT_TYPE_MAX_NUM
    };

    enum whisper_type {
        WHISPER_TYPE_NORMAL = 0,
        WHISPER_TYPE_NOT_EXIST = 1,
        WHISPER_TYPE_TARGET_BLOCKED = 2,
        WHISPER_TYPE_SENDER_BLOCKED = 3,
        WHISPER_TYPE_ERROR = 4,
        WHISPER_TYPE_GM = 5,
        WHISPER_TYPE_SYSTEM = 0xFF
    };
}
