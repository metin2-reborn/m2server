#include <metin2/config/config.h>
#include <metin2/game/admin/cmd.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/constants.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/characters/affect.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/characters/horse_rider.h>
#include <metin2/game/characters/mob_manager.h>
#include <metin2/game/combat/battle.h>
#include <metin2/game/combat/skill.h>
#include <metin2/game/empire/castle.h>
#include <metin2/game/empire/monarch.h>
#include <metin2/game/empire/priv_manager.h>
#include <metin2/game/empire/start_position.h>
#include <metin2/game/empire/threeway_war.h>
#include <metin2/game/events/arena.h>
#include <metin2/game/events/battle_arena.h>
#include <metin2/game/events/fishing.h>
#include <metin2/game/events/mining.h>
#include <metin2/game/events/xmas_event.h>
#include <metin2/game/guilds/building.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/guilds/guild_manager.h>
#include <metin2/game/items/cube.h>
#include <metin2/game/items/dragon_soul.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/parties/party.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/protocol/desc_manager.h>
#include <metin2/game/protocol/p2p.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/quest.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/quest/questnpc.h>
#include <metin2/game/quest/questpc.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/text/locale_service.h>
#include <metin2/game/world/regen.h>
#include <metin2/game/world/sectree_manager.h>

#include <metin2/common/utils.h>
#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/apply_types.h>
#include <metin2/proto/building.h>
#include <metin2/proto/character_types.h>
#include <metin2/proto/chat_types.h>
#include <metin2/proto/inventory.h>
#include <metin2/proto/item_types.h>
#include <metin2/proto/log_types.h>
#include <metin2/proto/mob_types.h>
#include <metin2/proto/tables.h>
#include <metin2/sql/query.h>

#include <mysql.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <iterator>
#include <list>
#include <string>
#include <utility>
#include <vector>

#include <strings.h>

namespace metin2::game::inline admin {

    namespace {

        auto resolve_mob(const char *arg) -> const mob * {
            const mob *mob;

            if (is_digit(*arg)) {
                uint32_t vnum = 0;
                str_to_number(vnum, arg);
                mob = get_mob_manager().get(vnum);
            } else
                mob = get_mob_manager().get(arg, true);

            return mob && mob->table.vnum ? mob : nullptr;
        }

        auto spawn_count(const char *arg, int max_count) -> int {
            int count = 1;

            if (*arg) {
                count = 0;
                str_to_number(count, arg);
            }

            return std::min(max_count, count);
        }

        void spawn_around(character *ch, const mob *proto, int count, void (character::*on_spawn)() = nullptr) {
            while (count--) {
                character *tch = get_character_manager().spawn_mob_range(proto->table.vnum,
                    ch->map_index(),
                    ch->x() - number(200, 750),
                    ch->y() - number(200, 750),
                    ch->x() + number(200, 750),
                    ch->y() + number(200, 750),
                    true,
                    proto->table.type == CHAR_TYPE_STONE);

                if (tch && on_spawn)
                    (tch->*on_spawn)();
            }
        }

        auto find_target_or_self(character *ch, const char *name) -> character * {
            if (!*name)
                return ch;

            character *tch = get_character_manager().find_pc(name);

            if (!tch)
                ch->chat_packet(CHAT_TYPE_INFO, "There is no such character.");

            return tch;
        }

        template <typename F>
        void with_weapon(character *ch, F &&fn) {
            if (item *weapon = ch->wear(WEAR_WEAPON))
                fn(weapon);
        }

    }

    enum {
        COMMANDAFFECT_STUN,
        COMMANDAFFECT_SLOW,
    };

    void command_apply_affect(character *ch, const char *argument, const char *affect_name, int cmd_affect) {
        char arg1[256];
        one_argument(argument, arg1, sizeof(arg1));

        if (!*arg1) {
            ch->chat_packet(CHAT_TYPE_INFO, "Usage: %s <name>", affect_name);
            return;
        }

        character *tch = get_character_manager().find_pc(arg1);
        if (!tch) {
            ch->chat_packet(CHAT_TYPE_INFO, "%s is not in same map", arg1);
            return;
        }

        switch (cmd_affect) {
        case COMMANDAFFECT_STUN:
            skill_attack_affect(tch, 1000, IMMUNE_STUN, AFFECT_STUN, POINT_NONE, 0, AFF_STUN, 30, "GM_STUN");
            break;
        case COMMANDAFFECT_SLOW:
            skill_attack_affect(tch, 1000, IMMUNE_SLOW, AFFECT_SLOW, POINT_MOV_SPEED, -30, AFF_SLOW, 30, "GM_SLOW");
            break;
        }

        metin2::log::info("Gm {} applied {} to player {}", ch->name(), affect_name, arg1);

        ch->chat_packet(CHAT_TYPE_INFO, "%s %s", arg1, affect_name);
    }

    ACMD(do_stun) {
        command_apply_affect(ch, argument, "stun", COMMANDAFFECT_STUN);
    }

    ACMD(do_slow) {
        command_apply_affect(ch, argument, "slow", COMMANDAFFECT_SLOW);
    }

    ACMD(do_transfer) {
        char arg1[256];
        one_argument(argument, arg1, sizeof(arg1));

        if (!*arg1) {
            ch->chat_packet(CHAT_TYPE_INFO, "Usage: transfer <name>");
            return;
        }

        character *tch = get_character_manager().find_pc(arg1);
        if (!tch) {
            cci *cci = get_p2p_manager().find(arg1);

            if (cci) {
                if (cci->channel != metin2::config::server().channel) {
                    ch->chat_packet(CHAT_TYPE_INFO, "Target is in %d channel (my channel %d)", cci->channel, metin2::config::server().channel);
                    return;
                }

                packet_gg_transfer pgg;

                pgg.header = HEADER_GG_TRANSFER;
                strlcpy(pgg.name, arg1, sizeof(pgg.name));
                pgg.x = ch->x();
                pgg.y = ch->y();

                get_p2p_manager().send(&pgg, sizeof(packet_gg_transfer));
                ch->chat_packet(CHAT_TYPE_INFO, "Transfer requested.");
            } else {
                ch->chat_packet(CHAT_TYPE_INFO, "There is no character(%s) by that name", arg1);
                metin2::log::debug("Transfer: no player named {}", arg1);
            }

            return;
        }

        if (ch == tch) {
            ch->chat_packet(CHAT_TYPE_INFO, "Transfer me?!?");
            return;
        }

        tch->warp_set(ch->x(), ch->y(), ch->map_index());
    }

    struct goto_info {
        std::string st_name;

        uint8_t empire;
        int index;
        uint32_t x, y;

        goto_info() {
            st_name = "";
            empire = 0;
            index = 0;

            x = 0;
            y = 0;
        }
        goto_info(const goto_info &src) {
            copy(src);
        }
        void operator=(const goto_info &src) {
            copy(src);
        }
        void copy(const goto_info &src) {
            st_name = src.st_name;
            empire = src.empire;
            index = src.index;

            x = src.x;
            y = src.y;
        }
    };

    static std::vector<goto_info> vec_goto_info;

    void character_add_goto_info(const std::string &st_name_value, uint8_t empire, int index_value, uint32_t x, uint32_t y) {
        goto_info new_goto_info;
        new_goto_info.st_name = st_name_value;
        new_goto_info.empire = empire;
        new_goto_info.index = index_value;
        new_goto_info.x = x;
        new_goto_info.y = y;
        vec_goto_info.push_back(new_goto_info);

        metin2::log::debug("Goto location {}: empire {:d}, map {:d}, position {:d}x{:d}", st_name_value, empire, index_value, x, y);
    }

    auto find_in_string(const char *find, const char *in) -> bool {
        const char *c = in;
        const char *p;

        p = strchr(c, '|');

        if (!p)
            return (0 == strncasecmp(find, in, strlen(find)));
        else {
            std::string sz;

            do {
                sz.assign(c, std::min<std::size_t>(64, p - c));

                if (!strncasecmp(find, sz.c_str(), strlen(find)))
                    return true;

                c = p + 1;
            } while ((p = strchr(c, '|')));

            sz.assign(c, std::min<std::size_t>(64, strlen(c)));

            if (!strncasecmp(find, sz.c_str(), strlen(find)))
                return true;
        }

        return false;
    }

    auto character_go_to_name(character *ch, uint8_t empire, int index_value, const char *goto_name) -> bool {
        std::vector<goto_info>::iterator i;
        for (i = vec_goto_info.begin(); i != vec_goto_info.end(); ++i) {
            const goto_info &c_each_goto_info = *i;

            if (index_value != 0) {
                if (index_value != c_each_goto_info.index)
                    continue;
            } else if (!find_in_string(goto_name, c_each_goto_info.st_name.c_str()))
                continue;

            if (c_each_goto_info.empire == 0 || c_each_goto_info.empire == empire) {
                int x = c_each_goto_info.x * 100;
                int y = c_each_goto_info.y * 100;

                ch->chat_packet(CHAT_TYPE_INFO, "You warp to ( %d, %d )", x, y);
                ch->warp_set(x, y);
                ch->stop();
                return true;
            }
        }
        return false;
    }

    ACMD(do_goto) {
        char arg1[256], arg2[256];
        int x = 0, y = 0, z = 0;

        two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

        if (!*arg1 && !*arg2) {
            ch->chat_packet(CHAT_TYPE_INFO, "Usage: goto <x meter> <y meter>");
            return;
        }

        if (is_digit(*arg1) && is_digit(*arg2)) {
            str_to_number(x, arg1);
            str_to_number(y, arg2);

            pixel_position p;

            if (get_sectree_manager().map_base_position(ch->x(), ch->y(), p)) {
                x += p.x / 100;
                y += p.y / 100;
            }

            ch->chat_packet(CHAT_TYPE_INFO, "You goto ( %d, %d )", x, y);
        } else {
            int map_index = 0;
            uint8_t empire = 0;

            if (*arg1 == '#')
                str_to_number(map_index, (arg1 + 1));

            if (*arg2 && is_digit(*arg2)) {
                str_to_number(empire, arg2);
                empire = std::clamp<int>(empire, 1, 3);
            } else
                empire = ch->empire();

            if (character_go_to_name(ch, empire, map_index, arg1)) {
                ch->chat_packet(CHAT_TYPE_INFO, "Cannot find map command syntax: /goto <mapname> [empire]");
                return;
            }

            return;
        }

        x *= 100;
        y *= 100;

        ch->show(ch->map_index(), x, y, z);
        ch->stop();
    }

    ACMD(do_warp) {
        char arg1[256], arg2[256];

        two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

        if (!*arg1) {
            ch->chat_packet(CHAT_TYPE_INFO, "Usage: warp <character name> | <x meter> <y meter>");
            return;
        }

        int x = 0, y = 0;

        if (is_digit(*arg1) && is_digit(*arg2)) {
            str_to_number(x, arg1);
            str_to_number(y, arg2);
        } else {
            character *tch = get_character_manager().find_pc(arg1);

            if (nullptr == tch) {
                const cci *cci = get_p2p_manager().find(arg1);

                if (nullptr != cci) {
                    if (cci->channel != metin2::config::server().channel) {
                        ch->chat_packet(CHAT_TYPE_INFO, "Target is in %d channel (my channel %d)", cci->channel, metin2::config::server().channel);
                        return;
                    }

                    ch->warp_to_pid(cci->pid);
                } else {
                    ch->chat_packet(CHAT_TYPE_INFO, "There is no one by that name");
                }

                return;
            } else {
                x = tch->x() / 100;
                y = tch->y() / 100;
            }
        }

        x *= 100;
        y *= 100;

        ch->chat_packet(CHAT_TYPE_INFO, "You warp to ( %d, %d )", x, y);
        ch->warp_set(x, y);
        ch->stop();
    }

    ACMD(do_item) {
        char arg1[256], arg2[256];
        two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

        if (!*arg1) {
            ch->chat_packet(CHAT_TYPE_INFO, "Usage: item <item vnum>");
            return;
        }

        int count = 1;

        if (*arg2) {
            str_to_number(count, arg2);
            count = std::clamp(count, 1, ITEM_MAX_COUNT);
        }

        uint32_t vnum_value;

        if (is_digit(*arg1))
            str_to_number(vnum_value, arg1);
        else {
            if (!get_item_manager().vnum(arg1, vnum_value)) {
                ch->chat_packet(CHAT_TYPE_INFO, "#%u item not exist by that vnum.", vnum_value);
                return;
            }
        }

        item *item = get_item_manager().create_item(vnum_value, count, 0, true);

        if (item) {
            if (item->is_dragon_soul()) {
                int empty_pos = ch->empty_dragon_soul_inventory(item);

                if (empty_pos != -1) {
                    item->add_to_character(ch, item_pos(DRAGON_SOUL_INVENTORY, empty_pos));
                    get_log_manager().item_log(ch, item, "GM", item->name());
                } else {
                    get_item_manager().destroy_item(item);
                    if (!ch->dragon_soul_is_qualified()) {
                        ch->chat_packet(CHAT_TYPE_INFO, "인벤이 활성화 되지 않음.");
                    } else
                        ch->chat_packet(CHAT_TYPE_INFO, "Not enough inventory space.");
                }
            } else {
                int empty_pos = ch->empty_inventory(item->size());

                if (empty_pos != -1) {
                    item->add_to_character(ch, item_pos(INVENTORY, empty_pos));
                    get_log_manager().item_log(ch, item, "GM", item->name());
                } else {
                    get_item_manager().destroy_item(item);
                    ch->chat_packet(CHAT_TYPE_INFO, "Not enough inventory space.");
                }
            }
        } else {
            ch->chat_packet(CHAT_TYPE_INFO, "#%u item not exist by that vnum.", vnum_value);
        }
    }

