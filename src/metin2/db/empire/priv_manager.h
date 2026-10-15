// vim: ts=8 sw=4
#pragma once

#include <metin2/db/clients/peer.h>

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

struct priv_empire_data {
    uint8_t type;
    int value;
    bool removed;
    uint8_t empire;

    // ADD_EMPIRE_PRIV_TIME
    time_t end_time_sec;

    priv_empire_data(uint8_t type, int value, uint8_t empire, time_t end_time_sec)
        : type(type), value(value), removed(false), empire(empire), end_time_sec(end_time_sec) {}
    // END_OF_ADD_EMPIRE_PRIV_TIME
};

/**
 * @version 05/06/08	Bang2ni - added duration
 */
struct priv_guild_data {
    uint8_t type;
    int value;
    bool removed;
    uint32_t guild_id;

    // ADD_GUILD_PRIV_TIME
    time_t end_time_sec; ///< Duration

    priv_guild_data(uint8_t type, int value, uint32_t guild_id, time_t end_time_sec_value)
        : type(type), value(value), removed(false), guild_id(guild_id), end_time_sec(end_time_sec_value) {}
    // END_OF_ADD_GUILD_PRIV_TIME
};

struct priv_char_data {
    uint8_t type;
    int value;
    bool removed;
    uint32_t pid;
    priv_char_data(uint8_t type, int value, uint32_t pid)
        : type(type), value(value), removed(false), pid(pid) {}
};

/**
 * @version 05/06/08	Bang2ni - added duration to Guild privilege functions
 */
class priv_manager : public singleton<priv_manager> {
  public:
    priv_manager();
    ~priv_manager() override;

    // ADD_GUILD_PRIV_TIME
    void add_guild_priv(uint32_t guild_id, uint8_t type, int value, time_t duration_sec);
    // END_OF_ADD_GUILD_PRIV_TIME

    // ADD_EMPIRE_PRIV_TIME
    void add_empire_priv(uint8_t empire, uint8_t type, int value, time_t duration_sec);
    // END_OF_ADD_EMPIRE_PRIV_TIME

    void add_char_priv(uint32_t pid, uint8_t type, int value);

    void update();

    void send_priv_on_setup(peer *peer);

  private:
    // ADD_GUILD_PRIV_TIME
    void send_change_guild_priv(uint32_t guild_id, uint8_t type, int value, time_t end_time_sec);
    // END_OF_ADD_GUILD_PRIV_TIME

    // ADD_EMPIRE_PRIV_TIME
    void send_change_empire_priv(uint8_t empire, uint8_t type, int value, time_t end_time_sec);
    // END_OF_ADD_EMPIRE_PRIV_TIME

    void send_change_char_priv(uint32_t pid, uint8_t type, int value);

    typedef std::pair<time_t, priv_char_data *> st_pair_char;
    typedef std::pair<time_t, priv_guild_data *> st_pair_guild;
    typedef std::pair<time_t, priv_empire_data *> st_pair_empire;

    std::priority_queue<st_pair_char, std::vector<st_pair_char>, std::greater<st_pair_char>>
        pq_priv_char_;
    std::priority_queue<st_pair_guild, std::vector<st_pair_guild>, std::greater<st_pair_guild>>
        pq_priv_guild_;
    std::priority_queue<st_pair_empire, std::vector<st_pair_empire>, std::greater<st_pair_empire>>
        pq_priv_empire_;

    priv_empire_data *aa_priv_empire_[MAX_PRIV_NUM][EMPIRE_MAX_NUM];
    std::map<uint32_t, priv_guild_data *> priv_guild_[MAX_PRIV_NUM];
    std::map<uint32_t, priv_char_data *> priv_char_[MAX_PRIV_NUM];
};
}
