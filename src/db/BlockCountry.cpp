#include "BlockCountry.h"

#include "DBManager.h"
#include "Peer.h"

#include <common/tables.h>
#include <libsql/AsyncSQL.h>
#include <libthecore/log.h>

#include <mysql.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

auto CBlockCountry::Load() -> bool {
    {
        const char *szQuery = "SELECT IP_FROM, IP_TO, COUNTRY_NAME FROM iptocountry";
        const std::unique_ptr<SQLMsg> pMsg(CDBManager::instance().DirectQuery(szQuery, SQL_ACCOUNT));

        if (pMsg->Get()->uiNumRows == 0) {
            sys_err(" DirectQuery failed(%s)", szQuery);
            return false;
        }

        MYSQL_ROW row;
        while ((row = mysql_fetch_row(pMsg->Get()->pSQLResult)) != nullptr) {
            const auto block_ip = BLOCK_IP{
                .ip_from = static_cast<uint32_t>(std::strtoul(row[0], nullptr, 10)),
                .ip_to = static_cast<uint32_t>(std::strtoul(row[1], nullptr, 10)),
            };

            m_block_ip.push_back(block_ip);

            sys_log(0, "BLOCKED_IP : %u - %u", block_ip.ip_from, block_ip.ip_to);
        }
    }

    // load block exception account
    {
        const char *szQuery = "SELECT login FROM block_exception";
        const std::unique_ptr<SQLMsg> pMsg(CDBManager::instance().DirectQuery(szQuery, SQL_ACCOUNT));

        if (pMsg->Get()->uiNumRows == 0) {
            sys_err(" DirectQuery failed(%s)", szQuery);
            return true;
        }

        MYSQL_ROW row;
        while ((row = mysql_fetch_row(pMsg->Get()->pSQLResult)) != nullptr) {
            m_block_exception.emplace_back(row[0]);

            sys_log(0, "BLOCK_EXCEPTION = %s", row[0]);
        }
    }

    return true;
}

void CBlockCountry::SendBlockedCountryIp(CPeer *peer) {
    sys_log(0, "SendBlockedCountryIp start");

    for (const auto &block_ip : m_block_ip) {
        auto packet = TPacketBlockCountryIp{
            .ip_from = block_ip.ip_from,
            .ip_to = block_ip.ip_to,
        };

        peer->EncodeHeader(HEADER_DG_BLOCK_COUNTRY_IP, 0, sizeof(TPacketBlockCountryIp));
        peer->Encode(&packet, sizeof(packet));
    }

    sys_log(0, "[DONE] CBlockCountry::SendBlockedCountryIp() : count = %zu", m_block_ip.size());
    sys_log(0, "SendBlockedCountryIp end");
}

void CBlockCountry::SendBlockException(CPeer *peer) {
    for (const auto &login : m_block_exception) {
        SendBlockExceptionOne(peer, login.c_str(), BLOCK_EXCEPTION_CMD_ADD);
    }
}

void CBlockCountry::SendBlockExceptionOne(CPeer *peer, const char *login, uint8_t cmd) {
    if (peer == nullptr || login == nullptr) {
        return;
    }

    if (BLOCK_EXCEPTION_CMD_ADD != cmd && BLOCK_EXCEPTION_CMD_DEL != cmd) {
        return;
    }

    TPacketBlockException packet;
    packet.cmd = cmd;
    strlcpy(packet.login, login, sizeof(packet.login));

    peer->EncodeHeader(HEADER_DG_BLOCK_EXCEPTION, 0, sizeof(TPacketBlockException));
    peer->Encode(&packet, sizeof(packet));
}

void CBlockCountry::AddBlockException(std::string_view login) {
    if (std::ranges::find(m_block_exception, login) == m_block_exception.end()) {
        m_block_exception.emplace_back(login);
    }
}

void CBlockCountry::DelBlockException(std::string_view login) {
    if (const auto it = std::ranges::find(m_block_exception, login); it != m_block_exception.end()) {
        m_block_exception.erase(it);
    }
}
