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
#include <metin2/proto/limits.h>
#include <metin2/proto/tables.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <format>
#include <iterator>
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

    input_processor::input_processor() : packet_table_(&client_packet_table()), buffer_left_(0) {
    }

    auto input_processor::process(desc *desc_value, const void *orig, int bytes, int &bytes_processed) -> bool {
        const char *data = (const char *)orig;

        uint8_t last_header = 0;
        int last_packet_len = 0;
        int packet_len;

        for (buffer_left_ = bytes; buffer_left_ > 0;) {
            auto header_value = (uint8_t)*(data);
            const packet_spec *spec = nullptr;

            if (header_value == 0)
                packet_len = 1;
            else if (!(spec = packet_table_->find(header_value))) {
                metin2::log::warn("Unknown packet header {:d} (last header {:d}, length {:d}, {:d} bytes left, host {}), closing",
                    header_value, last_header, last_packet_len, buffer_left_, desc_value->host_name());
                desc_value->set_phase(PHASE_CLOSE);
                return true;
            } else
                packet_len = spec->size;

            const bool is_sequence = spec && spec->sequence;

            if (buffer_left_ < packet_len)
                return true;

            if (header_value) {
                if (metin2::config::test_server() && header_value != HEADER_CG_MOVE)
                    metin2::log::debug("Packet header {:d}, {:d} bytes buffered", header_value, buffer_left_);

                const int buffer_left = buffer_left_;

                if (is_sequence)
                    --buffer_left_;

                int extra_packet_size = analyze(desc_value, header_value, data);
                buffer_left_ = buffer_left;

                if (extra_packet_size < 0)
                    return true;

                if (extra_packet_size > buffer_left_ - packet_len) {
                    metin2::log::warn("Packet {} (header {:d}) claims {:d} extra bytes, only {:d} left (host {}), closing",
                        spec->name, header_value, extra_packet_size, buffer_left_ - packet_len, desc_value->host_name());
                    desc_value->set_phase(PHASE_CLOSE);
                    return true;
                }

                packet_len += extra_packet_size;
            }

            if (header_value == HEADER_CG_PONG)
                metin2::log::debug("Ping reply: sequenced {:d}, sequence byte {:d}", is_sequence, *(uint8_t *)(data + packet_len - sizeof(uint8_t)));

            if (is_sequence) {
                uint8_t seq = desc_value->sequence();
                uint8_t seq_received = *(uint8_t *)(data + packet_len - sizeof(uint8_t));

                if (seq != seq_received) {
                    metin2::log::warn("Packet sequence mismatch on connection {}: expected {:#x}, got {:#x} (header {:d}), closing", static_cast<const void *>(static_cast<void *>(desc_value)), seq, seq_received, header_value);

                    character *ch = desc_value->get_character();

                    std::string text = std::format("SEQUENCE_LOG [{}]-------------\n", ch ? ch->name() : "UNKNOWN");

                    for (size_t i = 0; i < desc_value->seq_vector.size(); ++i) {
                        std::format_to(std::back_inserter(text), "\t[{:03} : 0x{:x}]\n",
                            desc_value->seq_vector[i].hdr,
                            desc_value->seq_vector[i].seq);
                    }

                    std::format_to(std::back_inserter(text), "\t[{:03} : 0x{:x}]\n", header_value, seq);
                    metin2::log::warn("{}", text);

                    desc_value->set_phase(PHASE_CLOSE);
                    return true;
                } else {
                    desc_value->push_seq(header_value, seq);
                    desc_value->set_next_sequence();
                }
            }

            data += packet_len;
            buffer_left_ -= packet_len;
            bytes_processed += packet_len;

            last_packet_len = packet_len;
            last_header = header_value;

            if (type() != desc_value->get_input_processor()->type())
                return false;
        }

        return true;
    }

    void input_processor::pong(desc *d) {
        d->set_pong(true);
    }

    void input_processor::handshake(desc *d, const char *data) {
        auto *p = (packet_cg_handshake *)data;

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

        auto *p = (packet_cg_client_version *)data;

        char timestamp[sizeof(p->timestamp)];
        char filename[sizeof(p->filename)];
        copy_packet_string(timestamp, p->timestamp);
        copy_packet_string(filename, p->filename);

        metin2::log::debug("Client version of player {}: {} ({})", ch->name(), timestamp, filename);
        ch->get_desc()->set_client_version(timestamp);
    }

    void login_failure(desc *d, const char *status_value) {
        if (!d)
            return;

        packet_gc_login_failure failure_packet;

        failure_packet.header = HEADER_GC_LOGIN_FAILURE;
        strlcpy(failure_packet.status, status_value, sizeof(failure_packet.status));

        d->packet(&failure_packet, sizeof(failure_packet));
    }

    input_handshake::input_handshake() {
        bind_packet_table(handshake_packet_table());
    }

    auto input_handshake::analyze(desc *d, uint8_t header_value, const char *data_value) -> int {
        if (header_value == HEADER_CG_ENTERGAME)
            return 0;

        if (header_value == HEADER_CG_TEXT) {
            if (is_empty_admin_page() || !is_admin_page(d->host_name())) {
                metin2::log::warn("Admin command from unauthorised host {} blocked", d->host_name());
                d->set_phase(PHASE_CLOSE);
                return -1;
            }

            ++data_value;

            const char *sep = (const char *)memchr(data_value, '\n', buffer_left_ - 1);

            if (!sep)
                return -1;

            if (sep > data_value && *(sep - 1) == '\r')
                --sep;

            std::string result_value;
            std::string buf;
            buf.assign(data_value, sep - data_value);

            metin2::log::info("Admin command from {}: {}", d->host_name(), buf);

            if (!buf.compare("IS_SERVER_UP")) {
                if (no_more_client)
                    result_value = "NO";
                else
                    result_value = "YES";
            } else if (!metin2::config::server().admin.password.empty() && buf == metin2::config::server().admin.password) {
                d->set_admin_mode();
                result_value = "UNKNOWN";
            } else if (!buf.compare("USER_COUNT")) {
                int total;
                int *pai_empire_user_count;
                int local;
                get_desc_manager().user_count(total, &pai_empire_user_count, local);
                result_value = std::format("{} {} {} {} {}", total, pai_empire_user_count[1], pai_empire_user_count[2], pai_empire_user_count[3], local);
            } else if (d->is_admin_mode() && !buf.compare("CHECK_P2P_CONNECTIONS")) {
                std::ostringstream oss(std::ostringstream::out);

                oss << "P2P CONNECTION NUMBER : " << get_p2p_manager().desc_count() << "\n";
                std::string host_names;
                get_p2p_manager().p2p_host_names(host_names);
                oss << host_names;
                result_value = oss.str();
                packet_gg_check_awakeness packet;
                packet.header = HEADER_GG_CHECK_AWAKENESS;

                get_p2p_manager().send(&packet, sizeof(packet));
            } else if (d->is_admin_mode() && !buf.compare(0, 15, "DELETE_AWARDID ")) {
                std::string msg = buf.substr(15, 26);

                packet_delete_award_id p;
                p.id = (uint32_t)(atoi(msg.c_str()));
                const std::string text = std::format("Sent to DB cache to delete ItemAward, id: {}", p.id);
                db_clientdesc->db_packet(HEADER_GD_DELETE_AWARDID, 0, &p, sizeof(p));
                result_value += text;
            } else {
                result_value = "UNKNOWN";

                if (d->is_admin_mode()) {
                    if (!buf.compare(0, 7, "NOTICE ")) {
                        std::string msg = buf.substr(7);
                        msg.resize(utf8_truncated_size(msg, 50));
                        get_log_manager().char_log(0, 0, 0, 1, "NOTICE", msg.c_str(), d->host_name());
                        broadcast_notice(msg.c_str());
                    } else if (!buf.compare("SHUTDOWN")) {
                        get_log_manager().char_log(0, 0, 0, 2, "SHUTDOWN", "", d->host_name());
                        packet_gg_shutdown p;
                        p.header = HEADER_GG_SHUTDOWN;
                        get_p2p_manager().send(&p, sizeof(packet_gg_shutdown));
                        metin2::log::info("Shutdown requested by admin host {}", d->host_name());
                        shutdown(10);
                    } else if (!buf.compare("SHUTDOWN_ONLY")) {
                        get_log_manager().char_log(0, 0, 0, 2, "SHUTDOWN", "", d->host_name());
                        metin2::log::info("Local-only shutdown requested by admin host {}", d->host_name());
                        shutdown(10);
                    } else if (!buf.compare(0, 3, "DC ")) {
                        std::string msg = buf.substr(3, LOGIN_MAX_LEN);

                        metin2::log::debug("Admin disconnect of account {}", msg);

                        packet_gg_disconnect pgg;

                        pgg.header = HEADER_GG_DISCONNECT;
                        strlcpy(pgg.login, msg.c_str(), sizeof(pgg.login));

                        get_p2p_manager().send(&pgg, sizeof(packet_gg_disconnect));

                        {
                            packet_dc p;
                            strlcpy(p.login, msg.c_str(), sizeof(p.login));
                            db_clientdesc->db_packet(HEADER_GD_DC, 0, &p, sizeof(p));
                        }
                    } else if (!buf.compare(0, 20, "CHECK_CLIENT_VERSION")) {
                        check_client_version();

                        uint8_t header = HEADER_GG_CHECK_CLIENT_VERSION;
                        get_p2p_manager().send(&header, sizeof(uint8_t));
                        result_value = "OK";
                    } else if (!buf.compare(0, 6, "RELOAD")) {
                        if (buf.size() == 6) {
                            load_state_user_count();
                            db_clientdesc->db_packet(HEADER_GD_RELOAD_PROTO, 0, nullptr, 0);
                            get_db_manager().load_db_string();
                        } else {
                            char c = buf[7];

                            switch (std::tolower(static_cast<unsigned char>(c))) {
                            case 'u':
                                load_state_user_count();
                                break;

                            case 'p':
                                db_clientdesc->db_packet(HEADER_GD_RELOAD_PROTO, 0, nullptr, 0);
                                break;

                            case 's':
                                get_db_manager().load_db_string();
                                break;

                            case 'q':
                                quest::get_quest_manager().reload();
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
                    } else if (!buf.compare(0, 6, "EVENT ")) {
                        std::istringstream is(buf);
                        std::string event_value, flag_name;
                        int32_t value_value;
                        is >> event_value >> flag_name >> value_value;

                        if (!is.fail()) {
                            metin2::log::info("Admin command: event flag {} set to {:d}", flag_name, value_value);
                            quest::get_quest_manager().request_set_event_flag(flag_name, value_value);
                            result_value = "EVENT FLAG CHANGE ";
                            result_value += flag_name;
                        } else {
                            result_value = "EVENT FLAG FAIL";
                        }
                    } else if (!buf.compare(0, 11, "BLOCK_CHAT ")) {
                        std::istringstream is(buf);
                        std::string block_chat, char_name;
                        int32_t duration_value;
                        is >> block_chat >> char_name >> duration_value;

                        if (!is.fail()) {
                            metin2::log::info("Admin command: chat block for player {} ({:d})", char_name, duration_value);

                            do_block_chat(nullptr, const_cast<char *>(buf.c_str() + 11), 0, 0);

                            result_value = "BLOCK_CHAT ";
                            result_value += char_name;
                        } else {
                            result_value = "BLOCK_CHAT FAIL";
                        }
                    } else if (!buf.compare(0, 12, "PRIV_EMPIRE ")) {
                        int empire, type, value, duration;
                        std::istringstream is(buf);
                        std::string priv_empire;
                        is >> priv_empire >> empire >> type >> value >> duration;

                        value = std::clamp(value, 0, 1000);
                        result_value = "PRIV_EMPIRE FAIL";

                        if (!is.fail()) {
                            if (empire < 0 || 3 < empire)
                                ;
                            else if (type < 1 || 4 < type)
                                ;
                            else if (duration < 0)
                                ;
                            else {
                                result_value = "PRIV_EMPIRE SUCCEED";

                                duration = duration * (60 * 60);

                                metin2::log::info("Admin command: empire {:d} privilege type {:d}, value {:d}, for {:d} seconds",
                                    empire, type, value, duration);
                                get_priv_manager().request_give_empire_priv(empire, type, value, duration);
                            }
                        }
                    }
                }
            }

            metin2::log::debug("Admin command {} result {}", buf, result_value);
            result_value += "\n";
            d->packet(result_value.c_str(), result_value.length());
            return (sep - data_value) + 1;
        } else if (header_value == HEADER_CG_MARK_LOGIN) {
            if (!metin2::config::game().mark.server) {
                metin2::log::warn("Guild mark login from {} but this is not a mark server", d->host_name());
                d->set_phase(PHASE_CLOSE);
                return 0;
            }

            // Anyone may download marks; uploads need the key from the player's game login, which the
            // client lacks on its first download, so a missing key means download only.
            const auto *p = (const packet_cg_mark_login *)data_value;
            const bool authenticated = get_desc_manager().check_random_key(p->handle, p->random_key);

            metin2::log::debug("Guild mark login{}", authenticated ? "" : " (download only)");
            d->set_mark_login(authenticated ? p->handle : 0);
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

            auto *p = (packet_key_agreement *)data_value;
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
