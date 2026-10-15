add_library(metin2_db STATIC
    Cache.cpp
    Cache.h
    ClientManager.cpp
    ClientManager.h
    ClientManagerBoot.cpp
    ClientManagerEventFlag.cpp
    ClientManagerGuild.cpp
    ClientManagerHorseName.cpp
    ClientManagerLogin.cpp
    ClientManagerParty.cpp
    ClientManagerPlayer.cpp
    CsvReader.cpp
    CsvReader.h
    DBManager.cpp
    DBManager.h
    GuildManager.cpp
    GuildManager.h
    ItemAwardManager.cpp
    ItemAwardManager.h
    ItemIDRangeManager.cpp
    ItemIDRangeManager.h
    LoginData.cpp
    LoginData.h
    Main.cpp
    Main.h
    Marriage.cpp
    Marriage.h
    Monarch.cpp
    Monarch.h
    MoneyLog.cpp
    MoneyLog.h
    Peer.cpp
    Peer.h
    PrivManager.cpp
    PrivManager.h
    ProtoReader.cpp
    ProtoReader.h
    QID.h
)

target_link_libraries(metin2_db PUBLIC common game metin2_config metin2_log net poly sql thecore)
target_include_directories(metin2_db PRIVATE ../)
