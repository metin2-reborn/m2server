#include <metin2/config/config.h>
#include <metin2/game/characters/character_manager.h>

#include <metin2/game/admin/cmd.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/fsm.h>
#include <metin2/game/characters/mob_manager.h>
#include <metin2/game/events/mining.h>
#include <metin2/game/events/xmas_event.h>
#include <metin2/game/parties/party.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/questlua.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/world/dungeon.h>
#include <metin2/game/world/regen.h>
#include <metin2/game/world/sectree.h>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/world/vid.h>

#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/character_types.h>
#include <metin2/proto/limits.h>
#include <metin2/proto/log_types.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <format>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <strings.h>

namespace metin2::game::inline characters {

    character_manager::character_manager() : vid_count_(0),
                                             chr_selected_stone_(nullptr),
                                             use_pending_destroy_(false) {
        register_race_num(xmas::MOB_XMAS_FIRWORK_SELLER_VNUM);
        register_race_num(xmas::MOB_SANTA_VNUM);
        register_race_num(xmas::MOB_XMAS_TREE_VNUM);

        mob_item_rate_ = 100;
        mob_damage_rate_ = 100;
        mob_gold_amount_rate_ = 100;
        mob_gold_drop_rate_ = 100;
        mob_exp_rate_ = 100;

        mob_item_rate_premium_ = 100;
        mob_gold_amount_rate_premium_ = 100;
        mob_gold_drop_rate_premium_ = 100;
        mob_exp_rate_premium_ = 100;
    }

    character_manager::~character_manager() {
        destroy();
    }

    void character_manager::destroy() {
        auto it = map_chr_by_vid_.begin();
        while (it != map_chr_by_vid_.end()) {
            character *ch = it->second;
            character_manager::instance().destroy_character(ch);
            it = map_chr_by_vid_.begin();
        }
    }

    void character_manager::graceful_shutdown() {
        auto it = map_pc_chr_.begin();

        while (it != map_pc_chr_.end())
            (it++)->second->disconnect("server shutdown");
    }

    auto character_manager::alloc_vid() -> uint32_t {
        ++vid_count_;
        return vid_count_;
    }

    auto character_manager::create_character(const char *name, uint32_t pid) -> character * {
        uint32_t dwVID = alloc_vid();

        auto *ch = new character;
        ch->create(name, dwVID, pid ? true : false);

        map_chr_by_vid_.insert(std::make_pair(dwVID, ch));

        if (pid) {
            char szName[CHARACTER_NAME_MAX_LEN + 1];
            str_lower(name, szName, sizeof(szName));

            map_pc_chr_.insert(name_map::value_type(szName, ch));
            map_chr_by_pid_.insert(std::make_pair(pid, ch));
        }

        return (ch);
    }

    void character_manager::destroy_character(character *ch) {
        if (!ch)
            return;

        auto it = map_chr_by_vid_.find(ch->get_vid());
        if (it == map_chr_by_vid_.end()) {
            metin2::log::error("Cannot destroy character vid {:d}: not found", (int32_t)(ch->get_vid()));
            return;
        }

        if (ch->is_npc() && !ch->is_pet() && ch->rider() == nullptr) {
            if (ch->get_dungeon()) {
                ch->get_dungeon()->dead_character(ch);
            }
        }

        if (use_pending_destroy_) {
            set_chr_pending_destroy_.insert(ch);
            return;
        }

        map_chr_by_vid_.erase(it);

        if (true == ch->is_pc()) {
            char szName[CHARACTER_NAME_MAX_LEN + 1];

            str_lower(ch->name(), szName, sizeof(szName));

            auto it = map_pc_chr_.find(szName);

            if (map_pc_chr_.end() != it)
                map_pc_chr_.erase(it);
        }

        if (0 != ch->player_id()) {
            auto it = map_chr_by_pid_.find(ch->player_id());

            if (map_chr_by_pid_.end() != it) {
                map_chr_by_pid_.erase(it);
            }
        }

        unregister_race_num_map(ch);

        remove_from_state_list(ch);

        delete ch;
    }

    auto character_manager::find(uint32_t vid) -> character * {
        auto it = map_chr_by_vid_.find(vid);

        if (map_chr_by_vid_.end() == it)
            return nullptr;

        character *found = it->second;
        if (found != nullptr && vid != (uint32_t)found->get_vid()) {
            metin2::log::error("Character lookup by vid {:d} returned vid {:d}", vid, (uint32_t)found->get_vid());
            return nullptr;
        }
        return found;
    }

