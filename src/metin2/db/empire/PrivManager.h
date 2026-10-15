// vim: ts=8 sw=4
#pragma once

#include <metin2/db/clients/Peer.h>

#include <metin2/proto/length.h>
#include <metin2/common/singleton.h>

#include <cstdint>
#include <ctime>
#include <functional>
#include <map>
#include <queue>
#include <utility>
#include <vector>

namespace metin2::db::inline empire {

struct TPrivEmpireData {
    uint8_t type;
    int value;
    bool bRemoved;
    uint8_t empire;

    // ADD_EMPIRE_PRIV_TIME
    time_t end_time_sec;

    TPrivEmpireData(uint8_t type, int value, uint8_t empire, time_t end_time_sec)
        : type(type), value(value), bRemoved(false), empire(empire), end_time_sec(end_time_sec) {}
    // END_OF_ADD_EMPIRE_PRIV_TIME
};

/**
 * @version 05/06/08	Bang2ni - added duration
 */
struct TPrivGuildData {
    uint8_t type;
    int value;
    bool bRemoved;
    uint32_t guild_id;

    // ADD_GUILD_PRIV_TIME
    time_t end_time_sec; ///< Duration

    TPrivGuildData(uint8_t type, int value, uint32_t guild_id, time_t _end_time_sec)
        : type(type), value(value), bRemoved(false), guild_id(guild_id), end_time_sec(_end_time_sec) {}
    // END_OF_ADD_GUILD_PRIV_TIME
};

struct TPrivCharData {
    uint8_t type;
    int value;
    bool bRemoved;
    uint32_t pid;
    TPrivCharData(uint8_t type, int value, uint32_t pid)
        : type(type), value(value), bRemoved(false), pid(pid) {}
};

/**
 * @version 05/06/08	Bang2ni - added duration to Guild privilege functions
 */
class CPrivManager : public singleton<CPrivManager> {
  public:
    CPrivManager();
    ~CPrivManager() override;

    // ADD_GUILD_PRIV_TIME
    void AddGuildPriv(uint32_t guild_id, uint8_t type, int value, time_t time_sec);
    // END_OF_ADD_GUILD_PRIV_TIME

    // ADD_EMPIRE_PRIV_TIME
    void AddEmpirePriv(uint8_t empire, uint8_t type, int value, time_t time_sec);
    // END_OF_ADD_EMPIRE_PRIV_TIME

    void AddCharPriv(uint32_t pid, uint8_t type, int value);

    void Update();

    void SendPrivOnSetup(CPeer *peer);

  private:
    // ADD_GUILD_PRIV_TIME
    void SendChangeGuildPriv(uint32_t guild_id, uint8_t type, int value, time_t end_time_sec);
    // END_OF_ADD_GUILD_PRIV_TIME

    // ADD_EMPIRE_PRIV_TIME
    void SendChangeEmpirePriv(uint8_t empire, uint8_t type, int value, time_t end_time_sec);
    // END_OF_ADD_EMPIRE_PRIV_TIME

    void SendChangeCharPriv(uint32_t pid, uint8_t type, int value);

    typedef std::pair<time_t, TPrivCharData *> stPairChar;
    typedef std::pair<time_t, TPrivGuildData *> stPairGuild;
    typedef std::pair<time_t, TPrivEmpireData *> stPairEmpire;

    std::priority_queue<stPairChar, std::vector<stPairChar>, std::greater<stPairChar>>
        m_pqPrivChar;
    std::priority_queue<stPairGuild, std::vector<stPairGuild>, std::greater<stPairGuild>>
        m_pqPrivGuild;
    std::priority_queue<stPairEmpire, std::vector<stPairEmpire>, std::greater<stPairEmpire>>
        m_pqPrivEmpire;

    TPrivEmpireData *m_aaPrivEmpire[MAX_PRIV_NUM][EMPIRE_MAX_NUM];
    std::map<uint32_t, TPrivGuildData *> m_aPrivGuild[MAX_PRIV_NUM];
    std::map<uint32_t, TPrivCharData *> m_aPrivChar[MAX_PRIV_NUM];
};
}
