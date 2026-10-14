#pragma once

#include "Peer.h"

#include <common/singleton.h>

#include <cstdint>
#include <string>
#include <vector>

// Blocked IP ranges (account.iptocountry) and the logins exempt from them
// (account.block_exception), forwarded to the game servers.
class CBlockCountry : public singleton<CBlockCountry> {
  private:
    struct BLOCK_IP {
        uint32_t ip_from;
        uint32_t ip_to;
    };

    std::vector<BLOCK_IP> m_block_ip;
    std::vector<std::string> m_block_exception;

  public:
    auto Load() -> bool;
    void SendBlockedCountryIp(CPeer *peer);
    void SendBlockException(CPeer *peer);
    void SendBlockExceptionOne(CPeer *peer, const char *login, uint8_t cmd);
    void AddBlockException(std::string_view login);
    void DelBlockException(std::string_view login);
};