    auto character_manager::find(const vid &vid) -> character * {
        character *tch = find((uint32_t)vid);

        if (!tch || tch->get_vid() != vid)
            return nullptr;

        return tch;
    }

    auto character_manager::find_by_pid(uint32_t pid) -> character * {
        auto it = map_chr_by_pid_.find(pid);

        if (map_chr_by_pid_.end() == it)
            return nullptr;

        character *found = it->second;
        if (found != nullptr && pid != found->player_id()) {
            metin2::log::error("Character lookup by pid {:d} returned pid {:d}", pid, found->player_id());
            return nullptr;
        }
        return found;
    }

    auto character_manager::find_pc(const char *name) -> character * {
        char szName[CHARACTER_NAME_MAX_LEN + 1];
        str_lower(name, szName, sizeof(szName));
        auto it = map_pc_chr_.find(szName);

        if (it == map_pc_chr_.end())
            return nullptr;

        character *found = it->second;
        if (found != nullptr && strncasecmp(szName, found->name(), CHARACTER_NAME_MAX_LEN) != 0) {
            metin2::log::error("Character lookup by name {} returned {}", name, found->name());
            return nullptr;
        }
        return found;
    }

    auto character_manager::spawn_mob_random_position(uint32_t vnum, int32_t map_index) -> character * {
        {
            if (vnum == 5001 && !quest::quest_manager::instance().event_flag("japan_regen")) {
                metin2::log::debug("Waegu (5001) regen disabled");
                return nullptr;
            }
        }

        {
            if (vnum == 5002 && !quest::quest_manager::instance().event_flag("newyear_mob")) {
                metin2::log::debug("Haetae new year mob (5002) regen disabled");
                return nullptr;
            }
        }

        {
            if (vnum == 5004 && !quest::quest_manager::instance().event_flag("independence_day")) {
                metin2::log::debug("Independence day mob (5004) regen disabled");
                return nullptr;
            }
        }

        const mob *pkMob = mob_manager::instance().get(vnum);

        if (!pkMob) {
            metin2::log::error("No mob data for vnum {:d}", vnum);
            return nullptr;
        }

        if (!map_allow_find(map_index)) {
            metin2::log::error("Cannot spawn mob: map {:d} is not served here", map_index);
            return nullptr;
        }

        sectree_map *pkSectreeMap = sectree_manager::instance().map(map_index);
        if (pkSectreeMap == nullptr) {
            return nullptr;
        }

        int i;
        int32_t x, y;
        for (i = 0; i < 2000; i++) {
            x = number(1, (pkSectreeMap->setting.width / 100) - 1) * 100 + pkSectreeMap->setting.base_x;
            y = number(1, (pkSectreeMap->setting.height / 100) - 1) * 100 + pkSectreeMap->setting.base_y;
            sectree *tree = pkSectreeMap->find(x, y);

            if (!tree)
                continue;

            uint32_t dwAttr = tree->get_attribute(x, y);

            if (is_set(dwAttr, ATTR_BLOCK | ATTR_OBJECT))
                continue;

            if (is_set(dwAttr, ATTR_BANPK))
                continue;

            break;
        }

        if (i == 2000) {
            metin2::log::error("Cannot find a valid random spawn location");
            return nullptr;
        }

        sectree *sectree = sectree_manager::instance().get(map_index, x, y);

        if (!sectree) {
            metin2::log::warn("Cannot spawn mob: no sectree at {:d} x {:d} (map {:d})", x, y, map_index);
            return nullptr;
        }

        character *ch = character_manager::instance().create_character(pkMob->table.locale_name);

        if (!ch) {
            metin2::log::error("Cannot create character for random mob spawn");
            return nullptr;
        }

        ch->set_proto(pkMob);

        if (pkMob->table.type == CHAR_TYPE_NPC)
            if (ch->empire() == 0)
                ch->set_empire(sectree_manager::instance().empire_from_map_index(map_index));

        ch->set_rotation(number(0, 360));

        if (!ch->show(map_index, x, y, 0, false)) {
            character_manager::instance().destroy_character(ch);
            metin2::log::error("Cannot show randomly spawned mob");
            return nullptr;
        }

        int32_t local_x = x - pkSectreeMap->setting.base_x;
        int32_t local_y = y - pkSectreeMap->setting.base_y;
        const std::string buf = std::format("spawn {}[{}] random position at {} {} {} {} (time: {})", ch->name(), vnum, x, y, local_x, local_y, get_global_time());

        if (metin2::config::test_server())
            send_notice(buf.c_str());

        metin2::log::info("{}", buf);
        return (ch);
    }

