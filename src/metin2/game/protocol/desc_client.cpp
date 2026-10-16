#include <metin2/config/config.h>
#include <metin2/game/protocol/desc_client.h>

#include <metin2/game/app/config.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/guilds/guild_manager.h>
#include <metin2/game/parties/party.h>
#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/desc_manager.h>
#include <metin2/game/protocol/p2p.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/protocol/protocol.h>

#include <metin2/core/main.h>
#include <metin2/log/log.h>
#include <metin2/net/connection.h>
#include <metin2/net/io.h>
#include <metin2/proto/tables.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>

namespace metin2::game::inline protocol {

    client_desc *db_clientdesc = nullptr;
    client_desc *auth_master_desc = nullptr;
    client_desc *netmarble_db_desc = nullptr;

    static auto get_known_client_desc_name(client_desc *desc) -> const char * {
        if (desc == db_clientdesc) {
            return "db_clientdesc";
        } else if (desc == auth_master_desc) {
            return "g_pkAuthMasterDesc";
        } else if (desc == netmarble_db_desc) {
            return "g_NetmarbleDBDesc";
        }
        return "unknown";
    }

    client_desc::client_desc() {
        phase_when_succeed_ = 0;
        retry_when_closed_ = false;
        last_try_to_connect_time_ = 0;
        t_last_channel_status_update_time_ = 0;
    }

    client_desc::~client_desc() = default;

    void client_desc::destroy() {
        if (!connection_) {
            return;
        }

        p2p_manager::instance().unregister_connector(this);

        if (this == db_clientdesc) {
            party_manager::instance().delete_all_party();
            party_manager::instance().disable_pc_party();
            guild_manager::instance().stop_all_guild_war();
        }

        metin2::log::info("Closing connection to {}:{:d}", host_, port_);

        desc::destroy();
    }

    void client_desc::set_retry_when_closed(bool b) {
        retry_when_closed_ = b;
    }

    auto client_desc::connect(int phase_when_succeed) -> bool {
        if (phase_when_succeed != 0)
            phase_when_succeed_ = phase_when_succeed;

        if (get_global_time() - last_try_to_connect_time_ < 3)
            return false;

        last_try_to_connect_time_ = get_global_time();

        if (connection_)
            return false;

        metin2::log::info("Connecting to {}:{:d}", host_, port_);

        // Blocking (up to 10 s) so packets sent right after the connect stay in order.
        auto socket = net::connect(host_, port_, std::chrono::seconds(10));

        if (socket) {
            connection_ = net::connection::create(std::move(*socket), MAX_SERVER_INPUT, MAX_SERVER_OUTPUT);
            connection_->start(*this);
            metin2::log::info("Connected to {}:{:d} ({})", host_, port_, static_cast<const void *>(this));
            set_phase(phase_when_succeed_);
            return true;
        } else {
            set_phase(PHASE_CLIENT_CONNECTING);
            return false;
        }
    }

    void client_desc::setup(const char *host, uint16_t port) {
        host_ = host;
        port_ = port;
    }

