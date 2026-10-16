#include <metin2/config/config.h>
#include <metin2/game/protocol/input.h>

#include <metin2/game/admin/cmd.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/empire/priv_manager.h>
#include <metin2/game/events/fishing.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/protocol/desc_manager.h>
#include <metin2/game/protocol/p2p.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/protocol/packet_info.h>
#include <metin2/game/quest/questmanager.h>

#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace metin2::game::inline protocol {

    auto is_empty_admin_page() -> bool {
        return metin2::config::server().admin.ips.empty();
    }

    auto is_admin_page(const char *ip) -> bool {
        for (size_t n = 0; n < metin2::config::server().admin.ips.size(); ++n) {
            if (metin2::config::server().admin.ips[n] == ip)
                return 1;
        }
        return 0;
    }

    input_processor::input_processor() : packet_info_(nullptr), buffer_left_(0) {
        if (!packet_info_)
            bind_packet_info(&packet_info_cg_);
    }

    void input_processor::bind_packet_info(packet_info *packet_info_value) {
        packet_info_ = packet_info_value;
    }

    auto input_processor::process(desc *desc_value, const void *orig, int bytes, int &bytes_processed) -> bool {
        const char *c_pData = (const char *)orig;

        uint8_t bLastHeader = 0;
        int iLastPacketLen = 0;
        int iPacketLen;

        if (!packet_info_) {
            metin2::log::error("Input processor has no packet table");
            return true;
        }

        for (buffer_left_ = bytes; buffer_left_ > 0;) {
            uint8_t bHeader = (uint8_t)*(c_pData);
            const char *c_pszName;

            if (bHeader == 0)
                iPacketLen = 1;
            else if (!packet_info_->get(bHeader, &iPacketLen, &c_pszName)) {
                metin2::log::warn("Unknown packet header {:d} (last header {:d}, length {:d}, {:d} bytes left, host {}), closing",
                    bHeader, bLastHeader, iLastPacketLen, buffer_left_, desc_value->host_name());
                desc_value->set_phase(PHASE_CLOSE);
                return true;
            }

            if (buffer_left_ < iPacketLen)
                return true;

            if (bHeader) {
                if (metin2::config::test_server() && bHeader != HEADER_CG_MOVE)
                    metin2::log::debug("Packet header {:d}, {:d} bytes buffered", bHeader, buffer_left_);

                packet_info_->start();

                const int iBufferLeft = buffer_left_;

                if (packet_info_->is_sequence(bHeader))
                    --buffer_left_;

                int iExtraPacketSize = analyze(desc_value, bHeader, c_pData);
                buffer_left_ = iBufferLeft;

                if (iExtraPacketSize < 0)
                    return true;

                if (iExtraPacketSize > buffer_left_ - iPacketLen) {
                    metin2::log::warn("Packet {} (header {:d}) claims {:d} extra bytes, only {:d} left (host {}), closing",
                        c_pszName, bHeader, iExtraPacketSize, buffer_left_ - iPacketLen, desc_value->host_name());
                    desc_value->set_phase(PHASE_CLOSE);
                    return true;
                }

                iPacketLen += iExtraPacketSize;
                packet_info_->end();
            }

            if (bHeader == HEADER_CG_PONG)
                metin2::log::debug("Ping reply: sequenced {:d}, sequence byte {:d}", packet_info_->is_sequence(bHeader), *(uint8_t *)(c_pData + iPacketLen - sizeof(uint8_t)));

            if (packet_info_->is_sequence(bHeader)) {
                uint8_t bSeq = desc_value->sequence();
                uint8_t bSeqReceived = *(uint8_t *)(c_pData + iPacketLen - sizeof(uint8_t));

                if (bSeq != bSeqReceived) {
                    metin2::log::warn("Packet sequence mismatch on connection {}: expected {:#x}, got {:#x} (header {:d}), closing", static_cast<const void *>(static_cast<void *>(desc_value)), bSeq, bSeqReceived, bHeader);

                    character *ch = desc_value->get_character();

                    char buf[1024];
                    int offset, len;

                    offset = snprintf(buf, sizeof(buf), "SEQUENCE_LOG [%s]-------------\n", ch ? ch->name() : "UNKNOWN");

                    if (offset < 0 || offset >= (int)sizeof(buf))
                        offset = sizeof(buf) - 1;

                    for (size_t i = 0; i < desc_value->seq_vector.size(); ++i) {
                        len = snprintf(buf + offset, sizeof(buf) - offset, "\t[%03d : 0x%x]\n",
                            desc_value->seq_vector[i].hdr,
                            desc_value->seq_vector[i].seq);

                        if (len < 0 || len >= (int)sizeof(buf) - offset)
                            offset += (sizeof(buf) - offset) - 1;
                        else
                            offset += len;
                    }

                    snprintf(buf + offset, sizeof(buf) - offset, "\t[%03d : 0x%x]\n", bHeader, bSeq);
                    metin2::log::warn("{}", buf);

                    desc_value->set_phase(PHASE_CLOSE);
                    return true;
                } else {
                    desc_value->push_seq(bHeader, bSeq);
                    desc_value->set_next_sequence();
                }
            }

            c_pData += iPacketLen;
            buffer_left_ -= iPacketLen;
            bytes_processed += iPacketLen;

            iLastPacketLen = iPacketLen;
            bLastHeader = bHeader;

            if (type() != desc_value->get_input_processor()->type())
                return false;
        }

        return true;
    }

    void input_processor::pong(desc *d) {
        d->set_pong(true);
    }

    void input_processor::handshake(desc *d, const char *data) {
        packet_cg_handshake *p = (packet_cg_handshake *)data;

        if (d->handshake() != p->handshake) {
            metin2::log::warn("Invalid handshake from {}, closing", d->host_name());
            d->set_phase(PHASE_CLOSE);
        } else {
            if (d->is_phase(PHASE_HANDSHAKE)) {
                if (d->handshake_process(p->time, p->delta, false)) {
                    d->send_key_agreement();
                }
            } else
                d->handshake_process(p->time, p->delta, true);
        }
    }

    void input_processor::version(character *ch, const char *data) {
        if (!ch)
            return;

        if (!ch->get_desc())
            return;

        packet_cg_client_version *p = (packet_cg_client_version *)data;

        char szTimestamp[sizeof(p->timestamp)];
        char szFilename[sizeof(p->filename)];
        copy_packet_string(szTimestamp, p->timestamp);
        copy_packet_string(szFilename, p->filename);

        metin2::log::debug("Client version of player {}: {} ({})", ch->name(), szTimestamp, szFilename);
        ch->get_desc()->set_client_version(szTimestamp);
    }

    void login_failure(desc *d, const char *status_value) {
        if (!d)
            return;

        packet_gc_login_failure failurePacket;

        failurePacket.header = HEADER_GC_LOGIN_FAILURE;
        strlcpy(failurePacket.status, status_value, sizeof(failurePacket.status));

        d->packet(&failurePacket, sizeof(failurePacket));
    }

    input_handshake::input_handshake() {
        handshake_packet_info_ = std::make_unique<packet_info_cg>();
        handshake_packet_info_->set_sequence(HEADER_CG_PONG, false);

        main_packet_info_ = packet_info_;
        bind_packet_info(handshake_packet_info_.get());
    }

    input_handshake::~input_handshake() {
    }

    auto input_handshake::analyze(desc *d, uint8_t header_value, const char *data_value) -> int {
        if (header_value == 10)
            return 0;

        if (header_value == HEADER_CG_TEXT) {
            if (is_empty_admin_page() || !is_admin_page(d->host_name())) {
                metin2::log::warn("Admin command from unauthorised host {} blocked", d->host_name());
                d->set_phase(PHASE_CLOSE);
                return -1;
            }

            ++data_value;

            const char *c_pSep = (const char *)memchr(data_value, '\n', buffer_left_ - 1);

            if (!c_pSep)
                return -1;

            if (c_pSep > data_value && *(c_pSep - 1) == '\r')
                --c_pSep;

            std::string stResult;
            std::string stBuf;
            stBuf.assign(data_value, c_pSep - data_value);

            metin2::log::info("Admin command from {}: {}", d->host_name(), stBuf.c_str());

            if (!stBuf.compare("IS_SERVER_UP")) {
                if (no_more_client)
                    stResult = "NO";
                else
                    stResult = "YES";
            } else if (!metin2::config::server().admin.password.empty() && stBuf == metin2::config::server().admin.password) {
                if (!is_empty_admin_page()) {
                    if (!is_admin_page(d->host_name())) {
                        char szTmp[64];
                        snprintf(szTmp, sizeof(szTmp), "WEBADMIN : Wrong Connector : %s", d->host_name());
                        stResult += szTmp;
                    } else {
                        d->set_admin_mode();
                        stResult = "UNKNOWN";
                    }
                } else {
                    d->set_admin_mode();
                    stResult = "UNKNOWN";
                }
            } else if (!stBuf.compare("USER_COUNT")) {
                char szTmp[64];

                if (!is_empty_admin_page()) {
                    if (!is_admin_page(d->host_name())) {
                        snprintf(szTmp, sizeof(szTmp), "WEBADMIN : Wrong Connector : %s", d->host_name());
                    } else {
                        int iTotal;
                        int *paiEmpireUserCount;
                        int iLocal;
                        desc_manager::instance().user_count(iTotal, &paiEmpireUserCount, iLocal);
                        snprintf(szTmp, sizeof(szTmp), "%d %d %d %d %d", iTotal, paiEmpireUserCount[1], paiEmpireUserCount[2], paiEmpireUserCount[3], iLocal);
                    }
                } else {
                    int iTotal;
                    int *paiEmpireUserCount;
                    int iLocal;
                    desc_manager::instance().user_count(iTotal, &paiEmpireUserCount, iLocal);
                    snprintf(szTmp, sizeof(szTmp), "%d %d %d %d %d", iTotal, paiEmpireUserCount[1], paiEmpireUserCount[2], paiEmpireUserCount[3], iLocal);
                }
                stResult += szTmp;
            } else if (d->is_admin_mode() && !stBuf.compare("CHECK_P2P_CONNECTIONS")) {
                std::ostringstream oss(std::ostringstream::out);

                oss << "P2P CONNECTION NUMBER : " << p2p_manager::instance().desc_count() << "\n";
                std::string hostNames;
                p2p_manager::instance().p2p_host_names(hostNames);
                oss << hostNames;
                stResult = oss.str();
                packet_gg_check_awakeness packet;
                packet.header = HEADER_GG_CHECK_AWAKENESS;

                p2p_manager::instance().send(&packet, sizeof(packet));
            } else if (d->is_admin_mode() && !stBuf.compare("PACKET_INFO")) {
                main_packet_info_->log("packet_info.txt");
                stResult = "OK";
            } else if (d->is_admin_mode() && !stBuf.compare(0, 15, "DELETE_AWARDID ")) {
                char szTmp[64];
                std::string msg = stBuf.substr(15, 26);

                packet_delete_award_id p;
                p.id = (uint32_t)(atoi(msg.c_str()));
                snprintf(szTmp, sizeof(szTmp), "Sent to DB cache to delete ItemAward, id: %d", p.id);
                db_clientdesc->db_packet(HEADER_GD_DELETE_AWARDID, 0, &p, sizeof(p));
                stResult += szTmp;
            } else {
                stResult = "UNKNOWN";

                if (d->is_admin_mode()) {
                    if (!stBuf.compare(0, 7, "NOTICE ")) {
                        std::string msg = stBuf.substr(7);
                        msg.resize(utf8_truncated_size(msg.c_str(), msg.size(), 50));
                        log_manager::instance().char_log(0, 0, 0, 1, "NOTICE", msg.c_str(), d->host_name());
                        broadcast_notice(msg.c_str());
                    } else if (!stBuf.compare("SHUTDOWN")) {
                        log_manager::instance().char_log(0, 0, 0, 2, "SHUTDOWN", "", d->host_name());
                        packet_gg_shutdown p;
                        p.header = HEADER_GG_SHUTDOWN;
                        p2p_manager::instance().send(&p, sizeof(packet_gg_shutdown));
                        metin2::log::info("Shutdown requested by admin host {}", d->host_name());
                        shutdown(10);
                    } else if (!stBuf.compare("SHUTDOWN_ONLY")) {
                        log_manager::instance().char_log(0, 0, 0, 2, "SHUTDOWN", "", d->host_name());
                        metin2::log::info("Local-only shutdown requested by admin host {}", d->host_name());
                        shutdown(10);
                    } else if (!stBuf.compare(0, 3, "DC ")) {
                        std::string msg = stBuf.substr(3, LOGIN_MAX_LEN);

                        metin2::log::debug("Admin disconnect of account {}", msg.c_str());

                        packet_gg_disconnect pgg;

                        pgg.header = HEADER_GG_DISCONNECT;
                        strlcpy(pgg.login, msg.c_str(), sizeof(pgg.login));

                        p2p_manager::instance().send(&pgg, sizeof(packet_gg_disconnect));

                        {
                            packet_dc p;
                            strlcpy(p.login, msg.c_str(), sizeof(p.login));
                            db_clientdesc->db_packet(HEADER_GD_DC, 0, &p, sizeof(p));
                        }
                    } else if (!stBuf.compare(0, 20, "CHECK_CLIENT_VERSION")) {
                        check_client_version();

                        uint8_t bHeader = HEADER_GG_CHECK_CLIENT_VERSION;
                        p2p_manager::instance().send(&bHeader, sizeof(uint8_t));
                        stResult = "OK";
                    } else if (!stBuf.compare(0, 6, "RELOAD")) {
                        if (stBuf.size() == 6) {
                            load_state_user_count();
                            db_clientdesc->db_packet(HEADER_GD_RELOAD_PROTO, 0, nullptr, 0);
                            db_manager::instance().load_db_string();
                        } else {
                            char c = stBuf[7];

                            switch (std::tolower(static_cast<unsigned char>(c))) {
                            case 'u':
                                load_state_user_count();
                                break;

                            case 'p':
                                db_clientdesc->db_packet(HEADER_GD_RELOAD_PROTO, 0, nullptr, 0);
                                break;

                            case 's':
                                db_manager::instance().load_db_string();
                                break;

                            case 'q':
                                quest::quest_manager::instance().reload();
                                break;

                            case 'f':
                                fishing::initialize();
                                break;

                            case 'a':
                                db_clientdesc->db_packet(HEADER_GD_RELOAD_ADMIN, 0, nullptr, 0);
                                metin2::log::info("Reloading gm list");
                                break;
                            }
                        }
                    } else if (!stBuf.compare(0, 6, "EVENT ")) {
                        std::istringstream is(stBuf);
                        std::string strEvent, strFlagName;
                        int32_t lValue;
                        is >> strEvent >> strFlagName >> lValue;

                        if (!is.fail()) {
                            metin2::log::info("Admin command: event flag {} set to {:d}", strFlagName.c_str(), lValue);
                            quest::quest_manager::instance().request_set_event_flag(strFlagName, lValue);
                            stResult = "EVENT FLAG CHANGE ";
                            stResult += strFlagName;
                        } else {
                            stResult = "EVENT FLAG FAIL";
                        }
                    } else if (!stBuf.compare(0, 11, "BLOCK_CHAT ")) {
                        std::istringstream is(stBuf);
                        std::string strBlockChat, strCharName;
                        int32_t lDuration;
                        is >> strBlockChat >> strCharName >> lDuration;

                        if (!is.fail()) {
                            metin2::log::info("Admin command: chat block for player {} ({:d})", strCharName.c_str(), lDuration);

                            do_block_chat(nullptr, const_cast<char *>(stBuf.c_str() + 11), 0, 0);

                            stResult = "BLOCK_CHAT ";
                            stResult += strCharName;
                        } else {
                            stResult = "BLOCK_CHAT FAIL";
                        }
                    } else if (!stBuf.compare(0, 12, "PRIV_EMPIRE ")) {
                        int empire, type, value, duration;
                        std::istringstream is(stBuf);
                        std::string strPrivEmpire;
                        is >> strPrivEmpire >> empire >> type >> value >> duration;

                        value = std::clamp(value, 0, 1000);
                        stResult = "PRIV_EMPIRE FAIL";

                        if (!is.fail()) {
                            if (empire < 0 || 3 < empire)
                                ;
                            else if (type < 1 || 4 < type)
                                ;
                            else if (value < 0)
                                ;
                            else if (duration < 0)
                                ;
                            else {
                                stResult = "PRIV_EMPIRE SUCCEED";

                                duration = duration * (60 * 60);

                                metin2::log::info("Admin command: empire {:d} privilege type {:d}, value {:d}, for {:d} seconds",
                                    empire, type, value, duration);
                                priv_manager::instance().request_give_empire_priv(empire, type, value, duration);
                            }
                        }
                    }
                }
            }

            metin2::log::debug("Admin command {} result {}", stBuf.c_str(), stResult.c_str());
            stResult += "\n";
            d->packet(stResult.c_str(), stResult.length());
            return (c_pSep - data_value) + 1;
        } else if (header_value == HEADER_CG_MARK_LOGIN) {
            if (!metin2::config::game().mark.server) {
                metin2::log::warn("Guild mark login from {} but this is not a mark server", d->host_name());
                d->set_phase(PHASE_CLOSE);
                return 0;
            }

            // Anyone may download marks; uploads need the key from the player's game login, which the
            // client lacks on its first download, so a missing key means download only.
            const packet_cg_mark_login *p = (const packet_cg_mark_login *)data_value;
            const bool bAuthenticated = desc_manager::instance().check_random_key(p->handle, p->random_key);

            metin2::log::debug("Guild mark login{}", bAuthenticated ? "" : " (download only)");
            d->set_mark_login(bAuthenticated ? p->handle : 0);
            d->set_phase(PHASE_LOGIN);
            return 0;
        } else if (header_value == HEADER_CG_STATE_CHECKER) {
            if (d->is_channel_status_requested()) {
                return 0;
            }
            d->set_channel_status_requested(true);
            db_clientdesc->db_packet(HEADER_GD_REQUEST_CHANNELSTATUS, d->handle(), nullptr, 0);
        } else if (header_value == HEADER_CG_PONG)
            pong(d);
        else if (header_value == HEADER_CG_HANDSHAKE)
            handshake(d, data_value);
        else if (header_value == HEADER_CG_KEY_AGREEMENT) {
            d->send_key_agreement_completed();
            // On its own: the client enables decryption per received chunk, so the encrypted packets that
            // follow must not share its segment.
            d->send_output_now();

            packet_key_agreement *p = (packet_key_agreement *)data_value;
            if (!d->is_cipher_prepared()) {
                metin2::log::warn("Key agreement from {} before the cipher was prepared, disconnecting", d->host_name());
                d->delayed_disconnect(5);
                return 0;
            }
            if (d->finish_handshake(p->agreed_length, p->data, p->data_length)) {
                if (metin2::config::is_auth()) {
                    d->set_phase(PHASE_AUTH);
                } else {
                    d->set_phase(PHASE_LOGIN);
                }
            } else {
                metin2::log::warn("Key agreement failed (agreed length {:d}, data length {:d}), closing",
                    p->agreed_length, p->data_length);
                d->set_phase(PHASE_CLOSE);
            }
        } else
            metin2::log::warn("Unhandled packet {:d} in handshake phase (host {})", header_value, d->host_name());

        return 0;
    }
}
