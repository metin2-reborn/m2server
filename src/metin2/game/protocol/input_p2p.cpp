#include <metin2/config/config.h>
#include <metin2/game/admin/cmd.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/affect.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/combat/skill.h>
#include <metin2/game/empire/castle.h>
#include <metin2/game/empire/empire_text_convert.h>
#include <metin2/game/events/xmas_event.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/guilds/guild_manager.h>
#include <metin2/game/items/unique_item.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/desc_manager.h>
#include <metin2/game/protocol/input.h>
#include <metin2/game/protocol/p2p.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/social/messenger_manager.h>

#include <metin2/log/log.h>
#include <metin2/proto/character_types.h>
#include <metin2/proto/chat_types.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace metin2::game::inline protocol {

    input_p2p::input_p2p() {
        bind_packet_table(p2p_packet_table());
    }

    void input_p2p::login(desc *d, const char *data) {
        p2p_manager::instance().login(d, (packet_gg_login *)data);
    }

    void input_p2p::logout(desc *d, const char *data) {
        packet_gg_logout *p = (packet_gg_logout *)data;
        p2p_manager::instance().logout(p->name);
    }

    auto input_p2p::relay(desc *d, const char *data, size_t bytes) -> int {
        packet_gg_relay *p = (packet_gg_relay *)data;

        if (bytes < sizeof(packet_gg_relay) + p->size)
            return -1;

        if (p->size < 0) {
            metin2::log::warn("Peer packet with negative length {:d}, closing", p->size);
            d->set_phase(PHASE_CLOSE);
            return -1;
        }

        metin2::log::debug("Peer relay to player {} ({:d} bytes)", p->name, p->size);

        character *pkChr = character_manager::instance().find_pc(p->name);

        const uint8_t *c_pbData = (const uint8_t *)(data + sizeof(packet_gg_relay));

        if (!pkChr)
            return p->size;

        if (*c_pbData == HEADER_GC_WHISPER) {
            if (pkChr->is_block_mode(BLOCK_WHISPER)) {
                return p->size;
            }

            char buf[1024];
            memcpy(buf, c_pbData, std::min<int>(p->size, sizeof(buf)));

            packet_gc_whisper *p2 = (packet_gc_whisper *)buf;
            uint8_t bToEmpire = (p2->type >> 4);
            p2->type = p2->type & 0x0F;
            if (p2->type == 0x0F) {
                p2->type = WHISPER_TYPE_SYSTEM;
            } else {
                if (!pkChr->is_equip_unique_group(UNIQUE_GROUP_RING_OF_LANGUAGE))
                    if (bToEmpire >= 1 && bToEmpire <= 3 && pkChr->empire() != bToEmpire) {
                        convert_empire_text(bToEmpire,
                            buf + sizeof(packet_gc_whisper),
                            p2->size - sizeof(packet_gc_whisper),
                            10 + 2 * pkChr->skill_power(SKILL_LANGUAGE1 + bToEmpire - 1));
                    }
            }

            pkChr->get_desc()->packet(buf, p->size);
        } else
            pkChr->get_desc()->packet(c_pbData, p->size);

        return (p->size);
    }

    auto input_p2p::notice(desc *d, const char *data, size_t bytes) -> int {
        packet_gg_notice *p = (packet_gg_notice *)data;

        if (bytes < sizeof(packet_gg_notice) + p->size)
            return -1;

        if (p->size < 0) {
            metin2::log::warn("Peer packet with negative length {:d}, closing", p->size);
            d->set_phase(PHASE_CLOSE);
            return -1;
        }

        char szBuf[256 + 1];
        strlcpy(szBuf, data + sizeof(packet_gg_notice), std::min<int>(p->size + 1, sizeof(szBuf)));
        send_notice(szBuf);
        return (p->size);
    }

    auto input_p2p::monarch_notice(desc *d, const char *data, size_t bytes) -> int {
        packet_gg_monarch_notice *p = (packet_gg_monarch_notice *)data;

        if (bytes < p->size + sizeof(packet_gg_monarch_notice))
            return -1;

        if (p->size < 0) {
            metin2::log::warn("Peer packet with negative length {:d}, closing", p->size);
            d->set_phase(PHASE_CLOSE);
            return -1;
        }

        char szBuf[256 + 1];
        strlcpy(szBuf, data + sizeof(packet_gg_monarch_notice), std::min<int>(p->size + 1, sizeof(szBuf)));
        send_monarch_notice(p->empire, szBuf);
        return (p->size);
    }

    auto input_p2p::monarch_transfer(desc *d, const char *data) -> int {
        packet_monarch_gg_transfer *p = (packet_monarch_gg_transfer *)data;
        character *pTargetChar = character_manager::instance().find_by_pid(p->target_pid);

        if (pTargetChar != nullptr) {
            unsigned int qIndex = quest::quest_manager::instance().quest_index_by_name("monarch_transfer");

            if (qIndex != 0) {
                pTargetChar->set_quest_flag("monarch_transfer.x", p->x);
                pTargetChar->set_quest_flag("monarch_transfer.y", p->y);
                quest::quest_manager::instance().letter(pTargetChar->player_id(), qIndex, 0);
            }
        }

        return 0;
    }

    auto input_p2p::guild(desc *d, const char *data, size_t bytes) -> int {
        packet_gg_guild *p = (packet_gg_guild *)data;
        bytes -= sizeof(packet_gg_guild);
        data += sizeof(packet_gg_guild);

        game::guilds::guild *g = guild_manager::instance().find_guild(p->guild);

        switch (p->sub_header) {
        case GUILD_SUBHEADER_GG_CHAT:
            {
                if (bytes < sizeof(packet_gg_guild_chat))
                    return -1;

                packet_gg_guild_chat *p = (packet_gg_guild_chat *)data;

                if (g)
                    g->p2p_chat(p->text);

                return sizeof(packet_gg_guild_chat);
            }

        case GUILD_SUBHEADER_GG_SET_MEMBER_COUNT_BONUS:
            {
                if (bytes < sizeof(int))
                    return -1;

                int iBonus = *((int *)data);
                game::guilds::guild *pGuild = guild_manager::instance().find_guild(p->guild);
                if (pGuild) {
                    pGuild->set_member_count_bonus(iBonus);
                }
                return sizeof(int);
            }
        default:
            metin2::log::error("Unknown guild subheader {:d} from peer", p->sub_header);
            break;
        }
        return 0;
    }

    void send_shout(const char *text, uint8_t empire) {
        const desc_manager::desc_set &c_ref_set = desc_manager::instance().client_set();
        for (desc *d : c_ref_set) {
            if (!d->get_character() || (d->get_character()->gm_level() == GM_PLAYER && d->empire() != empire))
                continue;

            d->get_character()->chat_packet(CHAT_TYPE_SHOUT, "%s", text);
        }
    }

    void input_p2p::shout(const char *data) {
        packet_gg_shout *p = (packet_gg_shout *)data;
        send_shout(p->text, p->empire);
    }

    void input_p2p::disconnect(const char *data) {
        packet_gg_disconnect *p = (packet_gg_disconnect *)data;

        desc *d = desc_manager::instance().find_by_login_name(p->login);

        if (!d)
            return;

        if (!d->get_character()) {
            d->set_phase(PHASE_CLOSE);
        } else
            d->disconnect_of_same_login();
    }

    void input_p2p::setup(desc *d, const char *data) {
        packet_gg_setup *p = (packet_gg_setup *)data;
        metin2::log::info("Peer setup: {}:{:d}", d->host_name(), p->port);
        d->set_p2p(d->host_name(), p->port);
    }

    void input_p2p::messenger_add(const char *data) {
        packet_gg_messenger *p = (packet_gg_messenger *)data;
        metin2::log::debug("Peer messenger: {} added {}", p->account, p->companion);
        messenger_manager::instance().apply_add_to_list(p->account, p->companion);
    }

    void input_p2p::messenger_remove(const char *data) {
        packet_gg_messenger *p = (packet_gg_messenger *)data;
        metin2::log::debug("Peer messenger: {} removed {}", p->account, p->companion);
        messenger_manager::instance().apply_remove_from_list(p->account, p->companion);
    }

    void input_p2p::find_position(desc *d, const char *data) {
        packet_gg_find_position *p = (packet_gg_find_position *)data;
        character *ch = character_manager::instance().find_by_pid(p->target_pid);
        if (ch && ch->map_index() < 10000) {
            packet_gg_warp_character pw;
            pw.header = HEADER_GG_WARP_CHARACTER;
            pw.pid = p->from_pid;
            pw.x = ch->x();
            pw.y = ch->y();
            d->packet(&pw, sizeof(pw));
        }
    }

    void input_p2p::warp_character(const char *data) {
        packet_gg_warp_character *p = (packet_gg_warp_character *)data;
        character *ch = character_manager::instance().find_by_pid(p->pid);
        if (ch) {
            ch->warp_set(p->x, p->y);
        }
    }

    void input_p2p::guild_war_zone_map_index(const char *data) {
        packet_gg_guild_war_map_index *p = (packet_gg_guild_war_map_index *)data;
        guild_manager &gm = guild_manager::instance();

        metin2::log::info("Guild war: guild {:d} vs {:d} on map {:d}", p->guild_id1, p->guild_id2, p->map_index);

        game::guilds::guild *g1 = gm.find_guild(p->guild_id1);
        game::guilds::guild *g2 = gm.find_guild(p->guild_id2);

        if (g1 && g2) {
            g1->set_guild_war_map_index(p->guild_id2, p->map_index);
            g2->set_guild_war_map_index(p->guild_id1, p->map_index);
        }
    }

    void input_p2p::transfer(const char *data) {
        packet_gg_transfer *p = (packet_gg_transfer *)data;

        character *ch = character_manager::instance().find_pc(p->name);

        if (ch)
            ch->warp_set(p->x, p->y);
    }

    void input_p2p::xmas_warp_santa(const char *data) {
        packet_gg_xmas_warp_santa *p = (packet_gg_xmas_warp_santa *)data;

        if (p->channel == metin2::config::server().channel && map_allow_find(p->map_index)) {
            int iNextSpawnDelay = 60;

            iNextSpawnDelay = 50 * 60;

            xmas::spawn_santa(p->map_index, iNextSpawnDelay);

            packet_gg_xmas_warp_santa_reply pack_reply;
            pack_reply.header = HEADER_GG_XMAS_WARP_SANTA_REPLY;
            pack_reply.channel = metin2::config::server().channel;
            p2p_manager::instance().send(&pack_reply, sizeof(pack_reply));
        }
    }

    void input_p2p::xmas_warp_santa_reply(const char *data) {
        packet_gg_xmas_warp_santa_reply *p = (packet_gg_xmas_warp_santa_reply *)data;

        if (p->channel == metin2::config::server().channel) {
            character_vector_interactor i;

            if (character_manager::instance().characters_by_race_num(xmas::MOB_SANTA_VNUM, i)) {
                character_vector_interactor::iterator it = i.begin();

                while (it != i.end()) {
                    character_manager::instance().destroy_character(*it++);
                }
            }
        }
    }

    void input_p2p::block_chat(const char *data) {
        packet_gg_block_chat *p = (packet_gg_block_chat *)data;

        character *ch = character_manager::instance().find_pc(p->name);

        if (ch) {
            metin2::log::info("Chat block applied to player {} for {:d} seconds", p->name, p->block_duration);
            ch->add_affect(AFFECT_BLOCK_CHAT, POINT_NONE, 0, AFF_NONE, p->block_duration, 0, true);
        } else {
            metin2::log::debug("Chat block for player {} ({:d} seconds) skipped: not on this server", p->name, p->block_duration);
        }
    }

    void input_p2p::iam_awake(desc *d, const char *data) {
        std::string hostNames;
        p2p_manager::instance().p2p_host_names(hostNames);
        metin2::log::info("Awake check from peer {}: {:d} peer connections: {}", d->host_name(), p2p_manager::instance().desc_count(), hostNames);
    }

    auto input_p2p::analyze(desc *d, uint8_t header, const char *data) -> int {
        if (metin2::config::test_server())
            metin2::log::debug("Peer packet header {:d}", header);

        int iExtraLen = 0;

        switch (header) {
        case HEADER_GG_SETUP:
            setup(d, data);
            break;

        case HEADER_GG_LOGIN:
            login(d, data);
            break;

        case HEADER_GG_LOGOUT:
            logout(d, data);
            break;

        case HEADER_GG_RELAY:
            if ((iExtraLen = relay(d, data, buffer_left_)) < 0)
                return -1;
            break;

        case HEADER_GG_NOTICE:
            if ((iExtraLen = notice(d, data, buffer_left_)) < 0)
                return -1;
            break;

        case HEADER_GG_SHUTDOWN:
            metin2::log::info("Shutdown requested by peer {}", d->host_name());
            shutdown(10);
            break;

        case HEADER_GG_GUILD:
            if ((iExtraLen = guild(d, data, buffer_left_)) < 0)
                return -1;
            break;

        case HEADER_GG_SHOUT:
            shout(data);
            break;

        case HEADER_GG_DISCONNECT:
            disconnect(data);
            break;

        case HEADER_GG_MESSENGER_ADD:
            messenger_add(data);
            break;

        case HEADER_GG_MESSENGER_REMOVE:
            messenger_remove(data);
            break;

        case HEADER_GG_FIND_POSITION:
            find_position(d, data);
            break;

        case HEADER_GG_WARP_CHARACTER:
            warp_character(data);
            break;

        case HEADER_GG_GUILD_WAR_ZONE_MAP_INDEX:
            guild_war_zone_map_index(data);
            break;

        case HEADER_GG_TRANSFER:
            transfer(data);
            break;

        case HEADER_GG_XMAS_WARP_SANTA:
            xmas_warp_santa(data);
            break;

        case HEADER_GG_XMAS_WARP_SANTA_REPLY:
            xmas_warp_santa_reply(data);
            break;

        case HEADER_GG_CHECK_CLIENT_VERSION:
            check_client_version();
            break;

        case HEADER_GG_BLOCK_CHAT:
            block_chat(data);
            break;

        case HEADER_GG_SIEGE:
            {
                packet_gg_siege *pSiege = (packet_gg_siege *)data;
                castle_siege(pSiege->empire, pSiege->tower_count);
            }
            break;

        case HEADER_GG_MONARCH_NOTICE:
            if ((iExtraLen = monarch_notice(d, data, buffer_left_)) < 0)
                return -1;
            break;

        case HEADER_GG_MONARCH_TRANSFER:
            monarch_transfer(d, data);
            break;

        case HEADER_GG_CHECK_AWAKENESS:
            iam_awake(d, data);
            break;
        }

        return (iExtraLen);
    }
}
