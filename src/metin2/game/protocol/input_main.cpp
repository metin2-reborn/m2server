#include <metin2/config/config.h>
#include <metin2/game/admin/cmd.h>
#include <metin2/game/admin/gm.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/characters/affect.h>
#include <metin2/game/characters/ani.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/characters/motion.h>
#include <metin2/game/combat/skill.h>
#include <metin2/game/empire/empire_text_convert.h>
#include <metin2/game/events/ox_event.h>
#include <metin2/game/guilds/building.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/guilds/guild_manager.h>
#include <metin2/game/items/belt_inventory_helper.h>
#include <metin2/game/items/dragon_soul.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/items/unique_item.h>
#include <metin2/game/parties/party.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/protocol/desc_manager.h>
#include <metin2/game/protocol/input.h>
#include <metin2/game/protocol/p2p.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/quest.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/quest/questpc.h>
#include <metin2/game/social/messenger_manager.h>
#include <metin2/game/text/banword.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/text/locale_service.h>
#include <metin2/game/text/spam.h>
#include <metin2/game/trade/exchange.h>
#include <metin2/game/trade/safebox.h>
#include <metin2/game/trade/shop.h>
#include <metin2/game/trade/shop_manager.h>

#include <metin2/core/main.h>
#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/character_types.h>
#include <metin2/proto/chat_types.h>
#include <metin2/proto/guild_types.h>
#include <metin2/proto/inventory.h>
#include <metin2/proto/item_types.h>
#include <metin2/proto/limits.h>
#include <metin2/proto/log_types.h>
#include <metin2/proto/shop_types.h>
#include <metin2/proto/tables.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <sys/time.h>

namespace metin2::game::inline protocol {

    static auto deposit_limit() -> int {
        return (1000 * 10000);
    }