    ACMD(do_group_random) {
        char arg1[256];
        one_argument(argument, arg1, sizeof(arg1));

        if (!*arg1) {
            ch->chat_packet(CHAT_TYPE_INFO, "Usage: grrandom <group vnum>");
            return;
        }

        uint32_t vnum = 0;
        str_to_number(vnum, arg1);
        get_character_manager().spawn_group_group(vnum, ch->map_index(), ch->x() - 500, ch->y() - 500, ch->x() + 500, ch->y() + 500);
    }

    ACMD(do_group) {
        char arg1[256];
        one_argument(argument, arg1, sizeof(arg1));

        if (!*arg1) {
            ch->chat_packet(CHAT_TYPE_INFO, "Usage: group <group vnum>");
            return;
        }

        uint32_t vnum = 0;
        str_to_number(vnum, arg1);

        if (metin2::config::test_server())
            metin2::log::debug("Gm spawns group {:d} on map {:d} at {:d}x{:d}", vnum, ch->map_index(), ch->x(), ch->y());

        get_character_manager().spawn_group(vnum, ch->map_index(), ch->x() - 500, ch->y() - 500, ch->x() + 500, ch->y() + 500);
    }

    ACMD(do_mob_coward) {
        char arg1[256], arg2[256];

        two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

        if (!*arg1) {
            ch->chat_packet(CHAT_TYPE_INFO, "Usage: mc <vnum>");
            return;
        }

        const mob *mob_value = resolve_mob(arg1);

        if (!mob_value) {
            ch->chat_packet(CHAT_TYPE_INFO, "No such mob by that vnum");
            return;
        }

        spawn_around(ch, mob_value, spawn_count(arg2, 20), &character::set_coward);
    }

    ACMD(do_mob_map) {
        char arg1[256];
        one_argument(argument, arg1, sizeof(arg1));

        if (!*arg1) {
            ch->chat_packet(CHAT_TYPE_INFO, "Syntax: mm <vnum>");
            return;
        }

        uint32_t vnum = 0;
        str_to_number(vnum, arg1);
        character *tch = get_character_manager().spawn_mob_random_position(vnum, ch->map_index());

        if (tch)
            ch->chat_packet(CHAT_TYPE_INFO, "%s spawned in %dx%d", tch->name(), tch->x(), tch->y());
        else
            ch->chat_packet(CHAT_TYPE_INFO, "Spawn failed.");
    }

    ACMD(do_mob_aggresive) {
        char arg1[256], arg2[256];

        two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

        if (!*arg1) {
            ch->chat_packet(CHAT_TYPE_INFO, "Usage: mob <mob vnum>");
            return;
        }

        const mob *mob_value = resolve_mob(arg1);

        if (!mob_value) {
            ch->chat_packet(CHAT_TYPE_INFO, "No such mob by that vnum");
            return;
        }

        spawn_around(ch, mob_value, spawn_count(arg2, 20), &character::set_aggressive);
    }

    ACMD(do_mob) {
        char arg1[256], arg2[256];

        two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

        if (!*arg1) {
            ch->chat_packet(CHAT_TYPE_INFO, "Usage: mob <mob vnum>");
            return;
        }

        const mob *mob_value = resolve_mob(arg1);

        if (!mob_value) {
            ch->chat_packet(CHAT_TYPE_INFO, "No such mob by that vnum");
            return;
        }

        spawn_around(ch, mob_value, spawn_count(arg2, metin2::config::test_server() ? 40 : 20));
    }