    auto character_manager::spawn_mob(uint32_t vnum, int32_t map_index, int32_t x, int32_t y, int32_t z, bool spawn_motion, int rot, bool show_value) -> character * {
        const mob *pkMob = mob_manager::instance().get(vnum);
        if (!pkMob) {
            metin2::log::error("No mob data for vnum {:d}", vnum);
            return nullptr;
        }

        if (!(pkMob->table.type == CHAR_TYPE_NPC || pkMob->table.type == CHAR_TYPE_WARP || pkMob->table.type == CHAR_TYPE_GOTO) || mining::is_vein_of_ore(vnum)) {
            sectree *tree = sectree_manager::instance().get(map_index, x, y);

            if (!tree) {
                metin2::log::warn("Cannot spawn mob: no sectree at {:d} {:d} for mob {:d} (map {:d})", x, y, vnum, map_index);
                return nullptr;
            }

            uint32_t dwAttr = tree->get_attribute(x, y);

            bool is_set_value = false;

            if (mining::is_vein_of_ore(vnum))
                is_set_value = is_set(dwAttr, ATTR_BLOCK);
            else
                is_set_value = is_set(dwAttr, ATTR_BLOCK | ATTR_OBJECT);

            if (is_set_value) {
                static bool s_isLog = quest::quest_manager::instance().event_flag("spawn_block_log");
                static uint32_t s_nextTime = get_global_time() + 10000;

                uint32_t curTime = get_global_time();

                if (curTime > s_nextTime) {
                    s_nextTime = curTime;
                    s_isLog = quest::quest_manager::instance().event_flag("spawn_block_log");
                }

                if (s_isLog)
                    metin2::log::info("Mob spawn: blocked position for {} ({:d}) at {:d} {:d} (attr {:d})", pkMob->table.name, vnum, x, y, dwAttr);
                return nullptr;
            }

            if (is_set(dwAttr, ATTR_BANPK)) {
                metin2::log::warn("Cannot spawn mob {} ({:d}) in a no-PK zone at {:d} {:d}", pkMob->table.name, vnum, x, y);
                return nullptr;
            }
        }

        sectree *sectree = sectree_manager::instance().get(map_index, x, y);

        if (!sectree) {
            metin2::log::warn("Cannot spawn mob: no sectree at {:d} x {:d} (map {:d})", x, y, map_index);
            return nullptr;
        }

        character *ch = character_manager::instance().create_character(pkMob->table.locale_name);

        if (!ch) {
            metin2::log::error("Cannot create character for mob spawn");
            return nullptr;
        }

        if (rot == -1)
            rot = number(0, 360);

        ch->set_proto(pkMob);

        if (pkMob->table.type == CHAR_TYPE_NPC)
            if (ch->empire() == 0)
                ch->set_empire(sectree_manager::instance().empire_from_map_index(map_index));

        ch->set_rotation(rot);

        if (show_value && !ch->show(map_index, x, y, z, spawn_motion)) {
            character_manager::instance().destroy_character(ch);
            metin2::log::error("Cannot show spawned mob");
            return nullptr;
        }

        return (ch);
    }

    auto character_manager::spawn_mob_range(uint32_t vnum, int32_t map_index, int sx, int sy, int ex, int ey, bool is_exception, bool spawn_motion, bool aggressive) -> character * {
        const mob *pkMob = mob_manager::instance().get(vnum);

        if (!pkMob)
            return nullptr;

        if (pkMob->table.type == CHAR_TYPE_STONE)
            spawn_motion = true;

        int i = 16;

        while (i--) {
            int x = number(sx, ex);
            int y = number(sy, ey);
            character *ch = spawn_mob(vnum, map_index, x, y, 0, spawn_motion);

            if (ch) {
                metin2::log::debug("Mob spawned: {} (vid {:d}) at {:d}x{:d}", ch->name(), (uint32_t)ch->get_vid(), ch->x(), ch->y());
                if (aggressive)
                    ch->set_aggressive();
                return (ch);
            }
        }

        return nullptr;
    }

    void character_manager::select_stone(character *chr) {
        chr_selected_stone_ = chr;
    }

