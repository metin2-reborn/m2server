add_executable(db
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
    Config.cpp
    Config.h
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
    version.cpp
)

target_compile_definitions(db PRIVATE __P4_VERSION__="40142")
target_link_libraries(db PRIVATE common game net poly sql thecore)
target_include_directories(db PRIVATE ../)