    ACMD(do_mob_ld) {
        char arg1[256], arg2[256], arg3[256], arg4[256];

        two_arguments(two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2)), arg3, sizeof(arg3), arg4, sizeof(arg4));

        if (!*arg1) {
            ch->chat_packet(CHAT_TYPE_INFO, "Usage: mob <mob vnum>");
            return;
        }

        const mob *mob_value = resolve_mob(arg1);

        if (!mob_value) {
            ch->chat_packet(CHAT_TYPE_INFO, "No such mob by that vnum");
            return;
        }

        int dir = 1;
        int32_t x = ch->x() / 100;
        int32_t y = ch->y() / 100;

        if (*arg2)
            str_to_number(x, arg2);
        if (*arg3)
            str_to_number(y, arg3);
        if (*arg4)
            str_to_number(dir, arg4);

        get_character_manager().spawn_mob(mob_value->table.vnum,
            ch->map_index(),
            x * 100,
            y * 100,
            ch->z(),
            mob_value->table.type == CHAR_TYPE_STONE,
            dir);
    }

    ACMD(do_purge) {
        char arg1[256];
        one_argument(argument, arg1, sizeof(arg1));

        const bool all = *arg1 && !strcmp(arg1, "all");

        sectree *sectree = ch->get_sectree();
        if (sectree) {
            sectree->for_each_around([&](entity *ent) {
                if (!ent->is_type(ENTITY_CHARACTER))
                    return;

                auto *chr = (character *)ent;

                int dist = distance_approx(chr->x() - ch->x(), chr->y() - ch->y());

                if (!all && dist >= 1000)
                    return;

                metin2::log::debug("Purge: {} at distance {:d}", chr->name(), dist);

                if (chr->is_npc() && !chr->is_pet() && chr->rider() == nullptr) {
                    get_character_manager().destroy_character(chr);
                }
            });
        } else
            metin2::log::error("Purge: gm has no sectree (map {:d}, position {:d}x{:d})", ch->map_index(), ch->x(), ch->y());
    }

    ACMD(do_item_purge) {
        int i;
        item *item;

        for (i = 0; i < INVENTORY_AND_EQUIP_SLOT_MAX; ++i) {
            if ((item = ch->inventory_item(i))) {
                get_item_manager().remove_item(item, "PURGE");
                ch->sync_quickslot(QUICKSLOT_TYPE_ITEM, i, 255);
            }
        }
        for (i = 0; i < DRAGON_SOUL_INVENTORY_MAX_NUM; ++i) {
            if ((item = ch->get_item(item_pos(DRAGON_SOUL_INVENTORY, i)))) {
                get_item_manager().remove_item(item, "PURGE");
            }
        }
    }

    ACMD(do_state) {
        char arg1[256];
        character *tch;

        one_argument(argument, arg1, sizeof(arg1));

        if (*arg1) {
            if (arg1[0] == '#') {
                tch = get_character_manager().find(strtoul(arg1 + 1, nullptr, 10));
            } else {
                desc *d = get_desc_manager().find_by_character_name(arg1);

                if (!d)
                    tch = nullptr;
                else
                    tch = d->get_character();
            }
        } else
            tch = ch;

        if (!tch)
            return;

        std::string text = std::format("{}'s State: ", tch->name());

        if (tch->is_position(POS_FIGHTING))
            text += "Battle";
        else if (tch->is_position(POS_DEAD))
            text += "Dead";
        else
            text += "Standing";

        if (ch->get_shop())
            text += ", Shop";

        if (ch->get_exchange())
            text += ", Exchange";

        ch->chat_packet(CHAT_TYPE_INFO, "%s", text.c_str());

        text = std::format("Coordinate {}x{} ({}x{})",
            tch->x(), tch->y(), tch->x() / 100, tch->y() / 100);

        sectree *sec = get_sectree_manager().get(tch->map_index(), tch->x(), tch->y());

        if (sec) {
            map_setting &map_setting = get_sectree_manager().map(tch->map_index())->setting;
            std::format_to(std::back_inserter(text), " MapIndex {} Attribute {:08X} Local Position ({} x {})",
                tch->map_index(), sec->get_attribute(tch->x(), tch->y()), (tch->x() - map_setting.base_x) / 100, (tch->y() - map_setting.base_y) / 100);
        }

        ch->chat_packet(CHAT_TYPE_INFO, "%s", text.c_str());

        ch->chat_packet(CHAT_TYPE_INFO, "LEV %d", tch->level());
        ch->chat_packet(CHAT_TYPE_INFO, "HP %d/%d", tch->hp(), tch->max_hp());
        ch->chat_packet(CHAT_TYPE_INFO, "SP %d/%d", tch->sp(), tch->max_sp());
        ch->chat_packet(CHAT_TYPE_INFO, "ATT %d MAGIC_ATT %d SPD %d CRIT %d%% PENE %d%% ATT_BONUS %d%%",
            tch->point(POINT_ATT_GRADE),
            tch->point(POINT_MAGIC_ATT_GRADE),
            tch->point(POINT_ATT_SPEED),
            tch->point(POINT_CRITICAL_PCT),
            tch->point(POINT_PENETRATE_PCT),
            tch->point(POINT_ATT_BONUS));
        ch->chat_packet(CHAT_TYPE_INFO, "DEF %d MAGIC_DEF %d BLOCK %d%% DODGE %d%% DEF_BONUS %d%%",
            tch->point(POINT_DEF_GRADE),
            tch->point(POINT_MAGIC_DEF_GRADE),
            tch->point(POINT_BLOCK),
            tch->point(POINT_DODGE),
            tch->point(POINT_DEF_BONUS));
        ch->chat_packet(CHAT_TYPE_INFO, "RESISTANCES:");
        ch->chat_packet(CHAT_TYPE_INFO, "   WARR:%3d%% ASAS:%3d%% SURA:%3d%% SHAM:%3d%%",
            tch->point(POINT_RESIST_WARRIOR),
            tch->point(POINT_RESIST_ASSASSIN),
            tch->point(POINT_RESIST_SURA),
            tch->point(POINT_RESIST_SHAMAN));
        ch->chat_packet(CHAT_TYPE_INFO, "   SWORD:%3d%% THSWORD:%3d%% DAGGER:%3d%% BELL:%3d%% FAN:%3d%% BOW:%3d%%",
            tch->point(POINT_RESIST_SWORD),
            tch->point(POINT_RESIST_TWOHAND),
            tch->point(POINT_RESIST_DAGGER),
            tch->point(POINT_RESIST_BELL),
            tch->point(POINT_RESIST_FAN),
            tch->point(POINT_RESIST_BOW));
        ch->chat_packet(CHAT_TYPE_INFO, "   FIRE:%3d%% ELEC:%3d%% MAGIC:%3d%% WIND:%3d%% CRIT:%3d%% PENE:%3d%%",
            tch->point(POINT_RESIST_FIRE),
            tch->point(POINT_RESIST_ELEC),
            tch->point(POINT_RESIST_MAGIC),
            tch->point(POINT_RESIST_WIND),
            tch->point(POINT_RESIST_CRITICAL),
            tch->point(POINT_RESIST_PENETRATE));
        ch->chat_packet(CHAT_TYPE_INFO, "   ICE:%3d%% EARTH:%3d%% DARK:%3d%%",
            tch->point(POINT_RESIST_ICE),
            tch->point(POINT_RESIST_EARTH),
            tch->point(POINT_RESIST_DARK));

        ch->chat_packet(CHAT_TYPE_INFO, "MALL:");
        ch->chat_packet(CHAT_TYPE_INFO, "   ATT:%3d%% DEF:%3d%% EXP:%3d%% ITEMx%d GOLDx%d",
            tch->point(POINT_MALL_ATTBONUS),
            tch->point(POINT_MALL_DEFBONUS),
            tch->point(POINT_MALL_EXPBONUS),
            tch->point(POINT_MALL_ITEMBONUS) / 10,
            tch->point(POINT_MALL_GOLDBONUS) / 10);

        ch->chat_packet(CHAT_TYPE_INFO, "BONUS:");
        ch->chat_packet(CHAT_TYPE_INFO, "   SKILL:%3d%% NORMAL:%3d%% SKILL_DEF:%3d%% NORMAL_DEF:%3d%%",
            tch->point(POINT_SKILL_DAMAGE_BONUS),
            tch->point(POINT_NORMAL_HIT_DAMAGE_BONUS),
            tch->point(POINT_SKILL_DEFEND_BONUS),
            tch->point(POINT_NORMAL_HIT_DEFEND_BONUS));

        ch->chat_packet(CHAT_TYPE_INFO, "   HUMAN:%3d%% ANIMAL:%3d%% ORC:%3d%% MILGYO:%3d%% UNDEAD:%3d%%",
            tch->point(POINT_ATTBONUS_HUMAN),
            tch->point(POINT_ATTBONUS_ANIMAL),
            tch->point(POINT_ATTBONUS_ORC),
            tch->point(POINT_ATTBONUS_MILGYO),
            tch->point(POINT_ATTBONUS_UNDEAD));

        ch->chat_packet(CHAT_TYPE_INFO, "   DEVIL:%3d%% INSECT:%3d%% FIRE:%3d%% ICE:%3d%% DESERT:%3d%%",
            tch->point(POINT_ATTBONUS_DEVIL),
            tch->point(POINT_ATTBONUS_INSECT),
            tch->point(POINT_ATTBONUS_FIRE),
            tch->point(POINT_ATTBONUS_ICE),
            tch->point(POINT_ATTBONUS_DESERT));

        ch->chat_packet(CHAT_TYPE_INFO, "   TREE:%3d%% MONSTER:%3d%%",
            tch->point(POINT_ATTBONUS_TREE),
            tch->point(POINT_ATTBONUS_MONSTER));

        ch->chat_packet(CHAT_TYPE_INFO, "   WARR:%3d%% ASAS:%3d%% SURA:%3d%% SHAM:%3d%%",
            tch->point(POINT_ATTBONUS_WARRIOR),
            tch->point(POINT_ATTBONUS_ASSASSIN),
            tch->point(POINT_ATTBONUS_SURA),
            tch->point(POINT_ATTBONUS_SHAMAN));

        for (int i = 0; i < MAX_PRIV_NUM; ++i)
            if (get_priv_manager().priv(tch, i)) {
                int by_empire = get_priv_manager().priv_by_empire(tch->empire(), i);
                int by_guild = 0;

                if (tch->get_guild())
                    by_guild = get_priv_manager().priv_by_guild(tch->get_guild()->id(), i);

                int by_player = get_priv_manager().priv_by_character(tch->player_id(), i);

                if (by_empire)
                    ch->chat_packet(CHAT_TYPE_INFO, "%s for empire : %d", lc_text(apsz_priv_names[i]), by_empire);

                if (by_guild)
                    ch->chat_packet(CHAT_TYPE_INFO, "%s for guild : %d", lc_text(apsz_priv_names[i]), by_guild);

                if (by_player)
                    ch->chat_packet(CHAT_TYPE_INFO, "%s for player : %d", lc_text(apsz_priv_names[i]), by_player);
            }
    }

    void send_notice(const char *buf) {
        const desc_manager::desc_set &c_ref_set = get_desc_manager().client_set();
        for (desc *d : c_ref_set) {
            if (!d->get_character())
                continue;

            d->get_character()->chat_packet(CHAT_TYPE_NOTICE, "%s", buf);
        }
    }

    void send_monarch_notice(uint8_t empire, const char *buf) {
        const desc_manager::desc_set &c_ref_set = get_desc_manager().client_set();
        for (desc *d : c_ref_set) {
            if (!d->get_character())
                continue;

            if (empire == d->get_character()->empire()) {
                d->get_character()->chat_packet(CHAT_TYPE_NOTICE, "%s", buf);
            }
        }
    }

    void send_notice_map(const char *buf, int map_index, bool big_font) {
        const desc_manager::desc_set &c_ref_set = get_desc_manager().client_set();
        for (desc *d : c_ref_set) {
            if (d->get_character() == nullptr) continue;
            if (d->get_character()->map_index() != map_index) continue;

            d->get_character()->chat_packet(big_font == true ? CHAT_TYPE_BIG_NOTICE : CHAT_TYPE_NOTICE, "%s", buf);
        }
    }

    void send_log(const char *buf) {
        const desc_manager::desc_set &c_ref_set = get_desc_manager().client_set();
        for (desc *d : c_ref_set) {
            if (!d->get_character())
                continue;

            if (d->get_character()->gm_level() > GM_PLAYER)
                d->get_character()->chat_packet(CHAT_TYPE_NOTICE, "%s", buf);
        }
    }

    void broadcast_notice(const char *buf_value) {
        packet_gg_notice p;
        p.header = HEADER_GG_NOTICE;
        p.size = strlen(buf_value) + 1;

        temp_buffer buf;
        buf.write(&p, sizeof(p));
        buf.write(buf_value, p.size);

        get_p2p_manager().send(buf.read_peek(), buf.size());

        send_notice(buf_value);
    }

    void broadcast_monarch_notice(uint8_t empire_value, const char *buf_value) {
        packet_gg_monarch_notice p;
        p.header = HEADER_GG_MONARCH_NOTICE;
        p.empire = empire_value;
        p.size = strlen(buf_value) + 1;

        temp_buffer buf;
        buf.write(&p, sizeof(p));
        buf.write(buf_value, p.size);

        get_p2p_manager().send(buf.read_peek(), buf.size());

        send_monarch_notice(empire_value, buf_value);
    }

    ACMD(do_notice) {
        broadcast_notice(argument);
    }

    ACMD(do_map_notice) {
        send_notice_map(argument, ch->map_index(), false);
    }

    ACMD(do_big_notice) {
        ch->chat_packet(CHAT_TYPE_BIG_NOTICE, "%s", argument);
    }

    ACMD(do_monarch_notice) {
        if (ch->is_monarch() == true) {
            broadcast_monarch_notice(ch->empire(), argument);
        } else {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("군주만이 사용 가능한 기능입니다"));
        }
    }

    ACMD(do_who) {
        int total;
        int *pai_empire_user_count;
        int local;

        get_desc_manager().user_count(total, &pai_empire_user_count, local);

        ch->chat_packet(CHAT_TYPE_INFO, "Total [%d] %d / %d / %d (this server %d)",
            total, pai_empire_user_count[1], pai_empire_user_count[2], pai_empire_user_count[3], local);
    }

    ACMD(do_user) {
        const desc_manager::desc_set &c_ref_set = get_desc_manager().client_set();
        int count = 0;
        std::string str;

        for (desc *d : c_ref_set) {
            if (!d->get_character())
                continue;

            std::format_to(std::back_inserter(str), "{:<16} ", d->get_character()->name());
            ++count;

            if (!(count % 4)) {
                ch->chat_packet(CHAT_TYPE_INFO, str.c_str());

                str.clear();
            }
        }

        if (count % 4)
            ch->chat_packet(CHAT_TYPE_INFO, str.c_str());

        ch->chat_packet(CHAT_TYPE_INFO, "Total %d", count);
    }

    ACMD(do_disconnect) {
        char arg1[256];
        one_argument(argument, arg1, sizeof(arg1));

        if (!*arg1) {
            ch->chat_packet(CHAT_TYPE_INFO, "ex) /dc <player name>");
            return;
        }

        desc *d = get_desc_manager().find_by_character_name(arg1);
        character *tch = d ? d->get_character() : nullptr;

        if (!tch) {
            ch->chat_packet(CHAT_TYPE_INFO, "%s: no such a player.", arg1);
            return;
        }

        if (tch == ch) {
            ch->chat_packet(CHAT_TYPE_INFO, "cannot disconnect myself");
            return;
        }

        get_desc_manager().destroy_desc(d);
    }

    ACMD(do_kill) {
        char arg1[256];
        one_argument(argument, arg1, sizeof(arg1));

        if (!*arg1) {
            ch->chat_packet(CHAT_TYPE_INFO, "ex) /kill <player name>");
            return;
        }

        desc *d = get_desc_manager().find_by_character_name(arg1);
        character *tch = d ? d->get_character() : nullptr;

        if (!tch) {
            ch->chat_packet(CHAT_TYPE_INFO, "%s: no such a player", arg1);
            return;
        }

        tch->dead();
    }

    constexpr int MISC = 0;
    constexpr int BINARY = 1;
    constexpr int NUMBER = 2;
    const struct set_struct {
        const char *cmd;
        const char type;
    } set_fields[] = {
        {"gold",      NUMBER},
        {"race",      BINARY},
        {"sex",       BINARY},
        {"exp",       NUMBER},
        {"max_hp",    NUMBER},
        {"max_sp",    NUMBER},
        {"skill",     NUMBER},
        {"alignment", NUMBER},
        {"align",     NUMBER},
        {"\n",        MISC  }
    };

    ACMD(do_set) {
        char arg1[256], arg2[256], arg3[256];

        character *tch = nullptr;

        int i, len;
        const char *line;

        line = two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));
        one_argument(line, arg3, sizeof(arg3));

        if (!*arg1 || !*arg2 || !*arg3) {
            ch->chat_packet(CHAT_TYPE_INFO, "Usage: set <name> <field> <value>");
            return;
        }

        tch = get_character_manager().find_pc(arg1);

        if (!tch) {
            ch->chat_packet(CHAT_TYPE_INFO, "%s not exist", arg1);
            return;
        }

        len = strlen(arg2);

        for (i = 0; *(set_fields[i].cmd) != '\n'; i++)
            if (!strncmp(arg2, set_fields[i].cmd, len))
                break;

        switch (i) {
        case 0:
            {
                int gold = 0;
                str_to_number(gold, arg3);
                get_db_manager().send_money_log(MONEY_LOG_MISC, 3, gold);
                int before_gold = tch->gold();
                tch->point_change(POINT_GOLD, gold, true);
                int after_gold = tch->gold();
                if (0 == after_gold && 0 != before_gold) {
                    get_log_manager().char_log(tch, gold, "ZERO_GOLD", "GM");
                }
            }
            break;

        case 1:
            break;

        case 2:
            break;

        case 3:
            {
                int amount = 0;
                str_to_number(amount, arg3);
                tch->point_change(POINT_EXP, amount, true);
            }
            break;

        case 4:
            {
                int amount = 0;
                str_to_number(amount, arg3);
                tch->point_change(POINT_MAX_HP, amount, true);
            }
            break;

        case 5:
            {
                int amount = 0;
                str_to_number(amount, arg3);
                tch->point_change(POINT_MAX_SP, amount, true);
            }
            break;

        case 6:
            {
                int amount = 0;
                str_to_number(amount, arg3);
                tch->point_change(POINT_SKILL, amount, true);
            }
            break;

        case 7:
        case 8:
            {
                int amount = 0;
                str_to_number(amount, arg3);
                tch->update_alignment(amount - ch->real_alignment());
            }
            break;
        }

        if (set_fields[i].type == NUMBER) {
            int amount = 0;
            str_to_number(amount, arg3);
            ch->chat_packet(CHAT_TYPE_INFO, "%s's %s set to [%d]", tch->name(), set_fields[i].cmd, amount);
        }
    }

    ACMD(do_reset) {
        ch->point_change(POINT_HP, ch->max_hp() - ch->hp());
        ch->point_change(POINT_SP, ch->max_sp() - ch->sp());
        ch->save();
    }

    ACMD(do_advance) {
        char arg1[256], arg2[256];
        two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

        if (!*arg1 || !*arg2) {
            ch->chat_packet(CHAT_TYPE_INFO, "Syntax: advance <name> <level>");
            return;
        }

        character *tch = get_character_manager().find_pc(arg1);

        if (!tch) {
            ch->chat_packet(CHAT_TYPE_INFO, "%s not exist", arg1);
            return;
        }

        int level = 0;
        str_to_number(level, arg2);

        tch->reset_point(std::clamp(level, 0, PLAYER_MAX_LEVEL_CONST));
    }

    ACMD(do_respawn) {
        char arg1[256];
        one_argument(argument, arg1, sizeof(arg1));

        if (*arg1 && !strcasecmp(arg1, "all")) {
            ch->chat_packet(CHAT_TYPE_INFO, "Respaw everywhere");
            regen_reset(0, 0);
        } else {
            ch->chat_packet(CHAT_TYPE_INFO, "Respaw around");
            regen_reset(ch->x(), ch->y());
        }
    }

    ACMD(do_safebox_size) {

        char arg1[256];
        one_argument(argument, arg1, sizeof(arg1));

        int size = 0;

        if (*arg1)
            str_to_number(size, arg1);

        if (size > 3 || size < 0)
            size = 0;

        ch->chat_packet(CHAT_TYPE_INFO, "Safebox size set to %d", size);
        ch->change_safebox_size(size);
    }

    ACMD(do_makeguild) {
        if (ch->get_guild())
            return;

        guild_manager &gm = get_guild_manager();

        char arg1[256];
        one_argument(argument, arg1, sizeof(arg1));

        guild_create_parameter cp;
        memset(&cp, 0, sizeof(cp));

        cp.master = ch;
        strlcpy(cp.name, arg1, sizeof(cp.name));

        if (!check_name(cp.name)) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("적합하지 않은 길드 이름 입니다."));
            return;
        }

        gm.create_guild(cp);
        ch->chat_packet(CHAT_TYPE_INFO, lc_text("(%s) 길드가 생성되었습니다. [임시]"), cp.name);
    }

    ACMD(do_deleteguild) {
        if (ch->get_guild())
            ch->get_guild()->request_disband(ch->player_id());
    }

    ACMD(do_greset) {
        if (ch->get_guild())
            ch->get_guild()->reset();
    }

    ACMD(do_refine_rod) {
        char arg1[256];
        one_argument(argument, arg1, sizeof(arg1));

        uint8_t cell = 0;
        str_to_number(cell, arg1);
        item *item = ch->inventory_item(cell);
        if (item)
            fishing::real_refine_rod(ch, item);
    }

    ACMD(do_refine_pick) {
        char arg1[256];
        one_argument(argument, arg1, sizeof(arg1));

        uint8_t cell = 0;
        str_to_number(cell, arg1);
        item *item = ch->inventory_item(cell);
        if (item) {
            mining::cheat_max_pick(ch, item);
            mining::real_refine_pick(ch, item);
        }
    }

    ACMD(do_max_pick) {
        char arg1[256];
        one_argument(argument, arg1, sizeof(arg1));

        uint8_t cell = 0;
        str_to_number(cell, arg1);
        item *item = ch->inventory_item(cell);
        if (item) {
            mining::cheat_max_pick(ch, item);
        }
    }

    ACMD(do_fishing_simul) {
        char arg1[256];
        char arg2[256];
        char arg3[256];
        argument = one_argument(argument, arg1, sizeof(arg1));
        two_arguments(argument, arg2, sizeof(arg2), arg3, sizeof(arg3));

        int count = 1000;
        int prob_idx = 0;
        int level = 100;

        ch->chat_packet(CHAT_TYPE_INFO, "Usage: fishing_simul <level> <prob index> <count>");

        if (*arg1)
            str_to_number(level, arg1);

        if (*arg2)
            str_to_number(prob_idx, arg2);

        if (*arg3)
            str_to_number(count, arg3);

        fishing::simulation(level, count, prob_idx, ch);
    }

    ACMD(do_invisibility) {
        if (ch->is_affect_flag(AFF_INVISIBILITY)) {
            ch->remove_affect(AFFECT_INVISIBILITY);
        } else {
            ch->add_affect(AFFECT_INVISIBILITY, POINT_NONE, 0, AFF_INVISIBILITY, INFINITE_AFFECT_DURATION, 0, true);
        }
    }

    ACMD(do_event_flag) {
        char arg1[256];
        char arg2[256];

        two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

        if (!(*arg1) || !(*arg2))
            return;

        int value = 0;
        str_to_number(value, arg2);

        if (!strcmp(arg1, "mob_item") ||
            !strcmp(arg1, "mob_exp") ||
            !strcmp(arg1, "mob_gold") ||
            !strcmp(arg1, "mob_dam") ||
            !strcmp(arg1, "mob_gold_pct") ||
            !strcmp(arg1, "mob_item_buyer") ||
            !strcmp(arg1, "mob_exp_buyer") ||
            !strcmp(arg1, "mob_gold_buyer") ||
            !strcmp(arg1, "mob_gold_pct_buyer"))
            value = std::clamp(value, 0, 1000);

        quest::get_quest_manager().request_set_event_flag(arg1, value);
        ch->chat_packet(CHAT_TYPE_INFO, "RequestSetEventFlag %s %d", arg1, value);
        metin2::log::info("Gm {} set event flag {} to {:d}", ch->name(), arg1, value);
    }

    ACMD(do_get_event_flag) {
        quest::get_quest_manager().send_event_flag_list(ch);
    }

    ACMD(do_private) {
        char arg1[256];
        one_argument(argument, arg1, sizeof(arg1));

        if (!*arg1) {
            ch->chat_packet(CHAT_TYPE_INFO, "Usage: private <map index>");
            return;
        }

        int32_t map_index_value;
        int32_t map_index = 0;
        str_to_number(map_index, arg1);
        if ((map_index_value = get_sectree_manager().create_private_map(map_index))) {
            ch->save_exit_location();

            sectree_map *sectree_map = get_sectree_manager().map(map_index_value);
            ch->warp_set(sectree_map->setting.pos_spawn.x, sectree_map->setting.pos_spawn.y, map_index_value);
        } else
            ch->chat_packet(CHAT_TYPE_INFO, "Can't find map by index %d", map_index);
    }

    ACMD(do_qf) {
        char arg1[256];

        one_argument(argument, arg1, sizeof(arg1));

        if (!*arg1)
            return;

        quest::pc *pc = quest::get_quest_manager().pc_force(ch->player_id());
        std::string questname = pc->current_quest_name();

        if (!questname.empty()) {
            int value = quest::get_quest_manager().quest_state_index(questname, arg1);

            pc->set_flag(questname + ".__status", value);
            pc->clear_timer();

            auto it = pc->quest_begin();
            unsigned int questindex = quest::get_quest_manager().quest_index_by_name(questname);

            while (it != pc->quest_end()) {
                if (it->first == questindex) {
                    it->second.st = value;
                    break;
                }

                ++it;
            }

            ch->chat_packet(CHAT_TYPE_INFO, "setting quest state flag %s %s %d", questname.c_str(), arg1, value);
        } else {
            ch->chat_packet(CHAT_TYPE_INFO, "setting quest state flag failed");
        }
    }

    ACMD(do_book) {
        char arg1[256];

        one_argument(argument, arg1, sizeof(arg1));

        skill_proto *proto;

        if (is_digit(*arg1)) {
            uint32_t vnum = 0;
            str_to_number(vnum, arg1);
            proto = get_skill_manager().get(vnum);
        } else
            proto = get_skill_manager().get(arg1);

        if (!proto) {
            ch->chat_packet(CHAT_TYPE_INFO, "There is no such a skill.");
            return;
        }

        item *item = ch->auto_give_item(50300);
        item->set_socket(0, proto->vnum);
    }

    ACMD(do_setskillother) {
        char arg1[256], arg2[256], arg3[256];
        argument = two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));
        one_argument(argument, arg3, sizeof(arg3));

        if (!*arg1 || !*arg2 || !*arg3 || !is_digit(*arg3)) {
            ch->chat_packet(CHAT_TYPE_INFO, "Syntax: setskillother <target> <skillname> <lev>");
            return;
        }

        character *tch = find_target_or_self(ch, arg1);

        if (!tch)
            return;

        skill_proto *pk;

        if (is_digit(*arg2)) {
            uint32_t vnum = 0;
            str_to_number(vnum, arg2);
            pk = get_skill_manager().get(vnum);
        } else
            pk = get_skill_manager().get(arg2);

        if (!pk) {
            ch->chat_packet(CHAT_TYPE_INFO, "No such a skill by that name.");
            return;
        }

        uint8_t level = 0;
        str_to_number(level, arg3);
        tch->set_skill_level(pk->vnum, level);
        tch->compute_points();
        tch->skill_level_packet();
    }

    ACMD(do_setskill) {
        char arg1[256], arg2[256];
        two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

        if (!*arg1 || !*arg2 || !is_digit(*arg2)) {
            ch->chat_packet(CHAT_TYPE_INFO, "Syntax: setskill <name> <lev>");
            return;
        }

        skill_proto *pk;

        if (is_digit(*arg1)) {
            uint32_t vnum = 0;
            str_to_number(vnum, arg1);
            pk = get_skill_manager().get(vnum);
        }

        else
            pk = get_skill_manager().get(arg1);

        if (!pk) {
            ch->chat_packet(CHAT_TYPE_INFO, "No such a skill by that name.");
            return;
        }

        uint8_t level = 0;
        str_to_number(level, arg2);
        ch->set_skill_level(pk->vnum, level);
        ch->compute_points();
        ch->skill_level_packet();
    }

    ACMD(do_set_skill_point) {
        char arg1[256];
        one_argument(argument, arg1, sizeof(arg1));

        int skill_point = 0;
        if (*arg1)
            str_to_number(skill_point, arg1);

        ch->set_real_point(POINT_SKILL, skill_point);
        ch->set_point(POINT_SKILL, ch->real_point(POINT_SKILL));
        ch->point_change(POINT_SKILL, 0);
    }

    ACMD(do_set_skill_group) {
        char arg1[256];
        one_argument(argument, arg1, sizeof(arg1));

        int skill_group = 0;
        if (*arg1)
            str_to_number(skill_group, arg1);

        ch->set_skill_group(skill_group);

        ch->clear_skill();
        ch->chat_packet(CHAT_TYPE_INFO, "skill group to %d.", skill_group);
    }

    ACMD(do_reload) {
        char arg1[256];
        one_argument(argument, arg1, sizeof(arg1));

        if (*arg1) {
            switch (std::tolower(static_cast<unsigned char>(*arg1))) {
            case 'u':
                ch->chat_packet(CHAT_TYPE_INFO, "Reloading state_user_count.");
                load_state_user_count();
                break;

            case 'p':
                ch->chat_packet(CHAT_TYPE_INFO, "Reloading prototype tables,");
                db_clientdesc->db_packet(HEADER_GD_RELOAD_PROTO, 0, nullptr, 0);
                break;

            case 's':
                ch->chat_packet(CHAT_TYPE_INFO, "Reloading notice string.");
                get_db_manager().load_db_string();
                break;

            case 'q':
                ch->chat_packet(CHAT_TYPE_INFO, "Reloading quest.");
                quest::get_quest_manager().reload();
                break;

            case 'f':
                fishing::initialize();
                break;

            case 'a':
                ch->chat_packet(CHAT_TYPE_INFO, "Reloading Admin infomation.");
                db_clientdesc->db_packet(HEADER_GD_RELOAD_ADMIN, 0, nullptr, 0);
                metin2::log::info("Gm {} reloads the gm list", ch->name());
                break;
            case 'c':
                cube_init();
                break;
            }
        } else {
            ch->chat_packet(CHAT_TYPE_INFO, "Reloading state_user_count.");
            load_state_user_count();

            ch->chat_packet(CHAT_TYPE_INFO, "Reloading prototype tables,");
            db_clientdesc->db_packet(HEADER_GD_RELOAD_PROTO, 0, nullptr, 0);

            ch->chat_packet(CHAT_TYPE_INFO, "Reloading notice string.");
            get_db_manager().load_db_string();
        }
    }

    ACMD(do_cooltime) {
        ch->disable_cooltime();
    }

    ACMD(do_level) {
        char arg2[256];
        one_argument(argument, arg2, sizeof(arg2));

        if (!*arg2) {
            ch->chat_packet(CHAT_TYPE_INFO, "Syntax: level <level>");
            return;
        }

        int level = 0;
        str_to_number(level, arg2);
        ch->reset_point(std::clamp(level, 1, PLAYER_MAX_LEVEL_CONST));

        ch->clear_skill();
        ch->clear_sub_skill();
    }

    ACMD(do_gwlist) {
        ch->chat_packet(CHAT_TYPE_NOTICE, lc_text("현재 전쟁중인 길드 입니다"));
        get_guild_manager().show_guild_war_list(ch);
    }

    ACMD(do_stop_guild_war) {
        char arg1[256], arg2[256];
        two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

        if (!*arg1 || !*arg2)
            return;

        int id1 = 0, id2 = 0;

        str_to_number(id1, arg1);
        str_to_number(id2, arg2);

        if (!id1 || !id2)
            return;

        if (id1 > id2) {
            std::swap(id1, id2);
        }

        ch->chat_packet(CHAT_TYPE_TALKING, "%d %d", id1, id2);
        get_guild_manager().request_end_war(id1, id2);
    }

    ACMD(do_cancel_guild_war) {
        char arg1[256], arg2[256];
        two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

        int id1 = 0, id2 = 0;
        str_to_number(id1, arg1);
        str_to_number(id2, arg2);

        if (id1 > id2)
            std::swap(id1, id2);

        get_guild_manager().request_cancel_war(id1, id2);
    }

    ACMD(do_guild_state) {
        char arg1[256];
        one_argument(argument, arg1, sizeof(arg1));

        guild *guild = get_guild_manager().find_guild_by_name(arg1);
        if (guild != nullptr) {
            ch->chat_packet(CHAT_TYPE_INFO, "GuildID: %d", guild->id());
            ch->chat_packet(CHAT_TYPE_INFO, "GuildMasterPID: %d", guild->master_pid());
            ch->chat_packet(CHAT_TYPE_INFO, "IsInWar: %d", guild->under_any_war());
        } else {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("%s: 존재하지 않는 길드 입니다."), arg1);
        }
    }

    ACMD(do_weaken) {
        char arg1[256];
        one_argument(argument, arg1, sizeof(arg1));

        const bool all = *arg1 && !strcmp(arg1, "all");

        ch->get_sectree()->for_each_around([&](entity *ent) {
            if (!ent->is_type(ENTITY_CHARACTER))
                return;

            auto *chr = (character *)ent;

            int dist = distance_approx(chr->x() - ch->x(), chr->y() - ch->y());

            if (!all && dist >= 1000)
                return;

            if (chr->is_npc())
                chr->point_change(POINT_HP, (10 - chr->hp()));
        });
    }

    ACMD(do_getqf) {
        char arg1[256];

        one_argument(argument, arg1, sizeof(arg1));

        character *tch = find_target_or_self(ch, arg1);

        if (!tch)
            return;

        quest::pc *pc = quest::get_quest_manager().get_pc(tch->player_id());

        if (pc)
            pc->send_flag_list(ch);
    }

    ACMD(do_set_state) {
        char arg1[256];
        char arg2[256];

        two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

        if (!*arg1 || !*arg2)
            return;

        quest::pc *pc = quest::get_quest_manager().pc_force(ch->player_id());
        std::string questname = arg1;
        std::string statename = arg2;

        if (!questname.empty()) {
            int value = quest::get_quest_manager().quest_state_index(questname, statename);

            pc->set_flag(questname + ".__status", value);
            pc->clear_timer();

            auto it = pc->quest_begin();
            unsigned int questindex = quest::get_quest_manager().quest_index_by_name(questname);

            while (it != pc->quest_end()) {
                if (it->first == questindex) {
                    it->second.st = value;
                    break;
                }

                ++it;
            }

            ch->chat_packet(CHAT_TYPE_INFO, "setting quest state flag %s %s %d", questname.c_str(), arg1, value);
        } else {
            ch->chat_packet(CHAT_TYPE_INFO, "setting quest state flag failed");
        }
    }

    ACMD(do_setqf) {
        char arg1[256];
        char arg2[256];
        char arg3[256];

        one_argument(two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2)), arg3, sizeof(arg3));

        if (!*arg1) {
            ch->chat_packet(CHAT_TYPE_INFO, "Syntax: setqf <flagname> <value> [<character name>]");
            return;
        }

        character *tch = find_target_or_self(ch, arg3);

        if (!tch)
            return;

        quest::pc *pc = quest::get_quest_manager().get_pc(tch->player_id());

        if (pc) {
            int value = 0;
            str_to_number(value, arg2);
            pc->set_flag(arg1, value);
            ch->chat_packet(CHAT_TYPE_INFO, "Quest flag set: %s %d", arg1, value);
        }
    }

    ACMD(do_delqf) {
        char arg1[256];
        char arg2[256];

        two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

        if (!*arg1) {
            ch->chat_packet(CHAT_TYPE_INFO, "Syntax: delqf <flagname> [<character name>]");
            return;
        }

        character *tch = find_target_or_self(ch, arg2);

        if (!tch)
            return;

        quest::pc *pc = quest::get_quest_manager().get_pc(tch->player_id());

        if (pc) {
            if (pc->delete_flag(arg1))
                ch->chat_packet(CHAT_TYPE_INFO, "Delete success.");
            else
                ch->chat_packet(CHAT_TYPE_INFO, "Delete failed. Quest flag does not exist.");
        }
    }

    ACMD(do_forgetme) {
        ch->forget_my_attacker();
    }

    ACMD(do_aggregate) {
        ch->aggregate_monster();
    }

    ACMD(do_attract_ranger) {
        ch->attract_ranger();
    }

    ACMD(do_pull_monster) {
        ch->pull_monster();
    }

    ACMD(do_polymorph) {
        char arg1[256], arg2[256];

        two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));
        if (*arg1) {
            uint32_t vnum = 0;
            str_to_number(vnum, arg1);
            bool maintain_stat = false;
            if (*arg2) {
                int value = 0;
                str_to_number(value, arg2);
                maintain_stat = (value > 0);
            }

            ch->set_polymorph(vnum, maintain_stat);
        }
    }

    ACMD(do_polymorph_item) {
        char arg1[256];

        one_argument(argument, arg1, sizeof(arg1));

        if (*arg1) {
            uint32_t vnum_value = 0;
            str_to_number(vnum_value, arg1);

            item *item = get_item_manager().create_item(70104, 1, 0, true);
            if (item) {
                item->set_socket(0, vnum_value);
                int empty_pos = ch->empty_inventory(item->size());

                if (empty_pos != -1) {
                    item->add_to_character(ch, item_pos(INVENTORY, empty_pos));
                    get_log_manager().item_log(ch, item, "GM", item->name());
                } else {
                    get_item_manager().destroy_item(item);
                    ch->chat_packet(CHAT_TYPE_INFO, "Not enough inventory space.");
                }
            } else {
                ch->chat_packet(CHAT_TYPE_INFO, "#%d item not exist by that vnum.", 70103);
            }
        }
    }

    ACMD(do_priv_empire) {
        char arg1[256] = {0};
        char arg2[256] = {0};
        char arg3[256] = {0};
        char arg4[256] = {0};
        int empire = 0;
        int type = 0;
        int value = 0;
        int duration = 0;

        const char *line = two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

        if (!*arg1 || !*arg2)
            goto USAGE;

        if (!line)
            goto USAGE;

        two_arguments(line, arg3, sizeof(arg3), arg4, sizeof(arg4));

        if (!*arg3 || !*arg4)
            goto USAGE;

        str_to_number(empire, arg1);
        str_to_number(type, arg2);
        str_to_number(value, arg3);
        value = std::clamp(value, 0, 1000);
        str_to_number(duration, arg4);

        if (empire < 0 || 3 < empire)
            goto USAGE;

        if (type < 1 || 4 < type)
            goto USAGE;

        if (value < 0)
            goto USAGE;

        if (duration < 0)
            goto USAGE;

        duration = duration * (60 * 60);

        metin2::log::info("Gm {} grants empire {:d} privilege type {:d}, value {:d}, for {:d} seconds", ch->name(),
            empire, type, value, duration);
        get_priv_manager().request_give_empire_priv(empire, type, value, duration);
        return;

    USAGE:
        ch->chat_packet(CHAT_TYPE_INFO, "usage : priv_empire <empire> <type> <value> <duration>");
        ch->chat_packet(CHAT_TYPE_INFO, "  <empire>    0 - 3 (0==all)");
        ch->chat_packet(CHAT_TYPE_INFO, "  <type>      1:item_drop, 2:gold_drop, 3:gold10_drop, 4:exp");
        ch->chat_packet(CHAT_TYPE_INFO, "  <value>     percent");
        ch->chat_packet(CHAT_TYPE_INFO, "  <duration>  hour");
    }

    ACMD(do_priv_guild) {
        char arg1[256];
        one_argument(argument, arg1, sizeof(arg1));

        if (*arg1) {
            guild *g = get_guild_manager().find_guild_by_name(arg1);

            if (!g) {
                uint32_t guild_id = 0;
                str_to_number(guild_id, arg1);
                g = get_guild_manager().find_guild(guild_id);
            }

            if (!g)
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("그런 이름 또는 번호의 길드가 없습니다."));
            else {
                const char buf[] = "";

                using namespace quest;
                pc *pc = get_quest_manager().get_pc(ch->player_id());
                quest_state qs = get_quest_manager().open_state("ADMIN_QUEST", QUEST_FISH_REFINE_STATE_INDEX);
                luaL_loadbuffer(qs.co, buf, strlen(buf), "ADMIN_QUEST");
                pc->set_quest("ADMIN_QUEST", qs);

                quest_state &rqs = *pc->running_quest_state();

                if (!get_quest_manager().run_state(rqs)) {
                    get_quest_manager().close_state(rqs);
                    pc->end_running();
                    return;
                }
            }
        }
    }

    ACMD(do_mount_test) {
        char arg1[256];

        one_argument(argument, arg1, sizeof(arg1));

        if (*arg1) {
            uint32_t vnum = 0;
            str_to_number(vnum, arg1);
            ch->mount_vnum(vnum);
        }
    }

    ACMD(do_observer) {
        ch->set_observer_mode(!ch->is_observer_mode());
    }

    ACMD(do_socket_item) {
        char arg1[256], arg2[256];
        two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

        if (*arg1) {
            uint32_t vnum_value = 0;
            str_to_number(vnum_value, arg1);

            int socket_count = 0;
            str_to_number(socket_count, arg2);

            if (!socket_count || socket_count >= ITEM_SOCKET_MAX_NUM)
                socket_count = 3;

            if (!vnum_value) {
                if (!get_item_manager().vnum(arg1, vnum_value)) {
                    ch->chat_packet(CHAT_TYPE_INFO, "#%d item not exist by that vnum.", vnum_value);
                    return;
                }
            }

            item *item = ch->auto_give_item(vnum_value);

            if (item) {
                for (int i = 0; i < socket_count; ++i)
                    item->set_socket(i, 1);
            } else {
                ch->chat_packet(CHAT_TYPE_INFO, "#%d cannot create item.", vnum_value);
            }
        }
    }

    ACMD(do_xmas) {
        char arg1[256];
        one_argument(argument, arg1, sizeof(arg1));

        int flag = 0;

        if (*arg1)
            str_to_number(flag, arg1);

        switch (subcmd) {
        case SCMD_XMAS_SNOW:
            quest::get_quest_manager().request_set_event_flag("xmas_snow", flag);
            break;

        case SCMD_XMAS_BOOM:
            quest::get_quest_manager().request_set_event_flag("xmas_boom", flag);
            break;

        case SCMD_XMAS_SANTA:
            quest::get_quest_manager().request_set_event_flag("xmas_santa", flag);
            break;
        }
    }

    ACMD(do_block_chat_list) {
        if (!ch || (ch->gm_level() < GM_HIGH_WIZARD && ch->quest_flag("chat_privilege.block") <= 0)) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("그런 명령어는 없습니다"));
            return;
        }

        get_db_manager().func_query([pid = ch->player_id()](sql_msg &msg) {
            character *ch = get_character_manager().find_by_pid(pid);

            if (ch == nullptr)
                return;
            if (msg.get()->num_rows) {
                MYSQL_ROW row;
                while ((row = mysql_fetch_row(msg.get()->res.get()))) {
                    ch->chat_packet(CHAT_TYPE_INFO, "%s %s sec", row[0], row[1]);
                }
            } else {
                ch->chat_packet(CHAT_TYPE_INFO, "No one currently blocked.");
            }
        },
            sql::format("SELECT p.name, a.lDuration FROM affect as a, player as p WHERE a.bType = {} AND a.dwPID = p.id", AFFECT_BLOCK_CHAT));
    }

    ACMD(do_vote_block_chat) {
        return;

        char arg1[256];
        argument = one_argument(argument, arg1, sizeof(arg1));

        if (!*arg1) {
            ch->chat_packet(CHAT_TYPE_INFO, "Usage: vote_block_chat <name>");
            return;
        }

        const char *name = arg1;
        int32_t block_duration = 10;
        metin2::log::info("Vote chat block of player {} for {:d} seconds", name, block_duration);

        character *tch = get_character_manager().find_pc(name);

        if (!tch) {
            cci *cci = get_p2p_manager().find(name);

            if (cci) {
                packet_gg_block_chat p;

                p.header = HEADER_GG_BLOCK_CHAT;
                strlcpy(p.name, name, sizeof(p.name));
                p.block_duration = block_duration;
                get_p2p_manager().send(&p, sizeof(packet_gg_block_chat));
            } else {
                packet_block_chat p;

                strlcpy(p.name, name, sizeof(p.name));
                p.duration = block_duration;
                db_clientdesc->db_packet(HEADER_GD_BLOCK_CHAT, ch ? ch->get_desc()->handle() : 0, &p, sizeof(p));
            }

            if (ch)
                ch->chat_packet(CHAT_TYPE_INFO, "Chat block requested.");

            return;
        }

        if (tch && ch != tch)
            tch->add_affect(AFFECT_BLOCK_CHAT, POINT_NONE, 0, AFF_NONE, block_duration, 0, true);
    }

    ACMD(do_block_chat) {
        if (ch && (ch->gm_level() < GM_HIGH_WIZARD && ch->quest_flag("chat_privilege.block") <= 0)) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("그런 명령어는 없습니다"));
            return;
        }

        char arg1[256];
        argument = one_argument(argument, arg1, sizeof(arg1));

        if (!*arg1) {
            if (ch)
                ch->chat_packet(CHAT_TYPE_INFO, "Usage: block_chat <name> <time> (0 to off)");

            return;
        }

        const char *name = arg1;
        int32_t block_duration = parse_time_str(argument);

        if (block_duration < 0) {
            if (ch) {
                ch->chat_packet(CHAT_TYPE_INFO, "잘못된 형식의 시간입니다. h, m, s를 붙여서 지정해 주십시오.");
                ch->chat_packet(CHAT_TYPE_INFO, "예) 10s, 10m, 1m 30s");
            }
            return;
        }

        metin2::log::info("Chat block of player {} for {:d} seconds", name, block_duration);

        character *tch = get_character_manager().find_pc(name);

        if (!tch) {
            cci *cci = get_p2p_manager().find(name);

            if (cci) {
                packet_gg_block_chat p;

                p.header = HEADER_GG_BLOCK_CHAT;
                strlcpy(p.name, name, sizeof(p.name));
                p.block_duration = block_duration;
                get_p2p_manager().send(&p, sizeof(packet_gg_block_chat));
            } else {
                packet_block_chat p;

                strlcpy(p.name, name, sizeof(p.name));
                p.duration = block_duration;
                db_clientdesc->db_packet(HEADER_GD_BLOCK_CHAT, ch ? ch->get_desc()->handle() : 0, &p, sizeof(p));
            }

            if (ch)
                ch->chat_packet(CHAT_TYPE_INFO, "Chat block requested.");

            return;
        }

        if (tch && ch != tch)
            tch->add_affect(AFFECT_BLOCK_CHAT, POINT_NONE, 0, AFF_NONE, block_duration, 0, true);
    }

    ACMD(do_build) {
        using namespace building;

        char arg1[256], arg2[256], arg3[256], arg4[256];
        const char *line = one_argument(argument, arg1, sizeof(arg1));
        uint8_t gm_level = ch->gm_level();

        land *land_value = get_manager().find_land(ch->map_index(), ch->x(), ch->y());

        if (!land_value) {
            metin2::log::warn("Player {} tried to build outside a building area", ch->name());
            return;
        }

        if (!*arg1) {
            ch->chat_packet(CHAT_TYPE_INFO, "Invalid syntax: no command");
            return;
        }

        if (gm_level == GM_PLAYER) {
            if ((!ch->get_guild() || ch->get_guild()->id() != land_value->owner())) {
                metin2::log::warn("Player {} tried to build on land not owned by their guild", ch->name());
                return;
            }

            if (ch->get_guild()->master_pid() != ch->player_id()) {
                metin2::log::warn("Player {} tried to build without being guild master", ch->name());
                return;
            }
        }

        switch (std::tolower(static_cast<unsigned char>(*arg1))) {
        case 'c':
            {
                char arg5[256], arg6[256];
                line = one_argument(two_arguments(line, arg1, sizeof(arg1), arg2, sizeof(arg2)), arg3, sizeof(arg3));
                one_argument(two_arguments(line, arg4, sizeof(arg4), arg5, sizeof(arg5)), arg6, sizeof(arg6));

                if (!*arg1 || !*arg2 || !*arg3 || !*arg4 || !*arg5 || !*arg6) {
                    ch->chat_packet(CHAT_TYPE_INFO, "Invalid syntax");
                    return;
                }

                uint32_t vnum = 0;
                str_to_number(vnum, arg1);

                using namespace building;

                const object_proto *t = get_manager().object_proto(vnum);
                if (!t) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("존재하지 않는 건물입니다."));
                    return;
                }

                const uint32_t building_max_price = 100000000;

                if (t->group_vnum) {
                    if (land_value->find_object_by_group(t->group_vnum)) {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("같이 지을 수 없는 종류의 건물이 지어져 있습니다."));
                        return;
                    }
                }

                if (t->depend_on_group_vnum) {
                    {
                        if (!land_value->find_object_by_group(t->depend_on_group_vnum)) {
                            ch->chat_packet(CHAT_TYPE_INFO, lc_text("건설에 필요한 건물이 지어져 있지 않습니다."));
                            return;
                        }
                    }
                }

                if (metin2::config::test_server() || gm_level == GM_PLAYER) {
                    if (t->price > building_max_price) {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("건물 비용 정보 이상으로 건설 작업에 실패했습니다."));
                        return;
                    }

                    if (ch->gold() < (int)t->price) {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("건설 비용이 부족합니다."));
                        return;
                    }

                    int i;
                    for (i = 0; i < OBJECT_MATERIAL_MAX_NUM; ++i) {
                        uint32_t item_vnum = t->materials[i].item_vnum;
                        uint32_t item_count = t->materials[i].count;

                        if (item_vnum == 0)
                            break;

                        if ((int)item_count > ch->count_specify_item(item_vnum)) {
                            ch->chat_packet(CHAT_TYPE_INFO, lc_text("자재가 부족하여 건설할 수 없습니다."));
                            return;
                        }
                    }
                }

                float x_rot = atof(arg4);
                float y_rot = atof(arg5);
                float z_rot = atof(arg6);

                int32_t map_x = 0;
                str_to_number(map_x, arg2);
                int32_t map_y = 0;
                str_to_number(map_y, arg3);

                bool is_success = land_value->request_create_object(vnum,
                    ch->map_index(),
                    map_x,
                    map_y,
                    x_rot,
                    y_rot,
                    z_rot, true);

                if (!is_success) {
                    if (metin2::config::test_server())
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("건물을 지을 수 없는 위치입니다."));
                    return;
                }

                if (metin2::config::test_server() || gm_level == GM_PLAYER) {
                    ch->point_change(POINT_GOLD, -t->price);

                    {
                        int i;
                        for (i = 0; i < OBJECT_MATERIAL_MAX_NUM; ++i) {
                            uint32_t item_vnum = t->materials[i].item_vnum;
                            uint32_t item_count = t->materials[i].count;

                            if (item_vnum == 0)
                                break;

                            metin2::log::debug("Build: material {:d} is item {:d} x {:d}", i, item_vnum, item_count);
                            ch->remove_specify_item(item_vnum, item_count);
                        }
                    }
                }
            }
            break;

        case 'd':
            {
                one_argument(line, arg1, sizeof(arg1));

                if (!*arg1) {
                    ch->chat_packet(CHAT_TYPE_INFO, "Invalid syntax");
                    return;
                }

                uint32_t vid = 0;
                str_to_number(vid, arg1);
                land_value->request_delete_object_by_vid(vid);
            }
            break;

        case 'w':
            if (gm_level > GM_PLAYER) {
                int map_index = ch->map_index();

                one_argument(line, arg1, sizeof(arg1));

                metin2::log::info("Guild wall build on map {:d}, direction {}", map_index, arg1);

                switch (arg1[0]) {
                case 's':
                    land_value->request_create_wall(map_index, 0.0f);
                    break;
                case 'n':
                    land_value->request_create_wall(map_index, 180.0f);
                    break;
                case 'e':
                    land_value->request_create_wall(map_index, 90.0f);
                    break;
                case 'w':
                    land_value->request_create_wall(map_index, 270.0f);
                    break;
                default:
                    ch->chat_packet(CHAT_TYPE_INFO, "guild.wall.build unknown_direction[%s]", arg1);
                    metin2::log::warn("Guild wall build with unknown direction {}", arg1);
                    break;
                }
            }
            break;

        case 'e':
            if (gm_level > GM_PLAYER) {
                land_value->request_delete_wall();
            }
            break;

        case 'W':

            if (gm_level > GM_PLAYER) {
                int set_id = 0, wall_size = 0;
                char arg5[256], arg6[256];
                line = two_arguments(line, arg1, sizeof(arg1), arg2, sizeof(arg2));
                line = two_arguments(line, arg3, sizeof(arg3), arg4, sizeof(arg4));
                two_arguments(line, arg5, sizeof(arg5), arg6, sizeof(arg6));

                str_to_number(set_id, arg1);
                str_to_number(wall_size, arg2);

                if (set_id != 14105 && set_id != 14115 && set_id != 14125) {
                    metin2::log::warn("Wall build with invalid wall set id {:d}", set_id);
                    break;
                } else {
                    bool door_east = false;
                    str_to_number(door_east, arg3);
                    bool door_west = false;
                    str_to_number(door_west, arg4);
                    bool door_south = false;
                    str_to_number(door_south, arg5);
                    bool door_north = false;
                    str_to_number(door_north, arg6);
                    land_value->request_create_wall_blocks(set_id, ch->map_index(), wall_size, door_east, door_west, door_south, door_north);
                }
            }
            break;

        case 'E':
            if (gm_level > GM_PLAYER) {
                one_argument(line, arg1, sizeof(arg1));
                uint32_t id = 0;
                str_to_number(id, arg1);
                land_value->request_delete_wall_blocks(id);
            }
            break;

        default:
            ch->chat_packet(CHAT_TYPE_INFO, "Invalid command %s", arg1);
            break;
        }
    }

    ACMD(do_clear_quest) {
        char arg1[256];

        one_argument(argument, arg1, sizeof(arg1));

        if (!*arg1)
            return;

        quest::pc *pc = quest::get_quest_manager().pc_force(ch->player_id());
        pc->clear_quest(arg1);
    }

    ACMD(do_horse_state) {
        ch->chat_packet(CHAT_TYPE_INFO, "Horse Information:");
        ch->chat_packet(CHAT_TYPE_INFO, "    Level  %d", ch->horse_level());
        ch->chat_packet(CHAT_TYPE_INFO, "    Health %d/%d (%d%%)", ch->horse_health(), ch->horse_max_health(), ch->horse_health() * 100 / ch->horse_max_health());
        ch->chat_packet(CHAT_TYPE_INFO, "    Stam   %d/%d (%d%%)", ch->horse_stamina(), ch->horse_max_stamina(), ch->horse_stamina() * 100 / ch->horse_max_stamina());
    }

    ACMD(do_horse_level) {
        char arg1[256] = {0};
        char arg2[256] = {0};
        character *victim;
        int level = 0;

        two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

        if (!*arg1 || !*arg2) {
            ch->chat_packet(CHAT_TYPE_INFO, "usage : /horse_level <name> <level>");
            return;
        }

        victim = get_character_manager().find_pc(arg1);

        if (nullptr == victim) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("존재하지 않는 캐릭터 입니다."));
            return;
        }

        str_to_number(level, arg2);
        level = std::clamp(level, 0, HORSE_MAX_LEVEL);

        ch->chat_packet(CHAT_TYPE_INFO, "horse level set (%s: %d)", victim->name(), level);

        victim->set_horse_level(level);
        victim->compute_points();
        victim->skill_level_packet();
        return;
    }

    ACMD(do_horse_ride) {
        if (ch->is_horse_riding())
            ch->stop_riding();
        else
            ch->start_riding();
    }

    ACMD(do_horse_summon) {
        ch->horse_summon(true, true);
    }

    ACMD(do_horse_unsummon) {
        ch->horse_summon(false, true);
    }

    ACMD(do_horse_set_stat) {
        char arg1[256], arg2[256];

        two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

        if (*arg1 && *arg2) {
            int hp = 0;
            str_to_number(hp, arg1);
            int stam = 0;
            str_to_number(stam, arg2);
            ch->update_horse_health(hp - ch->horse_health());
            ch->update_horse_stamina(stam - ch->horse_stamina());
        } else {
            ch->chat_packet(CHAT_TYPE_INFO, "Usage : /horse_set_stat <hp> <stamina>");
        }
    }

    ACMD(do_save_attribute_to_image) {
        char file_name[256];
        char map_index_value[256];

        two_arguments(argument, map_index_value, sizeof(map_index_value), file_name, sizeof(file_name));

        if (!*map_index_value || !*file_name) {
            ch->chat_packet(CHAT_TYPE_INFO, "Syntax: /saveati <map_index> <filename>");
            return;
        }

        int32_t map_index = 0;
        str_to_number(map_index, map_index_value);

        if (get_sectree_manager().save_attribute_to_image(map_index, file_name))
            ch->chat_packet(CHAT_TYPE_INFO, "Save done.");
        else
            ch->chat_packet(CHAT_TYPE_INFO, "Save failed.");
    }

    ACMD(do_affect_remove) {
        char arg1[256];
        char arg2[256];

        two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

        if (!*arg1 || !*arg2) {
            ch->chat_packet(CHAT_TYPE_INFO, "Syntax: /affect_remove <player name>");
            ch->chat_packet(CHAT_TYPE_INFO, "Syntax: /affect_remove <type> <point>");

            character *tch = ch;

            if (*arg1)
                if (!(tch = get_character_manager().find_pc(arg1)))
                    tch = ch;

            ch->chat_packet(CHAT_TYPE_INFO, "-- Affect List of %s -------------------------------", tch->name());
            ch->chat_packet(CHAT_TYPE_INFO, "Type Point Modif Duration Flag");

            const std::list<affect *> &cont = tch->affect_container();

            auto it = cont.begin();

            while (it != cont.end()) {
                affect *aff = *it++;

                ch->chat_packet(CHAT_TYPE_INFO, "%4d %5d %5d %8d %u",
                    aff->type, aff->apply_on, aff->apply_value, aff->duration, aff->flag);
            }
            return;
        }

        bool removed = false;

        affect *af;

        uint32_t type = 0;
        str_to_number(type, arg1);
        uint8_t point = 0;
        str_to_number(point, arg2);
        while ((af = ch->find_affect(type, point))) {
            ch->remove_affect(af);
            removed = true;
        }

        if (removed)
            ch->chat_packet(CHAT_TYPE_INFO, "Affect successfully removed.");
        else
            ch->chat_packet(CHAT_TYPE_INFO, "Not affected by that type and point.");
    }

    ACMD(do_change_attr) {
        with_weapon(ch, [](item *weapon) { weapon->change_attribute(); });
    }

    ACMD(do_add_attr) {
        with_weapon(ch, [](item *weapon) { weapon->add_attribute(); });
    }

    ACMD(do_add_socket) {
        with_weapon(ch, [](item *weapon) { weapon->add_socket(); });
    }

    ACMD(do_show_arena_list) {
        get_arena_manager().send_arena_map_list_to(ch);
    }

    ACMD(do_end_all_duel) {
        get_arena_manager().end_all_duel();
    }

    ACMD(do_end_duel) {
        char name[256];

        one_argument(argument, name, sizeof(name));

        character *ch_value = get_character_manager().find_pc(name);
        if (ch_value == nullptr) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("존재하지 않는 캐릭터 입니다."));
            return;
        }

        if (get_arena_manager().end_duel(ch_value->player_id()) == false) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("대련 강제 종료 실패"));
        } else {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("대련 강제 종료 성공"));
        }
    }

    ACMD(do_duel) {
        char name1[256];
        char name2[256];
        char set_value[256];
        char minute_value[256];
        int set = 0;
        int minute = 0;

        argument = two_arguments(argument, name1, sizeof(name1), name2, sizeof(name2));
        two_arguments(argument, set_value, sizeof(set_value), minute_value, sizeof(minute_value));

        str_to_number(set, set_value);

        if (set < 0) set = 1;
        if (set > 5) set = 5;

        if (!str_to_number(minute, minute_value)) {
            minute = 5;
        }
        if (minute < 5)
            minute = 5;

        character *char1 = get_character_manager().find_pc(name1);
        character *char2 = get_character_manager().find_pc(name2);

        if (char1 != nullptr && char2 != nullptr) {
            char1->remove_good_affect();
            char2->remove_good_affect();

            char1->remove_bad_affect();
            char2->remove_bad_affect();

            party *party = char1->get_party();
            if (party != nullptr) {
                if (party->member_count() == 2) {
                    get_party_manager().delete_party(party);
                } else {
                    char1->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 파티에서 나가셨습니다."));
                    party->quit(char1->player_id());
                }
            }

            party = char2->get_party();
            if (party != nullptr) {
                if (party->member_count() == 2) {
                    get_party_manager().delete_party(party);
                } else {
                    char2->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 파티에서 나가셨습니다."));
                    party->quit(char2->player_id());
                }
            }

            if (get_arena_manager().start_duel(char1, char2, set, minute) == true) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("대련이 성공적으로 시작 되었습니다."));
            } else {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("대련 시작에 문제가 있습니다."));
            }
        } else {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("대련자가 없습니다."));
        }
    }

    ACMD(do_stat_plus_amount) {
        char point_value[256];

        one_argument(argument, point_value, sizeof(point_value));

        if (*point_value == '\0')
            return;

        if (ch->is_polymorphed()) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("둔갑 중에는 능력을 올릴 수 없습니다."));
            return;
        }

        int remain_point = ch->point(POINT_STAT);

        if (remain_point <= 0) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("남은 스탯 포인트가 없습니다."));
            return;
        }

        int point = 0;
        str_to_number(point, point_value);

        if (remain_point < point) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("남은 스탯 포인트가 적습니다."));
            return;
        }

        if (point < 0) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("값을 잘못 입력하였습니다."));
            return;
        }

        switch (subcmd) {
        case POINT_HT:
            if (point + ch->point(POINT_HT) > 90) {
                point = 90 - ch->point(POINT_HT);
            }
            break;

        case POINT_IQ:
            if (point + ch->point(POINT_IQ) > 90) {
                point = 90 - ch->point(POINT_IQ);
            }
            break;

        case POINT_ST:
            if (point + ch->point(POINT_ST) > 90) {
                point = 90 - ch->point(POINT_ST);
            }
            break;

        case POINT_DX:
            if (point + ch->point(POINT_DX) > 90) {
                point = 90 - ch->point(POINT_DX);
            }
            break;

        default:
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("명령어의 서브 커맨드가 잘못 되었습니다."));
            return;
            break;
        }

        if (point != 0) {
            ch->set_real_point(subcmd, ch->real_point(subcmd) + point);
            ch->set_point(subcmd, ch->point(subcmd) + point);
            ch->compute_points();
            ch->point_change(subcmd, 0);

            ch->point_change(POINT_STAT, -point);
            ch->compute_points();
        }
    }

    struct t_two_pid {
        int pid1;
        int pid2;
    };

    ACMD(do_break_marriage) {
        char arg1[256], arg2[256];
        two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

        t_two_pid pids = {0, 0};

        str_to_number(pids.pid1, arg1);
        str_to_number(pids.pid2, arg2);

        ch->chat_packet(CHAT_TYPE_INFO, lc_text("플레이어 %d 와 플레이어  %d를 파혼시킵니다.."), pids.pid1, pids.pid2);
        db_clientdesc->db_packet(HEADER_GD_BREAK_MARRIAGE, 0, &pids, sizeof(pids));
    }

    ACMD(do_effect) {
        char arg1[256];

        one_argument(argument, arg1, sizeof(arg1));

        int effect_type = 0;
        str_to_number(effect_type, arg1);
        ch->effect_packet(effect_type);
    }

    ACMD(do_threeway_war_info) {
        ch->chat_packet(CHAT_TYPE_INFO, lc_text("각제국 진행 정보"));
        ch->chat_packet(CHAT_TYPE_INFO, lc_text("선택 맵 정보 성지 %d 통로 %d %d %d"), get_sungzi_map_index(), get_pass_map_index(1), get_pass_map_index(2), get_pass_map_index(3));
        ch->chat_packet(CHAT_TYPE_INFO, "ThreewayPhase %d", get_three_way_war().regen_flag());

        for (int n = 1; n < 4; ++n) {
            sectree_map *sec_map = get_sectree_manager().map(get_sungzi_map_index());

            int count[4] = {};

            if (sec_map) {
                sec_map->for_each([&](entity *ent) {
                    if (ent->is_type(ENTITY_CHARACTER)) {
                        auto *ch_value = (character *)ent;
                        if (ch_value && ch_value->is_pc())
                            ++count[ch_value->empire()];
                    }
                });
            }

            ch->chat_packet(CHAT_TYPE_INFO, "%s killscore %d usercount %d",
                empire_name(n),
                get_three_way_war().kill_score(n),
                count[n]);
        }
    }

    ACMD(do_threeway_war_myinfo) {
        ch->chat_packet(CHAT_TYPE_INFO, lc_text("나의 삼거리 진행정보"));
        ch->chat_packet(CHAT_TYPE_INFO, "Deadcount %d",
            get_three_way_war().revive_token_for_player(ch->player_id()));
    }

    ACMD(do_rmcandidacy) {
        char arg1[256];

        one_argument(argument, arg1, sizeof(arg1));

        if (!*arg1) {
            ch->chat_packet(CHAT_TYPE_INFO, "Usage: rmcandidacy <name>");
            return;
        }

        character *tch = get_character_manager().find_pc(arg1);

        if (!tch) {
            cci *cci = get_p2p_manager().find(arg1);

            if (cci) {
                if (cci->channel != metin2::config::server().channel) {
                    ch->chat_packet(CHAT_TYPE_INFO, "Target is in %d channel (my channel %d)", cci->channel, metin2::config::server().channel);
                    return;
                }
            }
        }

        db_clientdesc->db_packet(HEADER_GD_RMCANDIDACY, 0, nullptr, 32);
        db_clientdesc->packet(arg1, 32);
    }

    ACMD(do_setmonarch) {
        char arg1[256];

        one_argument(argument, arg1, sizeof(arg1));

        if (!*arg1) {
            ch->chat_packet(CHAT_TYPE_INFO, "Usage: setmonarch <name>");
            return;
        }

        db_clientdesc->db_packet(HEADER_GD_SETMONARCH, 0, nullptr, 32);
        db_clientdesc->packet(arg1, 32);
    }

    ACMD(do_rmmonarch) {
        char arg1[256];

        one_argument(argument, arg1, sizeof(arg1));

        if (!*arg1) {
            ch->chat_packet(CHAT_TYPE_INFO, "Usage: rmmonarch <name>");
            return;
        }

        db_clientdesc->db_packet(HEADER_GD_RMMONARCH, 0, nullptr, 32);
        db_clientdesc->packet(arg1, 32);
    }

    ACMD(do_check_monarch_money) {
        char arg1[256];

        one_argument(argument, arg1, sizeof(arg1));

        if (!*arg1)
            return;

        int empire = 0;
        str_to_number(empire, arg1);
        int nation_money = get_monarch().money(empire);

        ch->chat_packet(CHAT_TYPE_INFO, "국고: %d 원", nation_money);
    }

    ACMD(do_reset_subskill) {
        char arg1[256];

        one_argument(argument, arg1, sizeof(arg1));

        if (!*arg1) {
            ch->chat_packet(CHAT_TYPE_INFO, "Usage: reset_subskill <name>");
            return;
        }

        character *tch = get_character_manager().find_pc(arg1);

        if (tch == nullptr)
            return;

        tch->clear_sub_skill();
        ch->chat_packet(CHAT_TYPE_INFO, "Subskill of [%s] was reset", tch->name());
    }

    ACMD(do_siege) {
        char arg1[256], arg2[256];
        two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

        int empire = strtol(arg1, nullptr, 10);
        int tower_count = strtol(arg2, nullptr, 10);

        if (empire == 0) empire = number(1, 3);
        if (tower_count < 5 || tower_count > 10) tower_count = number(5, 10);

        packet_gg_siege packet;
        packet.header = HEADER_GG_SIEGE;
        packet.empire = empire;
        packet.tower_count = tower_count;

        get_p2p_manager().send(&packet, sizeof(packet_gg_siege));

        switch (castle_siege(empire, tower_count)) {
        case 0:
            ch->chat_packet(CHAT_TYPE_INFO, "SIEGE FAILED");
            break;
        case 1:
            ch->chat_packet(CHAT_TYPE_INFO, "SIEGE START Empire(%d) Tower(%d)", empire, tower_count);
            break;
        case 2:
            ch->chat_packet(CHAT_TYPE_INFO, "SIEGE END");
            break;
        }
    }

    ACMD(do_temp) {
        if (false == metin2::config::test_server())
            return;

        char arg1[256], arg2[256];
        two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

        if (0 == arg1[0] || 0 == arg2[0]) {
            ch->chat_packet(CHAT_TYPE_INFO, "Usage: empire money");
            return;
        }

        int empire = 0;
        str_to_number(empire, arg1);
        int money = 0;
        str_to_number(money, arg2);

        get_monarch().sendto_db_add_money(money, empire, ch);
    }

    ACMD(do_frog) {
        char arg1[256];
        one_argument(argument, arg1, sizeof(arg1));

        if (0 == arg1[0]) {
            ch->chat_packet(CHAT_TYPE_INFO, "Usage: empire(1, 2, 3)");
            return;
        }

        int empire = 0;
        str_to_number(empire, arg1);

        switch (empire) {
        case 1:
        case 2:
        case 3:
            if (is_castle_map(ch->map_index())) {
                castle_spawn_frog(empire);
                castle_save();
            } else
                ch->chat_packet(CHAT_TYPE_INFO, "You must spawn frog in castle");
            break;

        default:
            ch->chat_packet(CHAT_TYPE_INFO, "Usage: empire(1, 2, 3)");
            break;
        }
    }

    ACMD(do_flush) {
        char arg1[256];
        one_argument(argument, arg1, sizeof(arg1));

        if (0 == arg1[0]) {
            ch->chat_packet(CHAT_TYPE_INFO, "usage : /flush player_id");
            return;
        }

        auto pid = (uint32_t)strtoul(arg1, nullptr, 10);

        db_clientdesc->db_packet_header(HEADER_GD_FLUSH_CACHE, 0, sizeof(uint32_t));
        db_clientdesc->packet(&pid, sizeof(uint32_t));
    }

    ACMD(do_eclipse) {
        char arg1[256];
        one_argument(argument, arg1, sizeof(arg1));

        if (strtol(arg1, nullptr, 10) == 1) {
            quest::get_quest_manager().request_set_event_flag("eclipse", 1);
        } else {
            quest::get_quest_manager().request_set_event_flag("eclipse", 0);
        }
    }

    ACMD(do_weeklyevent) {
        char arg1[256];
        int empire = 0;

        if (get_battle_arena().is_running() == false) {
            one_argument(argument, arg1, sizeof(arg1));

            empire = strtol(arg1, nullptr, 10);

            if (empire == 1 || empire == 2 || empire == 3) {
                get_battle_arena().start(empire);
            } else {
                get_battle_arena().start(number(1, 3));
            }
            ch->chat_packet(CHAT_TYPE_INFO, "Weekly Event Start");
        } else {
            get_battle_arena().force_end();
            ch->chat_packet(CHAT_TYPE_INFO, "Weekly Event End");
        }
    }

    ACMD(do_event_helper) {
        char arg1[256];
        int mode = 0;

        one_argument(argument, arg1, sizeof(arg1));
        str_to_number(mode, arg1);

        if (mode == 1) {
            xmas::spawn_event_helper(true);
            ch->chat_packet(CHAT_TYPE_INFO, "Event Helper Spawn");
        } else {
            xmas::spawn_event_helper(false);
            ch->chat_packet(CHAT_TYPE_INFO, "Event Helper Delete");
        }
    }

    ACMD(do_get_mob_count) {
        sectree_map *sectree = get_sectree_manager().map(ch->map_index());

        if (sectree == nullptr)
            return;

        int count = 0;

        sectree->for_each([&](entity *ent) {
            if (ent->is_type(ENTITY_CHARACTER)) {
                auto *ch_value = static_cast<character *>(ent);

                if (ch_value->is_monster() == true || ch_value->is_stone()) {
                    count++;
                }
            }
        });

        ch->chat_packet(CHAT_TYPE_INFO, "MapIndex: %d MobCount %d", ch->map_index(), count);
    }

    ACMD(do_clear_land) {
        const building::land *land = building::get_manager().find_land(ch->map_index(), ch->x(), ch->y());

        if (nullptr == land) {
            return;
        }

        ch->chat_packet(CHAT_TYPE_INFO, "Guild Land(%d) Cleared", land->id());

        building::get_manager().clear_land(land->id());
    }

    ACMD(do_special_item) {
        get_item_manager().conv_special_drop_item_file();
    }

    ACMD(do_set_stat) {
        char name[256];
        char change_amount_value[256];

        two_arguments(argument, name, sizeof(name), change_amount_value, sizeof(change_amount_value));

        if (!*name || *change_amount_value == '\0') {
            ch->chat_packet(CHAT_TYPE_INFO, "Invalid argument.");
            return;
        }

        character *tch = get_character_manager().find_pc(name);

        if (!tch) {
            cci *cci = get_p2p_manager().find(name);

            if (cci) {
                ch->chat_packet(CHAT_TYPE_INFO, "Cannot find player(%s). %s is not in your game server.", name, name);
                return;
            } else {
                ch->chat_packet(CHAT_TYPE_INFO, "Cannot find player(%s). Perhaps %s doesn't login or exist.", name, name);
                return;
            }
        } else {
            if (tch->is_polymorphed()) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("둔갑 중에는 능력을 올릴 수 없습니다."));
                return;
            }

            if (subcmd != POINT_HT && subcmd != POINT_IQ && subcmd != POINT_ST && subcmd != POINT_DX) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("명령어의 서브 커맨드가 잘못 되었습니다."));
                return;
            }
            int remain_point = tch->point(POINT_STAT);
            int cur_point = tch->real_point(subcmd);
            int change_amount = 0;
            str_to_number(change_amount, change_amount_value);
            int point = cur_point + change_amount;

            int n;
            switch (subcmd) {
            case POINT_HT:
                if (point < job_initial_points[tch->job()].ht) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("Cannot set stat under initial stat."));
                    return;
                }
                n = 0;
                break;
            case POINT_IQ:
                if (point < job_initial_points[tch->job()].iq) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("Cannot set stat under initial stat."));
                    return;
                }
                n = 1;
                break;
            case POINT_ST:
                if (point < job_initial_points[tch->job()].st) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("Cannot set stat under initial stat."));
                    return;
                }
                n = 2;
                break;
            case POINT_DX:
                if (point < job_initial_points[tch->job()].dx) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("Cannot set stat under initial stat."));
                    return;
                }
                n = 3;
                break;
            }

            if (point > 90) {
                change_amount -= point - 90;
                point = 90;
            }

            if (remain_point < change_amount) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("남은 스탯 포인트가 적습니다."));
                return;
            }

            tch->set_real_point(subcmd, point);
            tch->set_point(subcmd, tch->point(subcmd) + change_amount);
            tch->compute_points();
            tch->point_change(subcmd, 0);

            tch->point_change(POINT_STAT, -change_amount);
            tch->compute_points();

            const char *stat_name[4] = {"con", "int", "str", "dex"};

            ch->chat_packet(CHAT_TYPE_INFO, "%s's %s change %d to %d", name, stat_name[n], cur_point, point);
        }
    }

    ACMD(do_get_item_id_list) {
        for (int i = 0; i < INVENTORY_AND_EQUIP_SLOT_MAX; i++) {
            item *item = ch->inventory_item(i);
            if (item != nullptr)
                ch->chat_packet(CHAT_TYPE_INFO, "cell : %d, name : %s, id : %d", item->cell(), item->name(), item->id());
        }
    }

    ACMD(do_set_socket) {
        char arg1[256];
        char arg2[256];
        char arg3[256];

        one_argument(two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2)), arg3, sizeof(arg3));

        int item_id, socket_num, value;
        if (!str_to_number(item_id, arg1) || !str_to_number(socket_num, arg2) || !str_to_number(value, arg3))
            return;

        item *item = get_item_manager().find(item_id);
        if (item)
            item->set_socket(socket_num, value);
    }

    ACMD(do_can_dead) {
        if (subcmd)
            ch->set_armada();
        else
            ch->reset_armada();
    }

    ACMD(do_full_set) {
        do_all_skill_master(ch, nullptr, 0, 0);
        do_item_full_set(ch, nullptr, 0, 0);
        do_attr_full_set(ch, nullptr, 0, 0);
    }

    ACMD(do_all_skill_master) {
        ch->set_horse_level(SKILL_MAX_LEVEL);
        for (int i = 0; i < SKILL_MAX_NUM; i++) {
            if (true == ch->can_use_skill(i)) {
                ch->set_skill_level(i, SKILL_MAX_LEVEL);
            } else {
                switch (i) {
                case SKILL_HORSE_WILDATTACK:
                case SKILL_HORSE_CHARGE:
                case SKILL_HORSE_ESCAPE:
                case SKILL_HORSE_WILDATTACK_RANGE:
                    ch->set_skill_level(i, SKILL_MAX_LEVEL);
                    break;
                }
            }
        }
        ch->compute_points();
        ch->skill_level_packet();
    }

    ACMD(do_item_full_set) {
        uint8_t job = ch->job();
        item *item;
        for (int i = 0; i < 6; i++) {
            item = ch->wear(i);
            if (item != nullptr)
                ch->unequip_item(item);
        }
        item = ch->wear(WEAR_SHIELD);
        if (item != nullptr)
            ch->unequip_item(item);

        switch (job) {
        case JOB_SURA:
            {

                item = get_item_manager().create_item(11699);
                if (!item || !item->equip_to(ch, item->find_equip_cell(ch)))
                    get_item_manager().destroy_item(item);
                item = get_item_manager().create_item(13049);
                if (!item || !item->equip_to(ch, item->find_equip_cell(ch)))
                    get_item_manager().destroy_item(item);
                item = get_item_manager().create_item(15189);
                if (!item || !item->equip_to(ch, item->find_equip_cell(ch)))
                    get_item_manager().destroy_item(item);
                item = get_item_manager().create_item(189);
                if (!item || !item->equip_to(ch, item->find_equip_cell(ch)))
                    get_item_manager().destroy_item(item);
                item = get_item_manager().create_item(12529);
                if (!item || !item->equip_to(ch, item->find_equip_cell(ch)))
                    get_item_manager().destroy_item(item);
                item = get_item_manager().create_item(14109);
                if (!item || !item->equip_to(ch, item->find_equip_cell(ch)))
                    get_item_manager().destroy_item(item);
                item = get_item_manager().create_item(17209);
                if (!item || !item->equip_to(ch, item->find_equip_cell(ch)))
                    get_item_manager().destroy_item(item);
                item = get_item_manager().create_item(16209);
                if (!item || !item->equip_to(ch, item->find_equip_cell(ch)))
                    get_item_manager().destroy_item(item);
            }
            break;
        case JOB_WARRIOR:
            {

                item = get_item_manager().create_item(11299);
                if (!item || !item->equip_to(ch, item->find_equip_cell(ch)))
                    get_item_manager().destroy_item(item);
                item = get_item_manager().create_item(13049);
                if (!item || !item->equip_to(ch, item->find_equip_cell(ch)))
                    get_item_manager().destroy_item(item);
                item = get_item_manager().create_item(15189);
                if (!item || !item->equip_to(ch, item->find_equip_cell(ch)))
                    get_item_manager().destroy_item(item);
                item = get_item_manager().create_item(3159);
                if (!item || !item->equip_to(ch, item->find_equip_cell(ch)))
                    get_item_manager().destroy_item(item);
                item = get_item_manager().create_item(12249);
                if (!item || !item->equip_to(ch, item->find_equip_cell(ch)))
                    get_item_manager().destroy_item(item);
                item = get_item_manager().create_item(14109);
                if (!item || !item->equip_to(ch, item->find_equip_cell(ch)))
                    get_item_manager().destroy_item(item);
                item = get_item_manager().create_item(17109);
                if (!item || !item->equip_to(ch, item->find_equip_cell(ch)))
                    get_item_manager().destroy_item(item);
                item = get_item_manager().create_item(16109);
                if (!item || !item->equip_to(ch, item->find_equip_cell(ch)))
                    get_item_manager().destroy_item(item);
            }
            break;
        case JOB_SHAMAN:
            {

                item = get_item_manager().create_item(11899);
                if (!item || !item->equip_to(ch, item->find_equip_cell(ch)))
                    get_item_manager().destroy_item(item);
                item = get_item_manager().create_item(13049);
                if (!item || !item->equip_to(ch, item->find_equip_cell(ch)))
                    get_item_manager().destroy_item(item);
                item = get_item_manager().create_item(15189);
                if (!item || !item->equip_to(ch, item->find_equip_cell(ch)))
                    get_item_manager().destroy_item(item);
                item = get_item_manager().create_item(7159);
                if (!item || !item->equip_to(ch, item->find_equip_cell(ch)))
                    get_item_manager().destroy_item(item);
                item = get_item_manager().create_item(12669);
                if (!item || !item->equip_to(ch, item->find_equip_cell(ch)))
                    get_item_manager().destroy_item(item);
                item = get_item_manager().create_item(14109);
                if (!item || !item->equip_to(ch, item->find_equip_cell(ch)))
                    get_item_manager().destroy_item(item);
                item = get_item_manager().create_item(17209);
                if (!item || !item->equip_to(ch, item->find_equip_cell(ch)))
                    get_item_manager().destroy_item(item);
                item = get_item_manager().create_item(16209);
                if (!item || !item->equip_to(ch, item->find_equip_cell(ch)))
                    get_item_manager().destroy_item(item);
            }
            break;
        case JOB_ASSASSIN:
            {

                item = get_item_manager().create_item(11499);
                if (!item || !item->equip_to(ch, item->find_equip_cell(ch)))
                    get_item_manager().destroy_item(item);
                item = get_item_manager().create_item(13049);
                if (!item || !item->equip_to(ch, item->find_equip_cell(ch)))
                    get_item_manager().destroy_item(item);
                item = get_item_manager().create_item(15189);
                if (!item || !item->equip_to(ch, item->find_equip_cell(ch)))
                    get_item_manager().destroy_item(item);
                item = get_item_manager().create_item(1139);
                if (!item || !item->equip_to(ch, item->find_equip_cell(ch)))
                    get_item_manager().destroy_item(item);
                item = get_item_manager().create_item(12389);
                if (!item || !item->equip_to(ch, item->find_equip_cell(ch)))
                    get_item_manager().destroy_item(item);
                item = get_item_manager().create_item(14109);
                if (!item || !item->equip_to(ch, item->find_equip_cell(ch)))
                    get_item_manager().destroy_item(item);
                item = get_item_manager().create_item(17189);
                if (!item || !item->equip_to(ch, item->find_equip_cell(ch)))
                    get_item_manager().destroy_item(item);
                item = get_item_manager().create_item(16189);
                if (!item || !item->equip_to(ch, item->find_equip_cell(ch)))
                    get_item_manager().destroy_item(item);
            }
            break;
        }
    }

    ACMD(do_attr_full_set) {
        uint8_t job = ch->job();
        item *item;

        switch (job) {
        case JOB_WARRIOR:
        case JOB_ASSASSIN:
        case JOB_SURA:
        case JOB_SHAMAN:
            {
                item = ch->wear(WEAR_HEAD);
                if (item != nullptr) {
                    item->clear_attribute();
                    item->set_force_attribute(0, APPLY_ATT_SPEED, 8);
                    item->set_force_attribute(1, APPLY_HP_REGEN, 30);
                    item->set_force_attribute(2, APPLY_SP_REGEN, 30);
                    item->set_force_attribute(3, APPLY_DODGE, 15);
                    item->set_force_attribute(4, APPLY_STEAL_SP, 10);
                }

                item = ch->wear(WEAR_WEAPON);
                if (item != nullptr) {
                    item->clear_attribute();
                    item->set_force_attribute(0, APPLY_CAST_SPEED, 20);
                    item->set_force_attribute(1, APPLY_CRITICAL_PCT, 10);
                    item->set_force_attribute(2, APPLY_PENETRATE_PCT, 10);
                    item->set_force_attribute(3, APPLY_ATTBONUS_DEVIL, 20);
                    item->set_force_attribute(4, APPLY_STR, 12);
                }

                item = ch->wear(WEAR_SHIELD);
                if (item != nullptr) {
                    item->clear_attribute();
                    item->set_force_attribute(0, APPLY_CON, 12);
                    item->set_force_attribute(1, APPLY_BLOCK, 15);
                    item->set_force_attribute(2, APPLY_REFLECT_MELEE, 10);
                    item->set_force_attribute(3, APPLY_IMMUNE_STUN, 1);
                    item->set_force_attribute(4, APPLY_IMMUNE_SLOW, 1);
                }

                item = ch->wear(WEAR_BODY);
                if (item != nullptr) {
                    item->clear_attribute();
                    item->set_force_attribute(0, APPLY_MAX_HP, 2000);
                    item->set_force_attribute(1, APPLY_CAST_SPEED, 20);
                    item->set_force_attribute(2, APPLY_STEAL_HP, 10);
                    item->set_force_attribute(3, APPLY_REFLECT_MELEE, 10);
                    item->set_force_attribute(4, APPLY_ATT_GRADE_BONUS, 50);
                }

                item = ch->wear(WEAR_FOOTS);
                if (item != nullptr) {
                    item->clear_attribute();
                    item->set_force_attribute(0, APPLY_MAX_HP, 2000);
                    item->set_force_attribute(1, APPLY_MAX_SP, 80);
                    item->set_force_attribute(2, APPLY_MOV_SPEED, 8);
                    item->set_force_attribute(3, APPLY_ATT_SPEED, 8);
                    item->set_force_attribute(4, APPLY_CRITICAL_PCT, 10);
                }

                item = ch->wear(WEAR_WRIST);
                if (item != nullptr) {
                    item->clear_attribute();
                    item->set_force_attribute(0, APPLY_MAX_HP, 2000);
                    item->set_force_attribute(1, APPLY_MAX_SP, 80);
                    item->set_force_attribute(2, APPLY_PENETRATE_PCT, 10);
                    item->set_force_attribute(3, APPLY_STEAL_HP, 10);
                    item->set_force_attribute(4, APPLY_MANA_BURN_PCT, 10);
                }
                item = ch->wear(WEAR_NECK);
                if (item != nullptr) {
                    item->clear_attribute();
                    item->set_force_attribute(0, APPLY_MAX_HP, 2000);
                    item->set_force_attribute(1, APPLY_MAX_SP, 80);
                    item->set_force_attribute(2, APPLY_CRITICAL_PCT, 10);
                    item->set_force_attribute(3, APPLY_PENETRATE_PCT, 10);
                    item->set_force_attribute(4, APPLY_STEAL_SP, 10);
                }
                item = ch->wear(WEAR_EAR);
                if (item != nullptr) {
                    item->clear_attribute();
                    item->set_force_attribute(0, APPLY_MOV_SPEED, 20);
                    item->set_force_attribute(1, APPLY_MANA_BURN_PCT, 10);
                    item->set_force_attribute(2, APPLY_POISON_REDUCE, 5);
                    item->set_force_attribute(3, APPLY_ATTBONUS_DEVIL, 20);
                    item->set_force_attribute(4, APPLY_ATTBONUS_UNDEAD, 20);
                }
            }
            break;
        }
    }

    ACMD(do_use_item) {
        char arg1[256];

        one_argument(argument, arg1, sizeof(arg1));

        int cell;
        str_to_number(cell, arg1);

        item *item = ch->inventory_item(cell);
        if (item) {
            ch->use_item(item_pos(INVENTORY, cell));
        } else {
            ch->chat_packet(CHAT_TYPE_INFO, "아이템이 없어서 착용할 수 없어.");
        }
    }

    ACMD(do_clear_affect) {
        ch->clear_affect(true);
    }

    ACMD(do_dragon_soul) {
        char arg1[512];
        const char *rest = one_argument(argument, arg1, sizeof(arg1));
        switch (arg1[0]) {
        case 'a':
            {
                one_argument(rest, arg1, sizeof(arg1));
                int deck_idx;
                if (str_to_number(deck_idx, arg1) == false) {
                    return;
                }
                ch->dragon_soul_activate_deck(deck_idx);
            }
            break;
        case 'd':
            {
                ch->dragon_soul_deactivate_all();
            }
            break;
        }
    }

    ACMD(do_ds_list) {
        for (int i = 0; i < DRAGON_SOUL_INVENTORY_MAX_NUM; i++) {
            item_pos cell(DRAGON_SOUL_INVENTORY, i);

            item *item = ch->get_item(cell);
            if (item != nullptr)
                ch->chat_packet(CHAT_TYPE_INFO, "cell : %d, name : %s, id : %d", item->cell(), item->name(), item->id());
        }
    }
}