    auto character_manager::spawn_move_group(uint32_t vnum, int32_t map_index, int sx, int sy, int ex, int ey, int tx, int ty, regen *regen_value, bool aggressive) -> bool {
        mob_group *pkGroup = mob_manager::instance().group(vnum);

        if (!pkGroup) {
            metin2::log::error("Mob group {:d} does not exist (map {:d})", vnum, map_index);
            return false;
        }

        character *pkChrMaster = nullptr;
        party *pkParty = nullptr;

        const std::vector<uint32_t> &c_rdwMembers = pkGroup->member_vector();

        bool bSpawnedByStone = false;
        bool bAggressive = aggressive;

        if (chr_selected_stone_) {
            bSpawnedByStone = true;
            if (chr_selected_stone_->get_dungeon())
                bAggressive = true;
        }

        for (uint32_t i = 0; i < c_rdwMembers.size(); ++i) {
            character *tch = spawn_mob_range(c_rdwMembers[i], map_index, sx, sy, ex, ey, true, bSpawnedByStone);

            if (!tch) {
                if (i == 0)
                    return false;

                continue;
            }

            sx = tch->x() - number(300, 500);
            sy = tch->y() - number(300, 500);
            ex = tch->x() + number(300, 500);
            ey = tch->y() + number(300, 500);

            if (chr_selected_stone_)
                tch->set_stone(chr_selected_stone_);
            else if (pkParty) {
                pkParty->join(tch->get_vid());
                pkParty->link(tch);
            } else if (!pkChrMaster) {
                pkChrMaster = tch;
                pkChrMaster->set_regen(regen_value);

                pkParty = party_manager::instance().create_party(pkChrMaster);
            }
            if (bAggressive)
                tch->set_aggressive();

            if (tch->go_to(tx, ty))
                tch->send_move_packet(FUNC_WAIT, 0, 0, 0, 0);
        }

        return true;
    }

    auto character_manager::spawn_group_group(uint32_t vnum, int32_t map_index, int sx, int sy, int ex, int ey, regen *regen_value, bool aggressive, dungeon *dungeon_value) -> bool {
        const uint32_t dwGroupID = mob_manager::instance().group_from_group_group(vnum);

        if (dwGroupID != 0) {
            return spawn_group(dwGroupID, map_index, sx, sy, ex, ey, regen_value, aggressive, dungeon_value);
        } else {
            metin2::log::error("Mob group group {:d} does not exist (map {:d})", vnum, map_index);
            return false;
        }
    }

    auto character_manager::spawn_group(uint32_t vnum, int32_t map_index, int sx, int sy, int ex, int ey, regen *regen_value, bool aggressive, dungeon *dungeon_value) -> character * {
        mob_group *pkGroup = mob_manager::instance().group(vnum);

        if (!pkGroup) {
            metin2::log::error("Mob group {:d} does not exist (map {:d})", vnum, map_index);
            return nullptr;
        }

        character *pkChrMaster = nullptr;
        party *pkParty = nullptr;

        const std::vector<uint32_t> &c_rdwMembers = pkGroup->member_vector();

        bool bSpawnedByStone = false;
        bool bAggressive = aggressive;

        if (chr_selected_stone_) {
            bSpawnedByStone = true;

            if (chr_selected_stone_->get_dungeon())
                bAggressive = true;
        }

        character *chLeader = nullptr;

        for (uint32_t i = 0; i < c_rdwMembers.size(); ++i) {
            character *tch = spawn_mob_range(c_rdwMembers[i], map_index, sx, sy, ex, ey, true, bSpawnedByStone);

            if (!tch) {
                if (i == 0)
                    return nullptr;

                continue;
            }

            if (i == 0)
                chLeader = tch;

            tch->set_dungeon(dungeon_value);

            sx = tch->x() - number(300, 500);
            sy = tch->y() - number(300, 500);
            ex = tch->x() + number(300, 500);
            ey = tch->y() + number(300, 500);

            if (chr_selected_stone_)
                tch->set_stone(chr_selected_stone_);
            else if (pkParty) {
                pkParty->join(tch->get_vid());
                pkParty->link(tch);
            } else if (!pkChrMaster) {
                pkChrMaster = tch;
                pkChrMaster->set_regen(regen_value);

                pkParty = party_manager::instance().create_party(pkChrMaster);
            }

            if (bAggressive)
                tch->set_aggressive();
        }

        return chLeader;
    }