    void send_block_chat_info(character *ch, int sec) {
        if (sec <= 0) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("채팅 금지 상태입니다."));
            return;
        }

        int32_t hour = sec / 3600;
        sec -= hour * 3600;

        int32_t min = (sec / 60);
        sec -= min * 60;

        char buf[128 + 1];

        if (hour > 0 && min > 0)
            snprintf(buf, sizeof(buf), lc_text("%d 시간 %d 분 %d 초 동안 채팅금지 상태입니다"), hour, min, sec);
        else if (hour > 0 && min == 0)
            snprintf(buf, sizeof(buf), lc_text("%d 시간 %d 초 동안 채팅금지 상태입니다"), hour, sec);
        else if (hour == 0 && min > 0)
            snprintf(buf, sizeof(buf), lc_text("%d 분 %d 초 동안 채팅금지 상태입니다"), min, sec);
        else
            snprintf(buf, sizeof(buf), lc_text("%d 초 동안 채팅금지 상태입니다"), sec);

        ch->chat_packet(CHAT_TYPE_INFO, buf);
    }

    EVENTINFO(spam_event_info) {
        char host[MAX_HOST_LENGTH + 1];

        spam_event_info() {
            ::memset(host, 0, MAX_HOST_LENGTH + 1);
        }
    };

    using spam_score_of_ip_t = std::unordered_map<std::string, std::pair<unsigned int, event_ptr>>;
    spam_score_of_ip_t spam_score_of_ip;

    EVENTFUNC(block_chat_by_ip_event) {
        spam_event_info *info = dynamic_cast<spam_event_info *>(event->info.get());

        if (info == nullptr) {
            metin2::log::error("Chat block event has no info");
            return 0;
        }

        const char *host = info->host;

        spam_score_of_ip_t::iterator it = spam_score_of_ip.find(host);

        if (it != spam_score_of_ip.end()) {
            it->second.first = 0;
            it->second.second = nullptr;
        }

        return 0;
    }

    auto spam_block_check(character *ch, const char *const buf, const size_t buflen) -> bool {

        if (ch->level() < metin2::config::game().spam.block_max_level) {
            spam_score_of_ip_t::iterator it = spam_score_of_ip.find(ch->get_desc()->host_name());

            if (it == spam_score_of_ip.end()) {
                spam_score_of_ip.insert(std::make_pair(ch->get_desc()->host_name(), std::make_pair(0, (event_ptr) nullptr)));
                it = spam_score_of_ip.find(ch->get_desc()->host_name());
            }

            if (it->second.second) {
                send_block_chat_info(ch, event_time(it->second.second) / metin2::config::server().passes_per_sec);
                return true;
            }

            unsigned int score;
            const char *word = spam_manager::instance().spam_score(buf, buflen, score);

            it->second.first += score;

            if (word)
                metin2::log::info("Spam: player {} said \"{}\", score {:d}, total {:d}, matched word {}", ch->name(), buf, score, it->second.first, word);

            if (it->second.first >= metin2::config::game().spam.block_score) {
                spam_event_info *info = alloc_event_info<spam_event_info>();
                strlcpy(info->host, ch->get_desc()->host_name(), sizeof(info->host));

                it->second.second = event_create(block_chat_by_ip_event, info, passes_per_sec(metin2::config::game().spam.block_duration));
                metin2::log::info("Spam: chat blocked for host {} for {:d} seconds", info->host, metin2::config::game().spam.block_duration);

                log_manager::instance().char_log(ch, 0, "SPAM", word);

                send_block_chat_info(ch, event_time(it->second.second) / metin2::config::server().passes_per_sec);

                return true;
            }
        }

        return false;
    }

    enum {
        TEXT_TAG_PLAIN,
        TEXT_TAG_TAG,
        TEXT_TAG_COLOR,
        TEXT_TAG_HYPERLINK_START,
        TEXT_TAG_HYPERLINK_END,
    };

    auto get_text_tag(const char *src, int max_len, int &tag_len, std::string &extra_info) -> int {
        tag_len = 1;

        if (max_len < 2 || *src != '|')
            return TEXT_TAG_PLAIN;

        const char *cur = ++src;

        if (*cur == '|') {
            tag_len = 2;
            return TEXT_TAG_TAG;
        } else if (*cur == 'c') {
            tag_len = 2;
            return TEXT_TAG_COLOR;
        } else if (*cur == 'H') {
            tag_len = 2;
            return TEXT_TAG_HYPERLINK_START;
        } else if (*cur == 'h') {
            tag_len = 2;
            return TEXT_TAG_HYPERLINK_END;
        }

        return TEXT_TAG_PLAIN;
    }

    void get_text_tag_info(const char *src, int src_len, int &hyperlinks, bool &colored) {
        colored = false;
        hyperlinks = 0;

        int len;
        std::string extraInfo;

        for (int i = 0; i < src_len;) {
            int tag = get_text_tag(&src[i], src_len - i, len, extraInfo);

            if (tag == TEXT_TAG_HYPERLINK_START)
                ++hyperlinks;

            if (tag == TEXT_TAG_COLOR)
                colored = true;

            i += len;
        }
    }

    auto process_text_tag(character *ch, const char *text, size_t len) -> int {
        int hyperlinks;
        bool colored;

        get_text_tag_info(text, len, hyperlinks, colored);

        if (colored == true && hyperlinks == 0)
            return 4;

        if (ch->get_exchange()) {
            if (hyperlinks == 0)
                return 0;
            else
                return 3;
        }

        int nPrismCount = ch->count_specify_item(ITEM_PRISM);

        if (nPrismCount < hyperlinks)
            return 1;

        if (!ch->my_shop()) {
            ch->remove_specify_item(ITEM_PRISM, hyperlinks);
            return 0;
        } else {
            int sellingNumber = ch->my_shop()->number_by_vnum(ITEM_PRISM);
            if (nPrismCount - sellingNumber < hyperlinks) {
                return 2;
            } else {
                ch->remove_specify_item(ITEM_PRISM, hyperlinks);
                return 0;
            }
        }

        return 4;
    }

    auto input_main::whisper(character *ch, const char *data, size_t bytes) -> int {
        const packet_cg_whisper *pinfo = reinterpret_cast<const packet_cg_whisper *>(data);

        if (bytes < pinfo->size)
            return -1;

        int iExtraLen = pinfo->size - sizeof(packet_cg_whisper);

        if (iExtraLen < 0) {
            metin2::log::warn("Invalid packet length (extra {:d}, size {:d}, buffer {:d})", iExtraLen, pinfo->size, bytes);
            ch->get_desc()->set_phase(PHASE_CLOSE);
            return -1;
        }

        if (ch->last_pm_pulse() < thecore_pulse())
            ch->clear_pm_counter();

        if (ch->pm_counter() > 3 && ch->last_pm_pulse() > thecore_pulse()) {
            ch->get_desc()->set_phase(PHASE_CLOSE);
            return -1;
        }

        if (ch->find_affect(AFFECT_BLOCK_CHAT)) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("채팅 금지 상태입니다."));
            return (iExtraLen);
        }

        char szNameTo[CHARACTER_NAME_MAX_LEN + 1];
        copy_packet_string(szNameTo, sizeof(szNameTo), pinfo->name_to, sizeof(pinfo->name_to));

        character *pkChr = character_manager::instance().find_pc(szNameTo);

        if (pkChr == ch)
            return (iExtraLen);

        ch->increase_pm_counter();
        ch->set_last_pm_pulse();

        desc *pkDesc = nullptr;

        uint8_t bOpponentEmpire = 0;

        if (metin2::config::test_server()) {
            if (!pkChr)
                metin2::log::debug("Whisper to {} ({}) from {}", "nobody", szNameTo, ch->name());
            else
                metin2::log::debug("Whisper to {} ({}) from {}", pkChr->name(), szNameTo, ch->name());
        }

        if (ch->is_block_mode(BLOCK_WHISPER)) {
            if (ch->get_desc()) {
                packet_gc_whisper pack;
                pack.header = HEADER_GC_WHISPER;
                pack.type = WHISPER_TYPE_SENDER_BLOCKED;
                pack.size = sizeof(packet_gc_whisper);
                strlcpy(pack.name_from, szNameTo, sizeof(pack.name_from));
                ch->get_desc()->packet(&pack, sizeof(pack));
            }
            return iExtraLen;
        }

        if (!pkChr) {
            cci *pkCCI = p2p_manager::instance().find(szNameTo);

            if (pkCCI) {
                pkDesc = pkCCI->desc;
                pkDesc->set_relay(szNameTo);
                bOpponentEmpire = pkCCI->empire;

                if (metin2::config::test_server())
                    metin2::log::debug("Whisper to {} from {} relayed to channel {:d}, map {:d}", "remote player", ch->name(), pkCCI->channel, pkCCI->map_index);
            }
        } else {
            pkDesc = pkChr->get_desc();
            bOpponentEmpire = pkChr->empire();
        }

        if (!pkDesc) {
            if (ch->get_desc()) {
                packet_gc_whisper pack;

                pack.header = HEADER_GC_WHISPER;
                pack.type = WHISPER_TYPE_NOT_EXIST;
                pack.size = sizeof(packet_gc_whisper);
                strlcpy(pack.name_from, szNameTo, sizeof(pack.name_from));
                ch->get_desc()->packet(&pack, sizeof(packet_gc_whisper));
                metin2::log::debug("Whisper: target {} not online", szNameTo);
            }
        } else {
            if (ch->is_block_mode(BLOCK_WHISPER)) {
                if (ch->get_desc()) {
                    packet_gc_whisper pack;
                    pack.header = HEADER_GC_WHISPER;
                    pack.type = WHISPER_TYPE_SENDER_BLOCKED;
                    pack.size = sizeof(packet_gc_whisper);
                    strlcpy(pack.name_from, szNameTo, sizeof(pack.name_from));
                    ch->get_desc()->packet(&pack, sizeof(pack));
                }
            } else if (pkChr && pkChr->is_block_mode(BLOCK_WHISPER)) {
                if (ch->get_desc()) {
                    packet_gc_whisper pack;
                    pack.header = HEADER_GC_WHISPER;
                    pack.type = WHISPER_TYPE_TARGET_BLOCKED;
                    pack.size = sizeof(packet_gc_whisper);
                    strlcpy(pack.name_from, szNameTo, sizeof(pack.name_from));
                    ch->get_desc()->packet(&pack, sizeof(pack));
                }
            } else {
                uint8_t bType = WHISPER_TYPE_NORMAL;

                char buf[CHAT_MAX_LEN + 1];
                copy_packet_string(buf, sizeof(buf), data + sizeof(packet_cg_whisper), iExtraLen);
                const size_t buflen = strlen(buf);

                if (true == spam_block_check(ch, buf, buflen)) {
                    if (!pkChr) {
                        cci *pkCCI = p2p_manager::instance().find(szNameTo);

                        if (pkCCI) {
                            pkDesc->set_relay("");
                        }
                    }
                    return iExtraLen;
                }

                banword_manager::instance().convert_string(buf, buflen);

                if (metin2::config::game().empire_whisper)
                    if (!ch->is_equip_unique_group(UNIQUE_GROUP_RING_OF_LANGUAGE))
                        if (!(pkChr && pkChr->is_equip_unique_group(UNIQUE_GROUP_RING_OF_LANGUAGE)))
                            if (bOpponentEmpire != ch->empire() && ch->empire() && bOpponentEmpire && ch->gm_level() == GM_PLAYER && gm_get_level(szNameTo) == GM_PLAYER) {
                                if (!pkChr) {
                                    bType = ch->empire() << 4;
                                } else {
                                    convert_empire_text(ch->empire(), buf, buflen, 10 + 2 * pkChr->skill_power(SKILL_LANGUAGE1 + ch->empire() - 1));
                                }
                            }

                int processReturn = process_text_tag(ch, buf, buflen);
                if (0 != processReturn) {
                    if (ch->get_desc()) {
                        item_table *pTable = item_manager::instance().table(ITEM_PRISM);

                        if (pTable) {
                            char buf[128];
                            int len;
                            if (3 == processReturn)
                                len = snprintf(buf, sizeof(buf), lc_text("다른 거래중(창고,교환,상점)에는 개인상점을 사용할 수 없습니다."), pTable->locale_name);
                            else
                                len = snprintf(buf, sizeof(buf), lc_text("%s이 필요합니다."), pTable->locale_name);

                            if (len < 0 || len >= (int)sizeof(buf))
                                len = sizeof(buf) - 1;

                            ++len;

                            packet_gc_whisper pack;

                            pack.header = HEADER_GC_WHISPER;
                            pack.type = WHISPER_TYPE_ERROR;
                            pack.size = sizeof(packet_gc_whisper) + len;
                            strlcpy(pack.name_from, szNameTo, sizeof(pack.name_from));

                            ch->get_desc()->buffered_packet(&pack, sizeof(pack));
                            ch->get_desc()->packet(buf, len);

                            metin2::log::debug("Whisper: not enough {} for player {}", pTable->locale_name, ch->name());
                        }
                    }

                    pkDesc->set_relay("");
                    return (iExtraLen);
                }

                if (ch->is_gm())
                    bType = (bType & 0xF0) | WHISPER_TYPE_GM;

                if (buflen > 0) {
                    packet_gc_whisper pack;

                    pack.header = HEADER_GC_WHISPER;
                    pack.size = sizeof(packet_gc_whisper) + buflen;
                    pack.type = bType;
                    strlcpy(pack.name_from, ch->name(), sizeof(pack.name_from));

                    temp_buffer tmpbuf;

                    tmpbuf.write(&pack, sizeof(pack));
                    tmpbuf.write(buf, buflen);

                    pkDesc->packet(tmpbuf.read_peek(), tmpbuf.size());
                }
            }
        }
        if (pkDesc)
            pkDesc->set_relay("");

        return (iExtraLen);
    }

    auto input_main::chat(character *ch, const char *data, size_t bytes) -> int {
        const packet_cg_chat *pinfo = reinterpret_cast<const packet_cg_chat *>(data);

        if (bytes < pinfo->size)
            return -1;

        const int iExtraLen = pinfo->size - sizeof(packet_cg_chat);

        if (iExtraLen < 0) {
            metin2::log::warn("Invalid packet length (extra {:d}, size {:d}, buffer {:d})", iExtraLen, pinfo->size, bytes);
            ch->get_desc()->set_phase(PHASE_CLOSE);
            return -1;
        }

        char buf[CHAT_MAX_LEN - (CHARACTER_NAME_MAX_LEN + 3) + 1];
        copy_packet_string(buf, sizeof(buf), data + sizeof(packet_cg_chat), iExtraLen);
        const size_t buflen = strlen(buf);

        if (buflen > 1 && *buf == '/') {
            interpret_command(ch, buf + 1, buflen - 1);
            return iExtraLen;
        }

        if (ch->increase_chat_counter() >= 10) {
            if (ch->chat_counter() == 10) {
                metin2::log::warn("Chat flood from player {}, disconnecting", ch->name());
                ch->get_desc()->delayed_disconnect(5);
            }

            return iExtraLen;
        }

        const affect *pAffect = ch->find_affect(AFFECT_BLOCK_CHAT);

        if (pAffect != nullptr) {
            send_block_chat_info(ch, pAffect->duration);
            return iExtraLen;
        }

        if (true == spam_block_check(ch, buf, buflen)) {
            return iExtraLen;
        }

        std::string chat = std::format("{} : {}", ch->name(), buf);

        if (CHAT_TYPE_SHOUT == pinfo->type) {
            log_manager::instance().shout_log(metin2::config::server().channel, ch->empire(), chat.c_str());
        }

        banword_manager::instance().convert_string(chat.data() + (chat.size() - buflen), buflen);

        if (chat.size() > CHAT_MAX_LEN)
            chat.resize(CHAT_MAX_LEN);

        const int len = chat.size();

        int processReturn = process_text_tag(ch, chat.c_str(), len);
        if (0 != processReturn) {
            const item_table *pTable = item_manager::instance().table(ITEM_PRISM);

            if (nullptr != pTable) {
                if (3 == processReturn)
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("다른 거래중(창고,교환,상점)에는 개인상점을 사용할 수 없습니다."), pTable->locale_name);
                else
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("%s이 필요합니다."), pTable->locale_name);
            }

            return iExtraLen;
        }

        if (pinfo->type == CHAT_TYPE_SHOUT) {
            const int SHOUT_LIMIT_LEVEL = 15;

            if (ch->level() < SHOUT_LIMIT_LEVEL) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("외치기는 레벨 %d 이상만 사용 가능 합니다."), SHOUT_LIMIT_LEVEL);
                return (iExtraLen);
            }

            if (thecore_pulse() - (int)ch->last_shout_pulse() < metin2::config::server().passes_per_sec * 15)
                return (iExtraLen);

            ch->set_last_shout_pulse(thecore_pulse());

            packet_gg_shout p;

            p.header = HEADER_GG_SHOUT;
            p.empire = ch->empire();
            utf8_strlcpy(p.text, chat.c_str());

            p2p_manager::instance().send(&p, sizeof(packet_gg_shout));

            send_shout(chat.c_str(), ch->empire());

            return (iExtraLen);
        }

        packet_gc_chat pack_chat;

        pack_chat.header = HEADER_GC_CHAT;
        pack_chat.size = sizeof(packet_gc_chat) + len;
        pack_chat.type = pinfo->type;
        pack_chat.id = ch->get_vid();

        switch (pinfo->type) {
        case CHAT_TYPE_TALKING:
            {
                const desc_manager::desc_set &c_ref_set = desc_manager::instance().client_set();

                const uint8_t empire = (ch->gm_level() > GM_PLAYER ||
                                           ch->is_equip_unique_group(UNIQUE_GROUP_RING_OF_LANGUAGE))
                                           ? 0
                                           : ch->empire();
                const int map_index = ch->map_index();
                const int namelen = strlen(ch->name());
                char converted_msg[CHAT_MAX_LEN + 1] = {};

                for (desc *d : c_ref_set) {
                    if (!d->get_character())
                        continue;

                    if (d->get_character()->map_index() != map_index)
                        continue;

                    d->buffered_packet(&pack_chat, sizeof(packet_gc_chat));

                    if (d->empire() == empire ||
                        empire == 0 ||
                        d->get_character()->gm_level() > GM_PLAYER ||
                        d->get_character()->is_equip_unique_group(UNIQUE_GROUP_RING_OF_LANGUAGE)) {
                        d->packet(chat.data(), len);
                    } else {
                        utf8_strlcpy(converted_msg, chat.c_str());
                        size_t converted_len = strlen(converted_msg);

                        convert_empire_text(empire, converted_msg + namelen, converted_len - namelen, 10 + 2 * d->get_character()->skill_power(SKILL_LANGUAGE1 + empire - 1));
                        d->packet(converted_msg, len);
                    }
                }
            }
            break;

        case CHAT_TYPE_PARTY:
            {
                if (!ch->get_party())
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("파티 중이 아닙니다."));
                else {
                    temp_buffer tbuf;

                    tbuf.write(&pack_chat, sizeof(pack_chat));
                    tbuf.write(chat.data(), len);

                    const void *buf = tbuf.read_peek();
                    const int buf_len = tbuf.size();

                    ch->get_party()->for_each_online_member([&](character *c) {
                        if (!c->get_desc())
                            return;

                        c->get_desc()->packet(buf, buf_len);
                    });
                }
            }
            break;

        case CHAT_TYPE_GUILD:
            {
                if (!ch->get_guild())
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("길드에 가입하지 않았습니다."));
                else
                    ch->get_guild()->chat(chat.c_str());
            }
            break;

        default:
            metin2::log::warn("Unknown chat type {:d}", pinfo->type);
            break;
        }

        return (iExtraLen);
    }

    void input_main::item_use(character *ch, const char *data) {
        ch->use_item(((struct packet_cg_item_use *)data)->cell);
    }

    void input_main::item_to_item(character *ch, const char *pc_data) {
        packet_cg_item_use_to_item *p = (packet_cg_item_use_to_item *)pc_data;
        if (ch)
            ch->use_item(p->cell, p->target_cell);
    }

    void input_main::item_drop(character *ch, const char *data) {
        struct packet_cg_item_drop *pinfo = (struct packet_cg_item_drop *)data;

        if (!ch)
            return;

        if (pinfo->gold > 0)
            ch->drop_gold(pinfo->gold);
        else
            ch->drop_item(pinfo->cell);
    }

    void input_main::item_drop2(character *ch, const char *data) {

        packet_cg_item_drop2 *pinfo = (packet_cg_item_drop2 *)data;

        if (!ch)
            return;
        if (pinfo->gold > 0)
            ch->drop_gold(pinfo->gold);
        else
            ch->drop_item(pinfo->cell, pinfo->count);
    }

    void input_main::item_move(character *ch, const char *data) {
        struct packet_cg_item_move *pinfo = (struct packet_cg_item_move *)data;

        if (ch)
            ch->move_item(pinfo->cell, pinfo->cell_to, pinfo->count);
    }

    void input_main::item_pickup(character *ch, const char *data) {
        struct packet_cg_item_pickup *pinfo = (struct packet_cg_item_pickup *)data;
        if (ch)
            ch->pickup_item(pinfo->vid);
    }

    void input_main::quickslot_add(character *ch, const char *data) {
        struct packet_cg_quickslot_add *pinfo = (struct packet_cg_quickslot_add *)data;
        ch->set_quickslot(pinfo->pos, pinfo->slot);
    }

    void input_main::quickslot_delete(character *ch, const char *data) {
        struct packet_cg_quickslot_del *pinfo = (struct packet_cg_quickslot_del *)data;
        ch->del_quickslot(pinfo->pos);
    }

    void input_main::quickslot_swap(character *ch, const char *data) {
        struct packet_cg_quickslot_swap *pinfo = (struct packet_cg_quickslot_swap *)data;
        ch->swap_quickslot(pinfo->pos, pinfo->change_pos);
    }

    auto input_main::messenger(character *ch, const char *data, size_t bytes) -> int {
        packet_cg_messenger *p = (packet_cg_messenger *)data;

        if (bytes < sizeof(packet_cg_messenger))
            return -1;

        data += sizeof(packet_cg_messenger);
        bytes -= sizeof(packet_cg_messenger);

        switch (p->subheader) {
        case MESSENGER_SUBHEADER_CG_ADD_BY_VID:
            {
                if (bytes < sizeof(packet_cg_messenger_add_by_vid))
                    return -1;

                packet_cg_messenger_add_by_vid *p2 = (packet_cg_messenger_add_by_vid *)data;
                character *ch_companion = character_manager::instance().find(p2->vid);

                if (!ch_companion)
                    return sizeof(packet_cg_messenger_add_by_vid);

                if (ch->is_observer_mode())
                    return sizeof(packet_cg_messenger_add_by_vid);

                if (ch_companion->is_block_mode(BLOCK_MESSENGER_INVITE)) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("상대방이 메신져 추가 거부 상태입니다."));
                    return sizeof(packet_cg_messenger_add_by_vid);
                }

                desc *d = ch_companion->get_desc();

                if (!d)
                    return sizeof(packet_cg_messenger_add_by_vid);

                if (ch->gm_level() == GM_PLAYER && ch_companion->gm_level() != GM_PLAYER) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("<메신져> 운영자는 메신져에 추가할 수 없습니다."));
                    return sizeof(packet_cg_messenger_add_by_vid);
                }

                if (ch->get_desc() == d)
                    return sizeof(packet_cg_messenger_add_by_vid);

                messenger_manager::instance().request_to_add(ch, ch_companion);
            }
            return sizeof(packet_cg_messenger_add_by_vid);

        case MESSENGER_SUBHEADER_CG_ADD_BY_NAME:
            {
                if (bytes < CHARACTER_NAME_MAX_LEN)
                    return -1;

                const std::string name(data, strnlen(data, CHARACTER_NAME_MAX_LEN));

                if (ch->gm_level() == GM_PLAYER && gm_get_level(name.c_str()) != GM_PLAYER) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("<메신져> 운영자는 메신져에 추가할 수 없습니다."));
                    return CHARACTER_NAME_MAX_LEN;
                }

                character *tch = character_manager::instance().find_pc(name.c_str());

                if (!tch)
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("%s 님은 접속되 있지 않습니다."), name.c_str());
                else {
                    if (tch == ch)
                        return CHARACTER_NAME_MAX_LEN;

                    if (tch->is_block_mode(BLOCK_MESSENGER_INVITE) == true) {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("상대방이 메신져 추가 거부 상태입니다."));
                    } else {
                        messenger_manager::instance().request_to_add(ch, tch);
                    }
                }
            }
            return CHARACTER_NAME_MAX_LEN;

        case MESSENGER_SUBHEADER_CG_REMOVE:
            {
                if (bytes < CHARACTER_NAME_MAX_LEN)
                    return -1;

                const std::string char_name(data, strnlen(data, CHARACTER_NAME_MAX_LEN));
                messenger_manager::instance().remove_from_list(ch->name(), char_name);
            }
            return CHARACTER_NAME_MAX_LEN;

        default:
            metin2::log::warn("Unknown messenger subheader {:d} from player {}", p->subheader, ch->name());
            break;
        }

        return 0;
    }

    auto input_main::shop(character *ch, const char *data, size_t bytes) -> int {
        packet_cg_shop *p = (packet_cg_shop *)data;

        if (bytes < sizeof(packet_cg_shop))
            return -1;

        if (metin2::config::test_server())
            metin2::log::debug("Shop packet subheader {:d}", p->subheader);

        const char *c_pData = data + sizeof(packet_cg_shop);
        bytes -= sizeof(packet_cg_shop);

        switch (p->subheader) {
        case SHOP_SUBHEADER_CG_END:
            metin2::log::debug("Shop: player {} stopped shopping", ch->name());
            shop_manager::instance().stop_shopping(ch);
            return 0;

        case SHOP_SUBHEADER_CG_BUY:
            {
                if (bytes < sizeof(uint8_t) + sizeof(uint8_t))
                    return -1;

                uint8_t bPos = *(c_pData + 1);
                metin2::log::debug("Shop: player {} buys slot {:d}", ch->name(), bPos);
                shop_manager::instance().buy(ch, bPos);
                return (sizeof(uint8_t) + sizeof(uint8_t));
            }

        case SHOP_SUBHEADER_CG_SELL:
            {
                if (bytes < sizeof(uint8_t))
                    return -1;

                uint8_t pos = *c_pData;

                metin2::log::debug("Shop: player {} sells inventory slot {:d}", ch->name(), pos);
                shop_manager::instance().sell(ch, pos);
                return sizeof(uint8_t);
            }

        case SHOP_SUBHEADER_CG_SELL2:
            {
                if (bytes < sizeof(uint8_t) + sizeof(uint8_t))
                    return -1;

                uint8_t pos = *(c_pData++);
                uint8_t count = *(c_pData);

                metin2::log::debug("Shop: player {} sells {:d} from inventory slot {:d}", ch->name(), count, pos);
                shop_manager::instance().sell(ch, pos, count);
                return sizeof(uint8_t) + sizeof(uint8_t);
            }

        default:
            metin2::log::warn("Unknown shop subheader {:d} from player {}", p->subheader, ch->name());
            break;
        }

        return 0;
    }

    void input_main::on_click(character *ch, const char *data) {
        struct packet_cg_on_click *pinfo = (struct packet_cg_on_click *)data;
        character *victim;

        if ((victim = character_manager::instance().find(pinfo->vid)))
            victim->on_click(ch);
        else if (metin2::config::test_server()) {
            metin2::log::debug("Player {} clicked unknown vid {:d}", ch->name(), pinfo->vid);
        }
    }

    void input_main::exchange(character *ch, const char *data) {
        struct packet_cg_exchange *pinfo = (struct packet_cg_exchange *)data;
        character *to_ch = nullptr;

        if (!ch->can_handle_item())
            return;

        int iPulse = thecore_pulse();

        if ((to_ch = character_manager::instance().find(pinfo->arg1))) {
            if (iPulse - to_ch->safebox_load_time() < passes_per_sec(portal_limit_time)) {
                to_ch->chat_packet(CHAT_TYPE_INFO, lc_text("거래 후 %d초 이내에 창고를 열수 없습니다."), portal_limit_time);
                return;
            }

            if (true == to_ch->is_dead()) {
                return;
            }
        }

        metin2::log::debug("Exchange packet subheader {:d}", pinfo->sub_header);

        if (iPulse - ch->safebox_load_time() < passes_per_sec(portal_limit_time)) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("거래 후 %d초 이내에 창고를 열수 없습니다."), portal_limit_time);
            return;
        }

        switch (pinfo->sub_header) {
        case EXCHANGE_SUBHEADER_CG_START:
            if (!ch->get_exchange()) {
                if ((to_ch = character_manager::instance().find(pinfo->arg1))) {
                    if (iPulse - ch->safebox_load_time() < passes_per_sec(portal_limit_time)) {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("창고를 연후 %d초 이내에는 거래를 할수 없습니다."), portal_limit_time);

                        if (metin2::config::test_server())
                            ch->chat_packet(CHAT_TYPE_INFO, "[TestOnly][Safebox]Pulse %d LoadTime %d PASS %d", iPulse, ch->safebox_load_time(), passes_per_sec(portal_limit_time));
                        return;
                    }

                    if (iPulse - to_ch->safebox_load_time() < passes_per_sec(portal_limit_time)) {
                        to_ch->chat_packet(CHAT_TYPE_INFO, lc_text("창고를 연후 %d초 이내에는 거래를 할수 없습니다."), portal_limit_time);

                        if (metin2::config::test_server())
                            to_ch->chat_packet(CHAT_TYPE_INFO, "[TestOnly][Safebox]Pulse %d LoadTime %d PASS %d", iPulse, to_ch->safebox_load_time(), passes_per_sec(portal_limit_time));
                        return;
                    }

                    if (ch->gold() >= GOLD_MAX) {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("액수가 20억 냥을 초과하여 거래를 할수가 없습니다.."));

                        metin2::log::warn("Exchange refused: gold {:d} at maximum for player (pid {:d}, name {})", ch->gold(), ch->player_id(), ch->name());
                        return;
                    }

                    if (to_ch->is_pc()) {
                        if (quest::quest_manager::instance().give_item_to_pc(ch->player_id(), to_ch)) {
                            metin2::log::debug("Exchange between {} and {} handled by a quest", ch->name(), to_ch->name());
                            return;
                        }
                    }

                    if (ch->my_shop() || ch->is_open_safebox() || ch->shop_owner() || ch->is_cube_open()) {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("다른 거래중일경우 개인상점을 열수가 없습니다."));
                        return;
                    }

                    ch->exchange_start(to_ch);
                }
            }
            break;

        case EXCHANGE_SUBHEADER_CG_ITEM_ADD:
            if (ch->get_exchange()) {
                if (ch->get_exchange()->company()->accept_status() != true)
                    ch->get_exchange()->add_item(pinfo->pos, pinfo->arg2);
            }
            break;

        case EXCHANGE_SUBHEADER_CG_ITEM_DEL:
            if (ch->get_exchange()) {
                if (ch->get_exchange()->company()->accept_status() != true)
                    ch->get_exchange()->remove_item(pinfo->arg1);
            }
            break;

        case EXCHANGE_SUBHEADER_CG_ELK_ADD:
            if (ch->get_exchange()) {
                const int64_t nTotalGold = static_cast<int64_t>(ch->get_exchange()->company()->owner()->gold()) + static_cast<int64_t>(pinfo->arg1);

                if (GOLD_MAX <= nTotalGold) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("상대방의 총금액이 20억 냥을 초과하여 거래를 할수가 없습니다.."));

                    metin2::log::warn("Exchange gold refused: partner would exceed maximum gold ({:d}, pid {:d}, name {})",
                        ch->get_exchange()->company()->owner()->gold(),
                        ch->get_exchange()->company()->owner()->player_id(),
                        ch->get_exchange()->company()->owner()->name());

                    return;
                }

                if (ch->get_exchange()->company()->accept_status() != true)
                    ch->get_exchange()->add_gold(pinfo->arg1);
            }
            break;

        case EXCHANGE_SUBHEADER_CG_ACCEPT:
            if (ch->get_exchange()) {
                ch->get_exchange()->accept(true);
            }

            break;

        case EXCHANGE_SUBHEADER_CG_CANCEL:
            if (ch->get_exchange())
                ch->get_exchange()->cancel();
            break;
        }
    }

    void input_main::position(character *ch, const char *data) {
        struct packet_cg_position *pinfo = (struct packet_cg_position *)data;

        switch (pinfo->position) {
        case POSITION_GENERAL:
            ch->standup();
            break;

        case POSITION_SITTING_CHAIR:
            ch->sitdown(0);
            break;

        case POSITION_SITTING_GROUND:
            ch->sitdown(1);
            break;
        }
    }

    static const int combo_sequence_by_skill_level[3][8] =
        {
            {14, 15, 16, 17, 0,  0,  0, 0},
            {14, 15, 16, 18, 20, 0,  0, 0},
            {14, 15, 16, 18, 19, 17, 0, 0},
    };

    constexpr int COMBO_HACK_ALLOWABLE_MS = 100;
    auto clac_valid_combo_interval(character *ch, uint8_t arg) -> uint32_t {
        int nInterval = 300;
        float fAdjustNum = 1.5f;

        if (!ch) {
            metin2::log::error("Combo interval check without a character");
            return nInterval;
        }

        if (arg == 13) {
            float normalAttackDuration = motion_manager::instance().normal_attack_duration(ch->race_num());
            nInterval = (int)(normalAttackDuration / (((float)ch->point(POINT_ATT_SPEED) / 100.f) * 900.f) + fAdjustNum);
        } else if (arg == 14) {
            nInterval = (int)(ani_combo_speed(ch, 1) / ((ch->point(POINT_ATT_SPEED) / 100.f) + fAdjustNum));
        } else if (arg > 14 && arg < 22) {
            nInterval = (int)(ani_combo_speed(ch, arg - 13) / ((ch->point(POINT_ATT_SPEED) / 100.f) + fAdjustNum));
        } else {
            metin2::log::warn("Invalid combo argument {:d} from player {}", arg, ch->name());
        }

        return nInterval;
    }

    auto check_combo_hack(character *ch, uint8_t arg, uint32_t time, bool check_speed_hack) -> bool {
        if (ch->is_stun() || ch->is_dead())
            return false;
        int ComboInterval = time - ch->last_combo_time();
        int HackScalar = 0;

        if (arg == 14) {
            if (check_speed_hack && ComboInterval > 0 && ComboInterval < ch->valid_combo_interval() - COMBO_HACK_ALLOWABLE_MS) {
            }

            ch->set_combo_sequence(1);
            ch->set_valid_combo_interval(clac_valid_combo_interval(ch, arg));
            ch->set_last_combo_time(time);
        } else if (arg > 14 && arg < 22) {
            int idx = std::min<int>(2, ch->combo_index());

            if (ch->combo_sequence() > 5) {
                HackScalar = 1;
                ch->set_valid_combo_interval(300);
                metin2::log::warn("Possible combo hack: player {} combo sequence {:d} too long", ch->name(), ch->combo_sequence());
            } else if (arg == 21 &&
                       idx == 2 &&
                       ch->combo_sequence() == 5 &&
                       ch->job() == JOB_ASSASSIN &&
                       ch->wear(WEAR_WEAPON) &&
                       ch->wear(WEAR_WEAPON)->sub_type() == WEAPON_DAGGER)
                ch->set_valid_combo_interval(300);
            else if (combo_sequence_by_skill_level[idx][ch->combo_sequence()] != arg) {
                HackScalar = 1;
                ch->set_valid_combo_interval(300);

                metin2::log::warn("Possible combo hack: player {} sent combo {:d}, expected {:d} (combo index {:d}, sequence {:d})",
                    ch->name(),
                    arg,
                    combo_sequence_by_skill_level[idx][ch->combo_sequence()],
                    idx,
                    ch->combo_sequence());
            } else {
                if (check_speed_hack && ComboInterval < ch->valid_combo_interval() - COMBO_HACK_ALLOWABLE_MS) {
                    HackScalar = 1 + (ch->valid_combo_interval() - ComboInterval) / 100;

                    metin2::log::warn("Possible combo hack: player {} combo {:d} interval {:d} ms, minimum {:d} ms (attack speed {:d}, riding {})",
                        ch->name(),
                        arg,
                        ComboInterval,
                        ch->valid_combo_interval(),
                        ch->point(POINT_ATT_SPEED),
                        ch->is_riding() ? "yes" : "no");
                }

                if (ch->is_riding())
                    ch->set_combo_sequence(ch->combo_sequence() == 1 ? 2 : 1);
                else
                    ch->set_combo_sequence(ch->combo_sequence() + 1);

                ch->set_valid_combo_interval(clac_valid_combo_interval(ch, arg));
                ch->set_last_combo_time(time);
            }
        } else if (arg == 13) {
            if (check_speed_hack && ComboInterval > 0 && ComboInterval < ch->valid_combo_interval() - COMBO_HACK_ALLOWABLE_MS) {
            }

            if (ch->race_num() >= MAIN_RACE_MAX_NUM) {

                ch->set_valid_combo_interval(clac_valid_combo_interval(ch, arg));
                ch->set_last_combo_time(time);
            } else {
            }
        } else {
            if (ch->get_desc()->delayed_disconnect(number(2, 9))) {
                log_manager::instance().hack_log("Hacker", ch);
                metin2::log::warn("Attack hack: player {} (combo argument {:d}), disconnecting", ch->name(), arg);
            }

            HackScalar = 10;
            ch->set_valid_combo_interval(300);
        }

        if (HackScalar) {
            if (get_dword_time() - ch->last_mount_time() > 1500)
                ch->increase_combo_hack_count(1 + HackScalar);

            ch->skip_combo_attack_by_time(ch->valid_combo_interval());
        }

        return HackScalar;
    }

    void input_main::move(character *ch, const char *data) {
        if (!ch->can_move())
            return;

        struct packet_cg_move *pinfo = (struct packet_cg_move *)data;

        if (pinfo->func >= FUNC_MAX_NUM && !(pinfo->func & 0x80)) {
            metin2::log::warn("Invalid move type {:d} from player {}", pinfo->func, ch->name());
            return;
        }

        {
            const float fDist = distance_sqrt((ch->x() - pinfo->x) / 100, (ch->y() - pinfo->y) / 100);

            if (((false == ch->is_riding() && fDist > 25) || fDist > 40) && OXEVENT_MAP_INDEX != ch->map_index()) {

                metin2::log::warn("Player {} tried to move too far ({:.1f} m, riding {:d})", ch->name(), fDist, ch->is_riding());

                ch->show(ch->map_index(), ch->x(), ch->y(), ch->z());
                ch->stop();
                return;
            }

            uint32_t dwCurTime = get_dword_time();
            bool CheckSpeedHack = (false == ch->get_desc()->is_handshaking() && dwCurTime - ch->get_desc()->client_time() > 7000);

            if (CheckSpeedHack) {
                int iDelta = (int)(pinfo->time - ch->get_desc()->client_time());
                int iServerDelta = (int)(dwCurTime - ch->get_desc()->client_time());

                iDelta = (int)(dwCurTime - pinfo->time);

                if (iDelta >= 30000) {
                    metin2::log::warn("Possible speed hack: player {} client timer lags by {:d} ms", ch->name(), iDelta);
                } else if (iDelta < -(iServerDelta / 50)) {
                    metin2::log::warn("Possible speed hack: player {} client timer ahead (delta {:d}, server delta {:d})", ch->name(), iDelta, iServerDelta);
                }
            }

            if (pinfo->func == FUNC_COMBO && metin2::config::game().anti_cheat.check_multihack) {
                check_combo_hack(ch, pinfo->arg, pinfo->time, CheckSpeedHack);
            }
        }

        if (pinfo->func == FUNC_MOVE) {
            if (ch->limit_point(POINT_MOV_SPEED) == 0)
                return;

            ch->set_rotation(pinfo->rot * 5);
            ch->reset_stop_time();

            ch->go_to(pinfo->x, pinfo->y);
        } else {
            if (pinfo->func == FUNC_ATTACK || pinfo->func == FUNC_COMBO)
                ch->on_move(true);
            else if (pinfo->func & FUNC_SKILL) {
                const int MASK_SKILL_MOTION = 0x7F;
                unsigned int motion = pinfo->func & MASK_SKILL_MOTION;

                if (!ch->is_usable_skill_motion(motion)) {
                    const char *name = ch->name();
                    unsigned int job = ch->job();
                    unsigned int group = ch->skill_group();

                    const std::string hack_text = std::format("SKILL_HACK: name={}, job={}, group={}, motion={}", name, job, group, motion);
                    log_manager::instance().hack_log(hack_text.c_str(), ch->get_desc()->get_account_table().login, ch->name(), ch->get_desc()->host_name());
                    metin2::log::warn("{}", hack_text);

                    if (metin2::config::test_server()) {
                        ch->get_desc()->delayed_disconnect(number(2, 8));
                        ch->chat_packet(CHAT_TYPE_INFO, hack_text.c_str());
                    } else {
                        ch->get_desc()->delayed_disconnect(number(150, 500));
                    }
                }

                ch->on_move();
            }

            ch->set_rotation(pinfo->rot * 5);
            ch->reset_stop_time();

            ch->move(pinfo->x, pinfo->y);
            ch->stop();
            ch->stop_stamina_consume();
        }

        packet_gc_move pack;

        pack.header = HEADER_GC_MOVE;
        pack.func = pinfo->func;
        pack.arg = pinfo->arg;
        pack.rot = pinfo->rot;
        pack.vid = ch->get_vid();
        pack.x = pinfo->x;
        pack.y = pinfo->y;
        pack.time = pinfo->time;
        pack.duration = (pinfo->func == FUNC_MOVE) ? ch->current_move_duration() : 0;

        ch->packet_around(&pack, sizeof(packet_gc_move), ch);
    }

    void input_main::attack(character *ch, const uint8_t header, const char *data) {
        if (nullptr == ch)
            return;

        struct type_identifier {
            uint8_t header;
            uint8_t type;
        };

        const struct type_identifier *const type = reinterpret_cast<const struct type_identifier *>(data);

        if (type->type > 0) {
            if (false == ch->can_use_skill(type->type)) {
                return;
            }

            switch (type->type) {
            case SKILL_GEOMPUNG:
            case SKILL_SANGONG:
            case SKILL_YEONSA:
            case SKILL_KWANKYEOK:
            case SKILL_HWAJO:
            case SKILL_GIGUNG:
            case SKILL_PABEOB:
            case SKILL_MARYUNG:
            case SKILL_TUSOK:
            case SKILL_MAHWAN:
            case SKILL_BIPABU:
            case SKILL_NOEJEON:
            case SKILL_CHAIN:
            case SKILL_HORSE_WILDATTACK_RANGE:
                if (HEADER_CG_SHOOT != type->header) {
                    if (metin2::config::test_server())
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("Attack :name[%s] Vnum[%d] can't use skill by attack(warning)"), type->type);
                    return;
                }
                break;
            }
        }

        switch (header) {
        case HEADER_CG_ATTACK:
            {
                if (nullptr == ch->get_desc())
                    return;

                const packet_cg_attack *const packMelee = reinterpret_cast<const packet_cg_attack *>(data);

                character *victim = character_manager::instance().find(packMelee->vid);

                if (nullptr == victim || ch == victim)
                    return;

                switch (victim->get_char_type()) {
                case CHAR_TYPE_NPC:
                case CHAR_TYPE_WARP:
                case CHAR_TYPE_GOTO:
                    return;
                }

                if (packMelee->type > 0) {
                    if (false == ch->check_skill_hit_count(packMelee->type, victim->get_vid())) {
                        return;
                    }
                }

                ch->attack(victim, packMelee->type);
            }
            break;

        case HEADER_CG_SHOOT:
            {
                const packet_cg_shoot *const packShoot = reinterpret_cast<const packet_cg_shoot *>(data);

                ch->shoot(packShoot->type);
            }
            break;
        }
    }

    auto input_main::sync_position(character *ch, const char *pc_data, size_t bytes) -> int {
        const packet_cg_sync_position *pinfo = reinterpret_cast<const packet_cg_sync_position *>(pc_data);

        if (bytes < pinfo->size)
            return -1;

        int iExtraLen = pinfo->size - sizeof(packet_cg_sync_position);

        if (iExtraLen < 0) {
            metin2::log::warn("Invalid packet length (extra {:d}, size {:d}, buffer {:d})", iExtraLen, pinfo->size, bytes);
            ch->get_desc()->set_phase(PHASE_CLOSE);
            return -1;
        }

        if (0 != (iExtraLen % sizeof(packet_cg_sync_position_element))) {
            metin2::log::warn("Sync position packet size {:d} is not a multiple of the element size (player {})", pinfo->size, ch->name());
            return iExtraLen;
        }

        int iCount = iExtraLen / sizeof(packet_cg_sync_position_element);

        if (iCount <= 0)
            return iExtraLen;

        static const int nCountLimit = 16;

        if (iCount > nCountLimit) {
            metin2::log::warn("Too many sync positions ({:d}) from player {}", iCount, ch->name());
            iCount = nCountLimit;
        }

        std::vector<packet_cg_sync_position_element> synced;
        synced.reserve(iCount);

        const packet_cg_sync_position_element *e =
            reinterpret_cast<const packet_cg_sync_position_element *>(pc_data + sizeof(packet_cg_sync_position));

        timeval tvCurTime;
        gettimeofday(&tvCurTime, nullptr);

        for (int i = 0; i < iCount; ++i, ++e) {
            character *victim = character_manager::instance().find(e->vid);

            if (!victim)
                continue;

            switch (victim->get_char_type()) {
            case CHAR_TYPE_NPC:
            case CHAR_TYPE_WARP:
            case CHAR_TYPE_GOTO:
                continue;
            }

            if (!victim->set_sync_owner(ch))
                continue;

            const float fDistWithSyncOwner = distance_sqrt((victim->x() - ch->x()) / 100, (victim->y() - ch->y()) / 100);
            static const float fLimitDistWithSyncOwner = 2500.f + 1000.f;
            if (fDistWithSyncOwner > fLimitDistWithSyncOwner) {
                if (ch->sync_hack_count() < metin2::config::game().anti_cheat.sync_hack_limit) {
                    ch->set_sync_hack_count(ch->sync_hack_count() + 1);
                    continue;
                } else {
                    log_manager::instance().hack_log("SYNC_POSITION_HACK", ch);

                    metin2::log::warn("Sync position hack: {:.1f} m from sync owner to {} by player {} (player {:d}x{:d}, target {:d}x{:d}, sync {:d}x{:d}), disconnecting",
                        fDistWithSyncOwner, victim->name(), ch->name(), ch->x(), ch->y(), victim->x(), victim->y(),
                        e->x, e->y);

                    ch->get_desc()->set_phase(PHASE_CLOSE);

                    return -1;
                }
            }

            const float fDist = distance_sqrt((victim->x() - e->x) / 100, (victim->y() - e->y) / 100);
            static const int32_t g_lValidSyncInterval = 50 * 1000;
            const timeval &tvLastSyncTime = victim->last_sync_time();
            const int64_t elapsed_usec = (static_cast<int64_t>(tvCurTime.tv_sec) - tvLastSyncTime.tv_sec) * 1000000 + (tvCurTime.tv_usec - tvLastSyncTime.tv_usec);

            if (elapsed_usec < g_lValidSyncInterval) {
                if (ch->sync_hack_count() < metin2::config::game().anti_cheat.sync_hack_limit) {
                    ch->set_sync_hack_count(ch->sync_hack_count() + 1);
                    continue;
                } else {
                    log_manager::instance().hack_log("SYNC_POSITION_HACK", ch);

                    metin2::log::warn("Sync position hack: interval {:d} ms too short for {} by player {} (target {:d}x{:d}, sync {:d}x{:d}), disconnecting",
                        static_cast<long>(elapsed_usec / 1000), victim->name(), ch->name(), victim->x(), victim->y(),
                        e->x, e->y);

                    ch->get_desc()->set_phase(PHASE_CLOSE);

                    return -1;
                }
            } else if (fDist > 25.0f) {
                log_manager::instance().hack_log("SYNC_POSITION_HACK", ch);

                metin2::log::warn("Sync position hack: moved {:.1f} m for {} by player {} (player {:d}x{:d}, target {:d}x{:d}, sync {:d}x{:d}), disconnecting",
                    fDist, victim->name(), ch->name(), ch->x(), ch->y(), victim->x(), victim->y(),
                    e->x, e->y);

                ch->get_desc()->set_phase(PHASE_CLOSE);

                return -1;
            } else {
                victim->set_last_sync_time(tvCurTime);
                victim->sync(e->x, e->y);
                synced.push_back(*e);
            }
        }

        if (!synced.empty()) {
            packet_gc_sync_position header;
            header.header = HEADER_GC_SYNC_POSITION;
            header.size = static_cast<uint16_t>(sizeof(header) + synced.size() * sizeof(packet_cg_sync_position_element));

            temp_buffer buf;
            buf.write(&header, sizeof(header));
            buf.write(synced.data(), synced.size() * sizeof(packet_cg_sync_position_element));
            ch->packet_around(buf.read_peek(), buf.size(), ch);
        }

        return iExtraLen;
    }

    void input_main::fly_target(character *ch, const char *pc_data, uint8_t header) {
        packet_cg_fly_targeting *p = (packet_cg_fly_targeting *)pc_data;
        ch->fly_target(p->target_vid, p->x, p->y, header);
    }

    void input_main::use_skill(character *ch, const char *pc_data) {
        packet_cg_use_skill *p = (packet_cg_use_skill *)pc_data;
        ch->use_skill(p->vnum, character_manager::instance().find(p->vid));
    }

    void input_main::script_button(character *ch, const void *data) {
        packet_cg_script_button *p = (packet_cg_script_button *)data;
        metin2::log::debug("Quest: pid {:d} pressed button {:d}", ch->player_id(), p->idx);

        quest::pc *pc = quest::quest_manager::instance().pc_force(ch->player_id());
        if (pc && pc->is_confirm_wait()) {
            quest::quest_manager::instance().confirm(ch->player_id(), quest::CONFIRM_TIMEOUT);
        } else if (p->idx & 0x80000000) {
            quest::quest_manager::instance().quest_info(ch->player_id(), p->idx & 0x7fffffff);
        } else {
            quest::quest_manager::instance().quest_button(ch->player_id(), p->idx);
        }
    }

    void input_main::script_answer(character *ch, const void *data) {
        packet_cg_script_answer *p = (packet_cg_script_answer *)data;
        metin2::log::debug("Quest: pid {:d} answered {:d}", ch->player_id(), p->answer);

        if (p->answer > 250) {
            quest::quest_manager::instance().resume(ch->player_id());
        } else {
            quest::quest_manager::instance().select(ch->player_id(), p->answer);
        }
    }

    void input_main::script_select_item(character *ch, const void *data) {
        packet_cg_script_select_item *p = (packet_cg_script_select_item *)data;
        metin2::log::debug("Quest: pid {:d} selected item {:d}", ch->player_id(), p->selection);
        quest::quest_manager::instance().select_item(ch->player_id(), p->selection);
    }

    void input_main::quest_input_string(character *ch, const void *data) {
        packet_cg_quest_input_string *p = (packet_cg_quest_input_string *)data;

        const std::string msg(p->msg, strnlen(p->msg, sizeof(p->msg) - 1));
        metin2::log::debug("Quest: pid {:d} entered \"{}\"", ch->player_id(), msg);

        quest::quest_manager::instance().input(ch->player_id(), msg.c_str());
    }

    void input_main::quest_confirm(character *ch, const void *data) {
        packet_cg_quest_confirm *p = (packet_cg_quest_confirm *)data;
        character *ch_wait = character_manager::instance().find_by_pid(p->request_pid);
        if (p->answer)
            p->answer = quest::CONFIRM_YES;
        metin2::log::debug("Quest: confirm from {} to pid {:d} ({}), answer {:d}", ch->name(), p->request_pid, (ch_wait) ? ch_wait->name() : "", p->answer);
        if (ch_wait) {
            quest::quest_manager::instance().confirm(ch_wait->player_id(), (quest::quest_confirm_type)p->answer, ch->player_id());
        }
    }

    void input_main::target(character *ch, const char *pc_data) {
        packet_cg_target *p = (packet_cg_target *)pc_data;

        building::object *pkObj = building::manager::instance().find_object_by_vid(p->vid);

        if (pkObj) {
            packet_gc_target pckTarget;
            pckTarget.header = HEADER_GC_TARGET;
            pckTarget.vid = p->vid;
            ch->get_desc()->packet(&pckTarget, sizeof(packet_gc_target));
        } else
            ch->set_target(character_manager::instance().find(p->vid));
    }

    void input_main::warp(character *ch, const char *pc_data) {
        ch->warp_end();
    }

    void input_main::safebox_checkin(character *ch, const char *data) {
        if (quest::quest_manager::instance().pc_force(ch->player_id())->is_running() == true)
            return;

        packet_cg_safebox_checkin *p = (packet_cg_safebox_checkin *)data;

        if (!ch->can_handle_item())
            return;

        safebox *pkSafebox = ch->get_safebox();
        item *pkItem = ch->get_item(p->item_pos);

        if (!pkSafebox || !pkItem)
            return;

        if (pkItem->is_exchanging())
            return;

        if (pkItem->type() == ITEM_BELT && pkItem->is_equipped())
            return;

        if (pkItem->cell() >= INVENTORY_MAX_NUM && is_set(pkItem->flag(), ITEM_FLAG_IRREMOVABLE)) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("<창고> 창고로 옮길 수 없는 아이템 입니다."));
            return;
        }

        if (!pkSafebox->is_empty(p->safe_pos, pkItem->size())) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("<창고> 옮길 수 없는 위치입니다."));
            return;
        }

        if (pkItem->vnum() == UNIQUE_ITEM_SAFEBOX_EXPAND) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("<창고> 이 아이템은 넣을 수 없습니다."));
            return;
        }

        if (is_set(pkItem->anti_flag(), ITEM_ANTIFLAG_SAFEBOX)) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("<창고> 이 아이템은 넣을 수 없습니다."));
            return;
        }

        if (true == pkItem->is_locked()) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("<창고> 이 아이템은 넣을 수 없습니다."));
            return;
        }

        pkItem->remove_from_character();
        if (!pkItem->is_dragon_soul())
            ch->sync_quickslot(QUICKSLOT_TYPE_ITEM, p->item_pos.cell, 255);
        pkSafebox->add(p->safe_pos, pkItem);

        const std::string hint = std::format("{} {}", pkItem->name(), pkItem->count());
        log_manager::instance().item_log(ch, pkItem, "SAFEBOX PUT", hint.c_str());
    }

    void input_main::safebox_checkout(character *ch, const char *data, bool mall_value) {
        packet_cg_safebox_checkout *p = (packet_cg_safebox_checkout *)data;

        if (!ch->can_handle_item())
            return;

        safebox *pkSafebox;

        if (mall_value)
            pkSafebox = ch->mall();
        else
            pkSafebox = ch->get_safebox();

        if (!pkSafebox)
            return;

        item *pkItem = pkSafebox->get(p->safe_pos);

        if (!pkItem)
            return;

        if (!ch->is_empty_item_grid(p->item_pos, pkItem->size()))
            return;

        if (INVENTORY == p->item_pos.window_type && p->item_pos.is_belt_inventory_position() && !belt_inventory_helper::can_move_into_belt_inventory(pkItem))
            return;

        if (pkItem->is_dragon_soul()) {
            if (mall_value) {
                ds_manager::instance().dragon_soul_item_initialize(pkItem);
            }

            if (DRAGON_SOUL_INVENTORY != p->item_pos.window_type) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("<창고> 옮길 수 없는 위치입니다."));
                return;
            }

            item_pos DestPos = p->item_pos;
            if (!ds_manager::instance().is_valid_cell_for_this_item(pkItem, DestPos)) {
                int iCell = ch->empty_dragon_soul_inventory(pkItem);
                if (iCell < 0) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("<창고> 옮길 수 없는 위치입니다."));
                    return;
                }
                DestPos = item_pos(DRAGON_SOUL_INVENTORY, iCell);
            }

            pkSafebox->remove(p->safe_pos);
            pkItem->add_to_character(ch, DestPos);
            item_manager::instance().flush_delayed_save(pkItem);
        } else {
            if (DRAGON_SOUL_INVENTORY == p->item_pos.window_type) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("<창고> 옮길 수 없는 위치입니다."));
                return;
            }

            pkSafebox->remove(p->safe_pos);
            if (mall_value) {
                if (nullptr == pkItem->proto()) {
                    metin2::log::error("Item {:d} has no proto", pkItem->id());
                    return;
                }
                if (100 == pkItem->proto()->alter_to_magic_item_pct && 0 == pkItem->attribute_count()) {
                    pkItem->alter_to_magic_item();
                }
            }
            pkItem->add_to_character(ch, p->item_pos);
            item_manager::instance().flush_delayed_save(pkItem);
        }

        uint32_t dwID = pkItem->id();
        db_clientdesc->db_packet_header(HEADER_GD_ITEM_FLUSH, 0, sizeof(uint32_t));
        db_clientdesc->packet(&dwID, sizeof(uint32_t));

        const std::string hint = std::format("{} {}", pkItem->name(), pkItem->count());
        if (mall_value)
            log_manager::instance().item_log(ch, pkItem, "MALL GET", hint.c_str());
        else
            log_manager::instance().item_log(ch, pkItem, "SAFEBOX GET", hint.c_str());
    }

    void input_main::safebox_item_move(character *ch, const char *data) {
        struct packet_cg_item_move *pinfo = (struct packet_cg_item_move *)data;

        if (!ch->can_handle_item())
            return;

        if (!ch->get_safebox())
            return;

        ch->get_safebox()->move_item(pinfo->cell.cell, pinfo->cell_to.cell, pinfo->count);
    }

    void input_main::party_invite(character *ch, const char *data) {
        if (ch->get_arena()) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("대련장에서 사용하실 수 없습니다."));
            return;
        }

        packet_cg_party_invite *p = (packet_cg_party_invite *)data;

        character *pInvitee = character_manager::instance().find(p->vid);

        if (!pInvitee || !ch->get_desc() || !pInvitee->get_desc()) {
            metin2::log::debug("Party invite: target vid {:d} not found or offline", p->vid);
            return;
        }

        ch->party_invite(pInvitee);
    }

    void input_main::party_invite_answer(character *ch, const char *data) {
        if (ch->get_arena()) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("대련장에서 사용하실 수 없습니다."));
            return;
        }

        packet_cg_party_invite_answer *p = (packet_cg_party_invite_answer *)data;

        character *pInviter = character_manager::instance().find(p->leader_vid);

        if (!pInviter)
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 파티요청을 한 캐릭터를 찾을수 없습니다."));
        else if (!p->accept)
            pInviter->party_invite_deny(ch->player_id());
        else
            pInviter->party_invite_accept(ch);
    }

    void input_main::party_set_state(character *ch, const char *data) {
        if (!party_manager::instance().is_enable_pc_party()) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 서버 문제로 파티 관련 처리를 할 수 없습니다."));
            return;
        }

        packet_cg_party_set_state *p = (packet_cg_party_set_state *)data;

        if (!ch->get_party())
            return;

        if (ch->get_party()->leader_pid() != ch->player_id()) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 리더만 변경할 수 있습니다."));
            return;
        }

        if (!ch->get_party()->is_member(p->pid)) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 상태를 변경하려는 사람이 파티원이 아닙니다."));
            return;
        }

        uint32_t pid = p->pid;
        metin2::log::debug("Party: set role of pid {:d} to {:d} {}", pid, p->role, p->flag ? "on" : "off");

        switch (p->role) {
        case PARTY_ROLE_NORMAL:
            break;

        case PARTY_ROLE_ATTACKER:
        case PARTY_ROLE_TANKER:
        case PARTY_ROLE_BUFFER:
        case PARTY_ROLE_SKILL_MASTER:
        case PARTY_ROLE_HASTE:
        case PARTY_ROLE_DEFENDER:
            if (ch->get_party()->set_role(pid, p->role, p->flag)) {
                packet_party_state_change pack;
                pack.leader_pid = ch->player_id();
                pack.pid = p->pid;
                pack.role = p->role;
                pack.flag = p->flag;
                db_clientdesc->db_packet(HEADER_GD_PARTY_STATE_CHANGE, 0, &pack, sizeof(pack));
            }
            break;

        default:
            metin2::log::warn("Invalid party role from player {}: {:d}", ch->name(), p->role);
            break;
        }
    }

    void input_main::party_remove(character *ch, const char *data) {
        if (ch->get_arena()) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("대련장에서 사용하실 수 없습니다."));
            return;
        }

        if (!party_manager::instance().is_enable_pc_party()) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 서버 문제로 파티 관련 처리를 할 수 없습니다."));
            return;
        }

        if (ch->get_dungeon()) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 던전 안에서는 파티에서 추방할 수 없습니다."));
            return;
        }

        packet_cg_party_remove *p = (packet_cg_party_remove *)data;

        if (!ch->get_party())
            return;

        party *pParty = ch->get_party();
        if (pParty->leader_pid() == ch->player_id()) {
            if (ch->get_dungeon()) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 던젼내에서는 파티원을 추방할 수 없습니다."));
            } else {
                if (p->pid == ch->player_id() || pParty->member_count() == 2) {
                    party_manager::instance().delete_party(pParty);
                } else {
                    character *B = character_manager::instance().find_by_pid(p->pid);
                    if (B) {
                        B->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 파티에서 추방당하셨습니다."));
                    }
                    pParty->quit(p->pid);
                }
            }
        } else {
            if (p->pid == ch->player_id()) {
                if (ch->get_dungeon()) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 던젼내에서는 파티를 나갈 수 없습니다."));
                } else {
                    if (pParty->member_count() == 2) {
                        party_manager::instance().delete_party(pParty);
                    } else {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 파티에서 나가셨습니다."));
                        pParty->quit(ch->player_id());
                    }
                }
            } else {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 다른 파티원을 탈퇴시킬 수 없습니다."));
            }
        }
    }

    void input_main::answer_make_guild(character *ch, const char *data) {
        packet_cg_answer_make_guild *p = (packet_cg_answer_make_guild *)data;

        const uint32_t dwKey = ch->guild_creation_key();
        ch->set_guild_creation_key(0);

        if (dwKey == 0 || p->key != dwKey) {
            metin2::log::warn("Guild name answer from player {} without a matching request", ch->name());
            return;
        }

        if (ch->level() < 40 || ch->gold() < 200000)
            return;

        if (get_global_time() - ch->quest_flag("guild_building.new_disband_time") <
            guild_manager::instance().disband_delay()) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 해산한 후 %d일 이내에는 길드를 만들 수 없습니다."),
                quest::quest_manager::instance().event_flag("guild_disband_delay"));
            return;
        }

        if (get_global_time() - ch->quest_flag("guild_building.new_withdraw_time") <
            guild_manager::instance().withdraw_delay()) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 탈퇴한 후 %d일 이내에는 길드를 만들 수 없습니다."),
                quest::quest_manager::instance().event_flag("guild_withdraw_delay"));
            return;
        }

        if (ch->get_guild())
            return;

        guild_manager &gm = guild_manager::instance();

        guild_create_parameter cp;
        memset(&cp, 0, sizeof(cp));

        cp.master = ch;
        copy_packet_string(cp.name, sizeof(cp.name), p->guild_name, sizeof(p->guild_name));

        if (cp.name[0] == 0 || !check_name(cp.name)) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("적합하지 않은 길드 이름 입니다."));
            return;
        }

        uint32_t dwGuildID = gm.create_guild(cp);

        if (dwGuildID) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> [%s] 길드가 생성되었습니다."), cp.name);

            int GuildCreateFee;

            GuildCreateFee = 200000;

            ch->point_change(POINT_GOLD, -GuildCreateFee);
            db_manager::instance().send_money_log(MONEY_LOG_GUILD, ch->player_id(), -GuildCreateFee);

            const std::string log_text = std::format("GUILD_NAME {} MASTER {}", cp.name, ch->name());
            log_manager::instance().char_log(ch, 0, "MAKE_GUILD", log_text.c_str());

            ch->remove_specify_item(GUILD_CREATE_ITEM_VNUM, 1);
        } else
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드 생성에 실패하였습니다."));
    }

    void input_main::party_use_skill(character *ch, const char *data) {
        packet_cg_party_use_skill *p = (packet_cg_party_use_skill *)data;
        if (!ch->get_party())
            return;

        if (ch->player_id() != ch->get_party()->leader_pid()) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 파티 기술은 파티장만 사용할 수 있습니다."));
            return;
        }

        switch (p->skill_index) {
        case PARTY_SKILL_HEAL:
            ch->get_party()->heal_party();
            break;
        case PARTY_SKILL_WARP:
            {
                character *pch = character_manager::instance().find(p->vid);
                if (pch)
                    ch->get_party()->summon_to_leader(pch->player_id());
                else
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 소환하려는 대상을 찾을 수 없습니다."));
            }
            break;
        }
    }

    void input_main::party_parameter(character *ch, const char *data) {
        packet_cg_party_parameter *p = (packet_cg_party_parameter *)data;

        if (ch->get_party() && ch->get_party()->leader_pid() == ch->player_id())
            ch->get_party()->set_parameter(p->distribute_mode);
    }

    auto get_sub_packet_size(const guild_subheader_cg &header) -> size_t {
        switch (header) {
        case GUILD_SUBHEADER_CG_DEPOSIT_MONEY:          return sizeof(int);
        case GUILD_SUBHEADER_CG_WITHDRAW_MONEY:         return sizeof(int);
        case GUILD_SUBHEADER_CG_ADD_MEMBER:             return sizeof(uint32_t);
        case GUILD_SUBHEADER_CG_REMOVE_MEMBER:          return sizeof(uint32_t);
        case GUILD_SUBHEADER_CG_CHANGE_GRADE_NAME:      return 10;
        case GUILD_SUBHEADER_CG_CHANGE_GRADE_AUTHORITY: return sizeof(uint8_t) + sizeof(uint8_t);
        case GUILD_SUBHEADER_CG_OFFER:                  return sizeof(uint32_t);
        case GUILD_SUBHEADER_CG_CHARGE_GSP:             return sizeof(int);
        case GUILD_SUBHEADER_CG_POST_COMMENT:           return 1;
        case GUILD_SUBHEADER_CG_DELETE_COMMENT:         return sizeof(uint32_t);
        case GUILD_SUBHEADER_CG_REFRESH_COMMENT:        return 0;
        case GUILD_SUBHEADER_CG_CHANGE_MEMBER_GRADE:    return sizeof(uint32_t) + sizeof(uint8_t);
        case GUILD_SUBHEADER_CG_USE_SKILL:              return sizeof(packet_cg_guild_use_skill);
        case GUILD_SUBHEADER_CG_CHANGE_MEMBER_GENERAL:  return sizeof(uint32_t) + sizeof(uint8_t);
        case GUILD_SUBHEADER_CG_GUILD_INVITE_ANSWER:    return sizeof(uint32_t) + sizeof(uint8_t);
        }

        return 0;
    }

    auto input_main::guild(character *ch, const char *data, size_t bytes) -> int {
        if (bytes < sizeof(packet_cg_guild))
            return -1;

        const packet_cg_guild *p = reinterpret_cast<const packet_cg_guild *>(data);
        const char *c_pData = data + sizeof(packet_cg_guild);

        bytes -= sizeof(packet_cg_guild);

        const guild_subheader_cg SubHeader = static_cast<guild_subheader_cg>(p->subheader);
        const size_t SubPacketLen = get_sub_packet_size(SubHeader);

        if (bytes < SubPacketLen) {
            return -1;
        }

        game::guilds::guild *pGuild = ch->get_guild();

        if (nullptr == pGuild) {
            if (SubHeader != GUILD_SUBHEADER_CG_GUILD_INVITE_ANSWER) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드에 속해있지 않습니다."));
                return SubPacketLen;
            }
        }

        switch (SubHeader) {
        case GUILD_SUBHEADER_CG_DEPOSIT_MONEY:
            {
                return SubPacketLen;

                const int gold = std::min(*reinterpret_cast<const int *>(c_pData), deposit_limit());

                if (gold < 0) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 잘못된 금액입니다."));
                    return SubPacketLen;
                }

                if (ch->gold() < gold) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 가지고 있는 돈이 부족합니다."));
                    return SubPacketLen;
                }

                pGuild->request_deposit_money(ch, gold);
            }
            return SubPacketLen;

        case GUILD_SUBHEADER_CG_WITHDRAW_MONEY:
            {
                return SubPacketLen;

                const int gold = std::min(*reinterpret_cast<const int *>(c_pData), 500000);

                if (gold < 0) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 잘못된 금액입니다."));
                    return SubPacketLen;
                }
            }
            return SubPacketLen;

        case GUILD_SUBHEADER_CG_ADD_MEMBER:
            {
                const uint32_t vid = *reinterpret_cast<const uint32_t *>(c_pData);
                character *newmember = character_manager::instance().find(vid);

                if (!newmember || !newmember->is_pc()) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 그러한 사람을 찾을 수 없습니다."));
                    return SubPacketLen;
                }

                if (!ch->is_pc())
                    return SubPacketLen;

                pGuild->invite(ch, newmember);
            }
            return SubPacketLen;

        case GUILD_SUBHEADER_CG_REMOVE_MEMBER:
            {
                if (pGuild->under_any_war() != 0) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드전 중에는 길드원을 탈퇴시킬 수 없습니다."));
                    return SubPacketLen;
                }

                const uint32_t pid = *reinterpret_cast<const uint32_t *>(c_pData);
                const guild_member *m = pGuild->member(ch->player_id());

                if (nullptr == m)
                    return -1;

                character *member = character_manager::instance().find_by_pid(pid);

                if (member) {
                    if (member->get_guild() != pGuild) {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 상대방이 같은 길드가 아닙니다."));
                        return SubPacketLen;
                    }

                    if (!pGuild->has_grade_auth(m->grade, GUILD_AUTH_REMOVE_MEMBER)) {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드원을 강제 탈퇴 시킬 권한이 없습니다."));
                        return SubPacketLen;
                    }

                    member->set_quest_flag("guild_building.new_withdraw_time", get_global_time());
                    pGuild->request_remove_member(member->player_id());

                } else {
                    if (!pGuild->has_grade_auth(m->grade, GUILD_AUTH_REMOVE_MEMBER)) {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드원을 강제 탈퇴 시킬 권한이 없습니다."));
                        return SubPacketLen;
                    }

                    if (pGuild->request_remove_member(pid))
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드원을 강제 탈퇴 시켰습니다."));
                    else
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 그러한 사람을 찾을 수 없습니다."));
                }
            }
            return SubPacketLen;

        case GUILD_SUBHEADER_CG_CHANGE_GRADE_NAME:
            {
                char gradename[GUILD_GRADE_NAME_MAX_LEN + 1];
                copy_packet_string(gradename, sizeof(gradename), c_pData + 1, SubPacketLen - 1);

                const guild_member *m = pGuild->member(ch->player_id());

                if (nullptr == m)
                    return -1;

                if (m->grade != GUILD_LEADER_GRADE) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 직위 이름을 변경할 권한이 없습니다."));
                } else if (*c_pData == GUILD_LEADER_GRADE) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드장의 직위 이름은 변경할 수 없습니다."));
                } else if (!check_name(gradename)) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 적합하지 않은 직위 이름 입니다."));
                } else {
                    pGuild->change_grade_name(*c_pData, gradename);
                }
            }
            return SubPacketLen;

        case GUILD_SUBHEADER_CG_CHANGE_GRADE_AUTHORITY:
            {
                const guild_member *m = pGuild->member(ch->player_id());

                if (nullptr == m)
                    return -1;

                if (m->grade != GUILD_LEADER_GRADE) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 직위 권한을 변경할 권한이 없습니다."));
                } else if (*c_pData == GUILD_LEADER_GRADE) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드장의 권한은 변경할 수 없습니다."));
                } else {
                    pGuild->change_grade_auth(*c_pData, *(c_pData + 1));
                }
            }
            return SubPacketLen;

        case GUILD_SUBHEADER_CG_OFFER:
            {
                uint32_t offer = *reinterpret_cast<const uint32_t *>(c_pData);

                if (pGuild->level() >= GUILD_MAX_LEVEL) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드가 이미 최고 레벨입니다."));
                } else {
                    offer /= 100;
                    offer *= 100;

                    if (pGuild->offer_exp(ch, offer)) {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> %u의 경험치를 투자하였습니다."), offer);
                    } else {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 경험치 투자에 실패하였습니다."));
                    }
                }
            }
            return SubPacketLen;

        case GUILD_SUBHEADER_CG_CHARGE_GSP:
            {
                const int offer = *reinterpret_cast<const int *>(c_pData);
                const int64_t gold = static_cast<int64_t>(offer) * 100;

                if (offer < 0 || gold > INT32_MAX || ch->gold() < gold) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 돈이 부족합니다."));
                    return SubPacketLen;
                }

                if (!pGuild->charge_sp(ch, offer)) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 용신력 회복에 실패하였습니다."));
                }
            }
            return SubPacketLen;

        case GUILD_SUBHEADER_CG_POST_COMMENT:
            {
                const size_t length = *c_pData;

                if (length > GUILD_COMMENT_MAX_LEN) {
                    metin2::log::warn("Guild comment from player {} too long ({:d} bytes), disconnecting", ch->name(), length);
                    ch->get_desc()->set_phase(PHASE_CLOSE);
                    return -1;
                }

                if (bytes < 1 + length)
                    return -1;

                const guild_member *m = pGuild->member(ch->player_id());

                if (nullptr == m)
                    return -1;

                if (length && !pGuild->has_grade_auth(m->grade, GUILD_AUTH_NOTICE) && *(c_pData + 1) == '!') {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 공지글을 작성할 권한이 없습니다."));
                } else {
                    std::string str(c_pData + 1, length);
                    pGuild->add_comment(ch, str);
                }

                return (1 + length);
            }

        case GUILD_SUBHEADER_CG_DELETE_COMMENT:
            {
                const uint32_t comment_id = *reinterpret_cast<const uint32_t *>(c_pData);

                pGuild->delete_comment(ch, comment_id);
            }
            return SubPacketLen;

        case GUILD_SUBHEADER_CG_REFRESH_COMMENT:
            pGuild->refresh_comment(ch);
            return SubPacketLen;

        case GUILD_SUBHEADER_CG_CHANGE_MEMBER_GRADE:
            {
                const uint32_t pid = *reinterpret_cast<const uint32_t *>(c_pData);
                const uint8_t grade = *(c_pData + sizeof(uint32_t));
                const guild_member *m = pGuild->member(ch->player_id());

                if (nullptr == m)
                    return -1;

                if (m->grade != GUILD_LEADER_GRADE)
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 직위를 변경할 권한이 없습니다."));
                else if (ch->player_id() == pid)
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드장의 직위는 변경할 수 없습니다."));
                else if (grade == 1)
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드장으로 직위를 변경할 수 없습니다."));
                else if (grade < 1 || grade > GUILD_GRADE_COUNT)
                    metin2::log::warn("Player {} sent invalid guild grade {:d}", ch->name(), grade);
                else
                    pGuild->change_member_grade(pid, grade);
            }
            return SubPacketLen;

        case GUILD_SUBHEADER_CG_USE_SKILL:
            {
                const packet_cg_guild_use_skill *p = reinterpret_cast<const packet_cg_guild_use_skill *>(c_pData);

                pGuild->use_skill(p->vnum, ch, p->pid);
            }
            return SubPacketLen;

        case GUILD_SUBHEADER_CG_CHANGE_MEMBER_GENERAL:
            {
                const uint32_t pid = *reinterpret_cast<const uint32_t *>(c_pData);
                const uint8_t is_general = *(c_pData + sizeof(uint32_t));
                const guild_member *m = pGuild->member(ch->player_id());

                if (nullptr == m)
                    return -1;

                if (m->grade != GUILD_LEADER_GRADE) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 장군을 지정할 권한이 없습니다."));
                } else {
                    if (!pGuild->change_member_general(pid, is_general)) {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 더이상 장수를 지정할 수 없습니다."));
                    }
                }
            }
            return SubPacketLen;

        case GUILD_SUBHEADER_CG_GUILD_INVITE_ANSWER:
            {
                const uint32_t guild_id = *reinterpret_cast<const uint32_t *>(c_pData);
                const uint8_t accept = *(c_pData + sizeof(uint32_t));

                game::guilds::guild *g = guild_manager::instance().find_guild(guild_id);

                if (g) {
                    if (accept)
                        g->invite_accept(ch);
                    else
                        g->invite_deny(ch->player_id());
                }
            }
            return SubPacketLen;
        }

        return 0;
    }

    void input_main::fishing(character *ch, const char *data) {
        packet_cg_fishing *p = (packet_cg_fishing *)data;
        ch->set_rotation(p->dir * 5);
        ch->fishing();
        return;
    }

    void input_main::item_give(character *ch, const char *data) {
        packet_cg_give_item *p = (packet_cg_give_item *)data;
        character *to_ch = character_manager::instance().find(p->target_vid);

        if (to_ch)
            ch->give_item(to_ch, p->item_pos);
        else
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("아이템을 건네줄 수 없습니다."));
    }

    void input_main::hack(character *ch, const char *data) {
        packet_cg_hack *p = (packet_cg_hack *)data;

        char buf[sizeof(p->buf)];
        copy_packet_string(buf, sizeof(buf), p->buf, sizeof(p->buf));

        metin2::log::warn("Client reported hack for player {}: {}", ch->name(), buf);

        ch->get_desc()->set_phase(PHASE_CLOSE);
    }

    auto input_main::my_shop(character *ch, const char *data, size_t bytes) -> int {
        packet_cg_my_shop *p = (packet_cg_my_shop *)data;
        int iExtraLen = p->count * sizeof(shop_item_table);

        if (bytes < sizeof(packet_cg_my_shop) + iExtraLen)
            return -1;

        if (ch->gold() >= GOLD_MAX) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("소유 돈이 20억냥을 넘어 거래를 핼수가 없습니다."));
            metin2::log::debug("Private shop refused: gold at maximum (pid {:d}, name {})", ch->player_id(), ch->name());
            return (iExtraLen);
        }

        if (ch->is_stun() || ch->is_dead())
            return (iExtraLen);

        if (ch->get_exchange() || ch->is_open_safebox() || ch->shop_owner() || ch->is_cube_open()) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("다른 거래중일경우 개인상점을 열수가 없습니다."));
            return (iExtraLen);
        }

        char szSign[SHOP_SIGN_MAX_LEN + 1];
        copy_packet_string(szSign, sizeof(szSign), p->sign, sizeof(p->sign));

        metin2::log::debug("Player {} opens a private shop with {:d} items", ch->name(), p->count);
        ch->open_my_shop(szSign, (shop_item_table *)(data + sizeof(packet_cg_my_shop)), p->count);
        return (iExtraLen);
    }

    void input_main::refine(character *ch, const char *data) {
        const packet_cg_refine *p = reinterpret_cast<const packet_cg_refine *>(data);

        if (ch->get_exchange() || ch->is_open_safebox() || ch->shop_owner() || ch->my_shop() || ch->is_cube_open()) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("창고,거래창등이 열린 상태에서는 개량을 할수가 없습니다"));
            ch->clear_refine_mode();
            return;
        }

        if (p->type == 255) {
            ch->clear_refine_mode();
            return;
        }

        if (p->pos >= INVENTORY_MAX_NUM) {
            ch->clear_refine_mode();
            return;
        }

        item *item = ch->inventory_item(p->pos);

        if (!item) {
            ch->clear_refine_mode();
            return;
        }

        ch->set_refine_time();

        if (p->type == REFINE_TYPE_NORMAL) {
            ch->do_refine(item);
        } else if (p->type == REFINE_TYPE_SCROLL || p->type == REFINE_TYPE_HYUNIRON || p->type == REFINE_TYPE_MUSIN || p->type == REFINE_TYPE_BDRAGON) {
            ch->do_refine_with_scroll(item);
        } else if (p->type == REFINE_TYPE_MONEY_ONLY) {
            game::items::item *const item = ch->inventory_item(p->pos);

            if (nullptr != item) {
                if (500 <= item->refine_set()) {
                    log_manager::instance().hack_log("DEVIL_TOWER_REFINE_HACK", ch);
                } else {
                    if (ch->quest_flag("deviltower_zone.can_refine")) {
                        ch->do_refine(item, true);
                        ch->set_quest_flag("deviltower_zone.can_refine", 0);
                    } else {
                        ch->chat_packet(CHAT_TYPE_INFO, "사귀 타워 완료 보상은 한번까지 사용가능합니다.");
                    }
                }
            }
        }

        ch->clear_refine_mode();
    }

    namespace {

        auto is_blocked_for_observer(uint8_t header) -> bool {
            switch (header) {
            case HEADER_CG_ITEM_USE:
            case HEADER_CG_ITEM_DROP:
            case HEADER_CG_ITEM_DROP2:
            case HEADER_CG_ITEM_MOVE:
            case HEADER_CG_ITEM_PICKUP:
            case HEADER_CG_ITEM_USE_TO_ITEM:
            case HEADER_CG_ITEM_GIVE:
            case HEADER_CG_EXCHANGE:
            case HEADER_CG_ATTACK:
            case HEADER_CG_SHOOT:
            case HEADER_CG_USE_SKILL:
                return true;
            default:
                return false;
            }
        }

    }

    auto input_main::analyze(desc *d, uint8_t header, const char *data) -> int {
        character *ch;

        if (!(ch = d->get_character())) {
            metin2::log::warn("Game packet on a connection without a character, host {}", d->host_name());
            d->set_phase(PHASE_CLOSE);
            return (0);
        }

        if (ch && ch->is_pc()) {
            if (get_global_time() < ch->analyze_protect) {
                ch->analyze_protect_count = ch->analyze_protect_count + 1;
                if (metin2::config::test_server()) {
                    ch->chat_packet(CHAT_TYPE_INFO, "<test server> analyze_protect_count. Count %d", ch->analyze_protect_count);
                }

                if (ch->analyze_protect_count >= 250) {
                    ch->analyze_protect_count = 0;
                    d->set_phase(PHASE_CLOSE);
                    return (0);
                }
            } else
                ch->analyze_protect_count = 0;

            ch->analyze_protect = get_global_time() + 1;
        }

        int iExtraLen = 0;

        if (metin2::config::test_server() && header != HEADER_CG_MOVE)
            metin2::log::debug("Game packet header {:d}", header);

        if (ch->is_observer_mode() && is_blocked_for_observer(header))
            return 0;

        switch (header) {
        case HEADER_CG_PONG:
            pong(d);
            break;

        case HEADER_CG_TIME_SYNC:
            handshake(d, data);
            break;

        case HEADER_CG_CHAT:
            if (metin2::config::test_server()) {
                char *pBuf = (char *)data;
                metin2::log::debug("Chat: {}", pBuf + sizeof(packet_cg_chat));
            }

            if ((iExtraLen = chat(ch, data, buffer_left_)) < 0)
                return -1;
            break;

        case HEADER_CG_WHISPER:
            if ((iExtraLen = whisper(ch, data, buffer_left_)) < 0)
                return -1;
            break;

        case HEADER_CG_MOVE:
            move(ch, data);

            if (check_client_version_enabled) {
                int version_value = atoi(client_version.c_str());
                int date = atoi(d->client_version());

                if (version_value != date) {
                    ch->chat_packet(CHAT_TYPE_NOTICE, lc_text("클라이언트 버전이 틀려 로그아웃 됩니다. 정상적으로 패치 후 접속하세요."));
                    d->delayed_disconnect(10);
                    log_manager::instance().hack_log("VERSION_CONFLICT", d->get_account_table().login, ch->name(), d->host_name());
                }
            }
            break;

        case HEADER_CG_CHARACTER_POSITION:
            position(ch, data);
            break;

        case HEADER_CG_ITEM_USE:
            item_use(ch, data);
            break;

        case HEADER_CG_ITEM_DROP:
            item_drop(ch, data);
            break;

        case HEADER_CG_ITEM_DROP2:
            item_drop2(ch, data);
            break;

        case HEADER_CG_ITEM_MOVE:
            item_move(ch, data);
            break;

        case HEADER_CG_ITEM_PICKUP:
            item_pickup(ch, data);
            break;

        case HEADER_CG_ITEM_USE_TO_ITEM:
            item_to_item(ch, data);
            break;

        case HEADER_CG_ITEM_GIVE:
            item_give(ch, data);
            break;

        case HEADER_CG_EXCHANGE:
            exchange(ch, data);
            break;

        case HEADER_CG_ATTACK:
        case HEADER_CG_SHOOT:
            attack(ch, header, data);
            break;

        case HEADER_CG_USE_SKILL:
            use_skill(ch, data);
            break;

        case HEADER_CG_QUICKSLOT_ADD:
            quickslot_add(ch, data);
            break;

        case HEADER_CG_QUICKSLOT_DEL:
            quickslot_delete(ch, data);
            break;

        case HEADER_CG_QUICKSLOT_SWAP:
            quickslot_swap(ch, data);
            break;

        case HEADER_CG_SHOP:
            if ((iExtraLen = shop(ch, data, buffer_left_)) < 0)
                return -1;
            break;

        case HEADER_CG_MESSENGER:
            if ((iExtraLen = messenger(ch, data, buffer_left_)) < 0)
                return -1;
            break;

        case HEADER_CG_ON_CLICK:
            on_click(ch, data);
            break;

        case HEADER_CG_SYNC_POSITION:
            if ((iExtraLen = sync_position(ch, data, buffer_left_)) < 0)
                return -1;
            break;

        case HEADER_CG_ADD_FLY_TARGETING:
        case HEADER_CG_FLY_TARGETING:
            fly_target(ch, data, header);
            break;

        case HEADER_CG_SCRIPT_BUTTON:
            script_button(ch, data);
            break;

        case HEADER_CG_SCRIPT_SELECT_ITEM:
            script_select_item(ch, data);
            break;

        case HEADER_CG_SCRIPT_ANSWER:
            script_answer(ch, data);
            break;

        case HEADER_CG_QUEST_INPUT_STRING:
            quest_input_string(ch, data);
            break;

        case HEADER_CG_QUEST_CONFIRM:
            quest_confirm(ch, data);
            break;

        case HEADER_CG_TARGET:
            target(ch, data);
            break;

        case HEADER_CG_WARP:
            warp(ch, data);
            break;

        case HEADER_CG_SAFEBOX_CHECKIN:
            safebox_checkin(ch, data);
            break;

        case HEADER_CG_SAFEBOX_CHECKOUT:
            safebox_checkout(ch, data, false);
            break;

        case HEADER_CG_SAFEBOX_ITEM_MOVE:
            safebox_item_move(ch, data);
            break;

        case HEADER_CG_MALL_CHECKOUT:
            safebox_checkout(ch, data, true);
            break;

        case HEADER_CG_PARTY_INVITE:
            party_invite(ch, data);
            break;

        case HEADER_CG_PARTY_REMOVE:
            party_remove(ch, data);
            break;

        case HEADER_CG_PARTY_INVITE_ANSWER:
            party_invite_answer(ch, data);
            break;

        case HEADER_CG_PARTY_SET_STATE:
            party_set_state(ch, data);
            break;

        case HEADER_CG_PARTY_USE_SKILL:
            party_use_skill(ch, data);
            break;

        case HEADER_CG_PARTY_PARAMETER:
            party_parameter(ch, data);
            break;

        case HEADER_CG_ANSWER_MAKE_GUILD:
            answer_make_guild(ch, data);
            break;

        case HEADER_CG_GUILD:
            if ((iExtraLen = guild(ch, data, buffer_left_)) < 0)
                return -1;
            break;

        case HEADER_CG_FISHING:
            fishing(ch, data);
            break;

        case HEADER_CG_HACK:
            hack(ch, data);
            break;

        case HEADER_CG_MYSHOP:
            if ((iExtraLen = my_shop(ch, data, buffer_left_)) < 0)
                return -1;
            break;

        case HEADER_CG_REFINE:
            refine(ch, data);
            break;

        case HEADER_CG_CLIENT_VERSION:
            version(ch, data);
            break;

        case HEADER_CG_DRAGON_SOUL_REFINE:
            {
                packet_cg_dragon_soul_refine *p = reinterpret_cast<packet_cg_dragon_soul_refine *>((void *)data);
                switch (p->sub_type) {
                case DS_SUB_HEADER_CLOSE:
                    ch->dragon_soul_refine_window_close();
                    break;
                case DS_SUB_HEADER_DO_REFINE_GRADE:
                    {
                        ds_manager::instance().do_refine_grade(ch, p->item_grid);
                    }
                    break;
                case DS_SUB_HEADER_DO_REFINE_STEP:
                    {
                        ds_manager::instance().do_refine_step(ch, p->item_grid);
                    }
                    break;
                case DS_SUB_HEADER_DO_REFINE_STRENGTH:
                    {
                        ds_manager::instance().do_refine_strength(ch, p->item_grid);
                    }
                    break;
                }
            }

            break;
        }
        return (iExtraLen);
    }
}