    void client_desc::set_phase(int phase) {
        switch (phase) {
        case PHASE_CLIENT_CONNECTING:
            metin2::log::debug("Connection to {}:{:d} waiting to reconnect", host_, port_);
            input_processor_ = nullptr;
            break;

        case PHASE_DBCLIENT:
            {
                metin2::log::debug("Db connection established, sending setup");

                if (!metin2::config::is_auth()) {
                    static bool bSentBoot = false;

                    if (!bSentBoot) {
                        bSentBoot = true;
                        packet_gd_boot p;
                        p.item_id_range[0] = 0;
                        p.item_id_range[1] = 0;
                        memcpy(p.ip, public_ip, 16);
                        db_packet(HEADER_GD_BOOT, 0, &p, sizeof(p));
                    }
                }

                temp_buffer buf;

                packet_gd_setup p;

                memset(&p, 0, sizeof(p));
                strlcpy(p.public_ip, public_ip, sizeof(p.public_ip));

                if (!metin2::config::is_auth()) {
                    p.channel = metin2::config::server().channel;
                    p.listen_port = metin2::config::server().port;
                    p.p2p_port = metin2::config::server().p2p_port;
                    p.auth_server = false;
                    map_allow_copy(p.maps, MAX_MAPS_PER_SERVER);

                    const desc_manager::desc_set &c_set = desc_manager::instance().client_set();
                    desc_manager::desc_set::const_iterator it;

                    for (it = c_set.begin(); it != c_set.end(); ++it) {
                        desc *d = *it;

                        if (d->get_account_table().id != 0)
                            ++p.login_count;
                    }

                    buf.write(&p, sizeof(p));

                    if (p.login_count) {
                        packet_login_on_setup pck;

                        for (it = c_set.begin(); it != c_set.end(); ++it) {
                            desc *d = *it;

                            account_table &r = d->get_account_table();

                            if (r.id != 0) {
                                pck.id = r.id;
                                strlcpy(pck.login, r.login, sizeof(pck.login));
                                strlcpy(pck.social_id, r.social_id, sizeof(pck.social_id));
                                strlcpy(pck.host, d->host_name(), sizeof(pck.host));
                                pck.login_key = d->get_login_key();

                                buf.write(&pck, sizeof(packet_login_on_setup));
                            }
                        }
                    }

                    metin2::log::info("Db setup: {:d} users logged in, {:d} bytes", p.login_count, buf.size());

                    party_manager::instance().enable_pc_party();
                } else {
                    p.auth_server = true;
                    buf.write(&p, sizeof(p));
                }

                db_packet(HEADER_GD_SETUP, 0, buf.read_peek(), buf.size());
                input_processor_ = &input_db_;
            }
            break;

        case PHASE_P2P:
            metin2::log::debug("Peer connection entered p2p phase");
            input_processor_ = &input_p2p_;
            break;

        case PHASE_CLOSE:
            input_processor_ = nullptr;
            break;
        }

        phase_ = phase;
    }

    void client_desc::db_packet_header(uint8_t header, uint32_t handle, uint32_t size) {
        connection_->send(encode_byte(header), sizeof(uint8_t));
        connection_->send(encode_4bytes(handle), sizeof(uint32_t));
        connection_->send(encode_4bytes(size), sizeof(uint32_t));
    }

    void client_desc::db_packet(uint8_t header, uint32_t handle, const void *data, uint32_t size) {
        if (!connection_) {
            metin2::log::warn("Db packet to {} dropped: not connected",
                get_known_client_desc_name(this));
            return;
        }
        metin2::log::debug("Db packet out: header {:d}, handle {:d}, size {:d}, {:d} bytes pending", header, handle, size, connection_->pending_output());
        db_packet_header(header, handle, size);

        if (data)
            connection_->send(data, size);
    }

    void client_desc::packet(const void *data, int size) {
        if (!connection_) {
            metin2::log::warn("Packet to {} dropped: not connected",
                get_known_client_desc_name(this));
            return;
        }
        connection_->send(data, size);
    }

    auto client_desc::is_retry_when_closed() -> bool {
        return (0 == thecore_is_shutdowned() && retry_when_closed_);
    }

    void client_desc::update(uint32_t t) {
        if (!metin2::config::is_auth()) {
            update_channel_status(t, false);
        }
    }

    void client_desc::update_channel_status(uint32_t t, bool force) {
        enum {
            CHANNELSTATUS_UPDATE_PERIOD = 5 * 60 * 1000,
        };
        if (force || t_last_channel_status_update_time_ + CHANNELSTATUS_UPDATE_PERIOD < t) {
            int iTotal;
            int *paiEmpireUserCount;
            int iLocal;
            desc_manager::instance().user_count(iTotal, &paiEmpireUserCount, iLocal);

            channel_status channelStatus;
            channelStatus.port = metin2::config::server().port;

            if (no_more_client)
                channelStatus.status = 0;
            else
                channelStatus.status = iTotal > full_user_count ? 3 : iTotal > busy_user_count ? 2
                                                                                               : 1;

            db_packet(HEADER_GD_UPDATE_CHANNELSTATUS, 0, &channelStatus, sizeof(channelStatus));
            t_last_channel_status_update_time_ = t;
        }
    }

    void client_desc::reset() {
        std::string host = host_;
        uint16_t port = port_;

        destroy();
        initialize();

        host_ = host;
        port_ = port;
    }
}