    void character_manager::update(int pulse) {
        begin_pending_destroy();

        {
            if (!map_pc_chr_.empty()) {
                character_vector v;
                v.reserve(map_pc_chr_.size());
                for (const auto &[name, chr] : map_pc_chr_)
                    v.push_back(chr);
                if (0 == (pulse % passes_per_sec(5))) {
                    for (character *ch : v) {
                        ch->reset_chat_counter();
                        ch->fsm::update();
                    }
                } else {
                    for (character *ch : v)
                        ch->update_character(pulse);
                }
            }
        }

        {
            if (!set_chr_state_.empty()) {
                character_vector v;
                v.reserve(set_chr_state_.size());
                v.insert(v.end(), set_chr_state_.begin(), set_chr_state_.end());
                for (character *ch : v)
                    ch->update_state_machine(pulse);
            }
        }

        {
            character_vector_interactor i;

            if (character_manager::instance().characters_by_race_num(xmas::MOB_SANTA_VNUM, i)) {
                for (character *ch : i)
                    ch->update_state_machine(pulse);
            }
        }

        if (0 == (pulse % passes_per_sec(3600))) {
            for (auto it = map_mob_kill_count_.begin(); it != map_mob_kill_count_.end(); ++it)
                db_manager::instance().send_money_log(MONEY_LOG_MONSTER_KILL, it->first, it->second);

            map_mob_kill_count_.clear();
        }

        if (metin2::config::test_server() && 0 == (pulse % passes_per_sec(60)))
            metin2::log::debug("Characters: {:d} by vid, {:d} by pid", map_chr_by_vid_.size(), map_chr_by_pid_.size());

        flush_pending_destroy();
    }

    void character_manager::process_delayed_save() {
        auto it = set_chr_for_delayed_save_.begin();

        while (it != set_chr_for_delayed_save_.end()) {
            character *pkChr = *it++;
            pkChr->save_real();
        }

        set_chr_for_delayed_save_.clear();
    }

    auto character_manager::add_to_state_list(character *ch) -> bool {
        assert(ch != nullptr);

        auto it = set_chr_state_.find(ch);

        if (it == set_chr_state_.end()) {
            set_chr_state_.insert(ch);
            return true;
        }

        return false;
    }

    void character_manager::remove_from_state_list(character *ch) {
        auto it = set_chr_state_.find(ch);

        if (it != set_chr_state_.end()) {
            set_chr_state_.erase(it);
        }
    }

    void character_manager::delayed_save(character *ch) {
        set_chr_for_delayed_save_.insert(ch);
    }

    auto character_manager::flush_delayed_save(character *ch) -> bool {
        auto it = set_chr_for_delayed_save_.find(ch);

        if (it == set_chr_for_delayed_save_.end())
            return false;

        set_chr_for_delayed_save_.erase(it);
        ch->save_real();
        return true;
    }

    void character_manager::register_for_monster_log(character *ch) {
        set_chr_monster_log_.insert(ch);
    }

    void character_manager::unregister_for_monster_log(character *ch) {
        set_chr_monster_log_.erase(ch);
    }

    void character_manager::packet_monster_log(character *ch, const void *buf, int size) {
        decltype(set_chr_monster_log_.begin()) it;

        for (it = set_chr_monster_log_.begin(); it != set_chr_monster_log_.end(); ++it) {
            character *c = *it;

            if (ch && distance_approx(c->x() - ch->x(), c->y() - ch->y()) > 6000)
                continue;

            desc *d = c->get_desc();

            if (d)
                d->packet(buf, size);
        }
    }

    void character_manager::kill_log(uint32_t vnum) {
        const uint32_t SEND_LIMIT = 10000;

        auto it = map_mob_kill_count_.find(vnum);

        if (it == map_mob_kill_count_.end())
            map_mob_kill_count_.insert(std::make_pair(vnum, 1));
        else {
            ++it->second;

            if (it->second > SEND_LIMIT) {
                db_manager::instance().send_money_log(MONEY_LOG_MONSTER_KILL, it->first, it->second);
                map_mob_kill_count_.erase(it);
            }
        }
    }

    void character_manager::register_race_num(uint32_t vnum) {
        set_registered_race_num_.insert(vnum);
    }

    void character_manager::register_race_num_map(character *ch) {
        uint32_t dwVnum = ch->race_num();

        if (set_registered_race_num_.find(dwVnum) != set_registered_race_num_.end()) {
            metin2::log::debug("Registered {} in race map for vnum {:d}", ch->name(), dwVnum);
            map_chr_by_race_num_[dwVnum].insert(ch);
        }
    }

    void character_manager::unregister_race_num_map(character *ch) {
        uint32_t dwVnum = ch->race_num();

        auto it = map_chr_by_race_num_.find(dwVnum);

        if (it != map_chr_by_race_num_.end())
            it->second.erase(ch);
    }

    auto character_manager::characters_by_race_num(uint32_t race_num, character_vector_interactor &i) -> bool {
        auto it = map_chr_by_race_num_.find(race_num);

        if (it == map_chr_by_race_num_.end())
            return false;

        i = it->second;
        return true;
    }

    auto character_manager::find_specify_pc(unsigned int job_flag, int32_t map_index_value, character *except, int min_level, int max_level) -> character * {
        character *chFind = nullptr;
        decltype(map_chr_by_pid_.begin()) it;
        int n = 0;

        for (it = map_chr_by_pid_.begin(); it != map_chr_by_pid_.end(); ++it) {
            character *ch = it->second;

            if (ch == except)
                continue;

            if (ch->level() < min_level)
                continue;

            if (ch->level() > max_level)
                continue;

            if (ch->map_index() != map_index_value)
                continue;

            if (job_flag) {
                unsigned int uiChrJob = (1 << ((ch->job() + 1) * 3 + ch->skill_group()));

                if (!is_set(job_flag, uiChrJob))
                    continue;
            }

            if (!chFind || number(1, ++n) == 1)
                chFind = ch;
        }

        return chFind;
    }

    auto character_manager::mob_item_rate(character *ch) -> int {
        if (ch && ch->premium_remain_seconds(PREMIUM_ITEM) > 0)
            return mob_item_rate_premium_;
        return mob_item_rate_;
    }

    auto character_manager::mob_damage_rate(character *ch) -> int {
        return mob_damage_rate_;
    }

    auto character_manager::mob_gold_amount_rate(character *ch) -> int {
        if (!ch)
            return mob_gold_amount_rate_;

        if (ch && ch->premium_remain_seconds(PREMIUM_GOLD) > 0)
            return mob_gold_amount_rate_premium_;
        return mob_gold_amount_rate_;
    }

    auto character_manager::mob_gold_drop_rate(character *ch) -> int {
        if (!ch)
            return mob_gold_drop_rate_;

        if (ch && ch->premium_remain_seconds(PREMIUM_GOLD) > 0)
            return mob_gold_drop_rate_premium_;
        return mob_gold_drop_rate_;
    }

    auto character_manager::mob_exp_rate(character *ch) -> int {
        if (!ch)
            return mob_exp_rate_;

        if (ch && ch->premium_remain_seconds(PREMIUM_EXP) > 0)
            return mob_exp_rate_premium_;
        return mob_exp_rate_;
    }

    void character_manager::send_script_to_map(int32_t map_index, const std::string &s) {
        sectree_map *pSecMap = sectree_manager::instance().map(map_index);

        if (nullptr == pSecMap)
            return;

        struct packet_script p;

        p.header = HEADER_GC_SCRIPT;
        p.skin = 1;
        p.src_size = s.size();

        temp_buffer buf;
        p.size = p.src_size + sizeof(struct packet_script);
        buf.write(&p, sizeof(struct packet_script));
        buf.write(&s[0], s.size());

        quest::send_buffer_to_map(pSecMap, buf);
    }

    auto character_manager::begin_pending_destroy() -> bool {
        if (use_pending_destroy_)
            return false;

        use_pending_destroy_ = true;
        return true;
    }

    void character_manager::flush_pending_destroy() {
        use_pending_destroy_ = false; // The flag must be set first or nothing is destroyed.

        if (!set_chr_pending_destroy_.empty()) {
            metin2::log::debug("Destroying {:d} pending characters", set_chr_pending_destroy_.size());

            auto it = set_chr_pending_destroy_.begin(),
                 end = set_chr_pending_destroy_.end();
            for (; it != end; ++it) {
                character_manager::instance().destroy_character(*it);
            }

            set_chr_pending_destroy_.clear();
        }
    }

    character_vector_interactor::character_vector_interactor(const character_set &r) {
        reserve(r.size());
        insert(end(), r.begin(), r.end());

        if (character_manager::instance().begin_pending_destroy())
            my_begin_ = true;
    }

    character_vector_interactor::~character_vector_interactor() {
        if (my_begin_)
            character_manager::instance().flush_pending_destroy();
    }
}
