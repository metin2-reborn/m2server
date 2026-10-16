#include <metin2/config/config.h>
#include <metin2/game/characters/character.h>

#include <metin2/game/admin/gm.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/constants.h>
#include <metin2/game/app/crc32.h>
#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/characters/affect.h>
#include <metin2/game/characters/affect_flag.h>
#include <metin2/game/characters/buff_on_attributes.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/characters/fsm.h>
#include <metin2/game/characters/horsename_manager.h>
#include <metin2/game/characters/mob_manager.h>
#include <metin2/game/characters/motion.h>
#include <metin2/game/characters/pet_system.h>
#include <metin2/game/combat/pvp.h>
#include <metin2/game/combat/skill.h>
#include <metin2/game/combat/skill_power.h>
#include <metin2/game/empire/castle.h>
#include <metin2/game/empire/monarch.h>
#include <metin2/game/empire/start_position.h>
#include <metin2/game/events/arena.h>
#include <metin2/game/events/blue_dragon_binder.h>
#include <metin2/game/events/fishing.h>
#include <metin2/game/events/mining.h>
#include <metin2/game/events/xmas_event.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/guilds/guild_manager.h>
#include <metin2/game/guilds/war_map.h>
#include <metin2/game/items/dragon_soul.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/items/unique_item.h>
#include <metin2/game/parties/party.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/protocol/input.h>
#include <metin2/game/protocol/p2p.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/quest/questpc.h>
#include <metin2/game/quest/target.h>
#include <metin2/game/social/marriage.h>
#include <metin2/game/social/messenger_manager.h>
#include <metin2/game/social/wedding.h>
#include <metin2/game/text/banword.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/trade/exchange.h>
#include <metin2/game/trade/safebox.h>
#include <metin2/game/trade/shop.h>
#include <metin2/game/trade/shop_manager.h>
#include <metin2/game/world/dungeon.h>
#include <metin2/game/world/entity.h>
#include <metin2/game/world/map_location.h>
#include <metin2/game/world/regen.h>
#include <metin2/game/world/sectree.h>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/world/vector.h>
#include <metin2/game/world/vid.h>

#include <metin2/common/utils.h>
#include <metin2/core/main.h>
#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/sql/query.h>

#include <mysql.h>

#include <algorithm>
#include <cassert>
#include <climits>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <map>
#include <memory>
#include <numbers>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>

#include <arpa/inet.h>
#include <strings.h>

namespace metin2::game::inline characters {

    auto can_enter_zone(character *const &ch, int map_index) -> bool;

    auto can_enter_zone(character *const &ch, int map_index) -> bool {
        switch (map_index) {
        case 301:
        case 302:
        case 303:
        case 304:
            if (ch->level() < 90)
                return false;
        }
        return true;
    }

    auto dynamic_character_ptr::get() const -> character * {
        character *p = nullptr;
        if (is_pc) {
            p = character_manager::instance().find_by_pid(id);
        } else {
            p = character_manager::instance().find(id);
        }
        return p;
    }

    auto dynamic_character_ptr::operator=(character *character) -> dynamic_character_ptr & {
        if (character == nullptr) {
            reset();
            return *this;
        }
        if (character->is_pc()) {
            is_pc = true;
            id = character->player_id();
        } else {
            is_pc = false;
            id = character->get_vid();
        }
        return *this;
    }

    character::character() {
        state_idle_.set(this, &character::begin_state_empty, &character::state_idle, &character::end_state_empty);
        state_move_.set(this, &character::begin_state_empty, &character::state_move, &character::end_state_empty);
        state_battle_.set(this, &character::begin_state_empty, &character::state_battle, &character::end_state_empty);

        entity::initialize(ENTITY_CHARACTER);

        sync_time_ = get_float_time() - 3;

        set_position(POS_STANDING);

        play_start_time_ = last_move_time_ = get_dword_time();

        goto_state(state_idle_);

        last_attack_time_ = get_dword_time() - 20000;
        last_dead_time_ = get_dword_time() - 180000;

        reset_change_attack_position_time();

        under_guild_war_info_message_time_ = get_dword_time() - 60000;

        reset_chain_lightning_index();
        reset_stop_time();

        last_victim_set_time_ = get_dword_time() - 3000;
        last_skill_time_ = get_dword_time();

        potion_limit_ = quest::quest_manager::instance().event_flag("arena_potion_limit_count");

        init_mc();

        points_instant_.dragon_soul_active_deck = -1;
    }

    character::~character() {
        destroy();
    }

    void character::create(const char *name, uint32_t vid, bool is_pc) {
        static int s_crc = 172814;

        const std::string crc_string = std::format("{}{}{}", name, static_cast<const void *>(this), ++s_crc);
        vid_ = game::world::vid(vid, get_cr_c32(crc_string.c_str(), crc_string.size()));

        if (is_pc)
            name_ = name;
    }

    void character::destroy() {
        close_my_shop();

        if (regen_) {
            if (dungeon_) {
                if (dungeon_->is_valid_regen(regen_, regen_id_)) {
                    --regen_->count;
                }
            } else {
                --regen_->count;
            }
            regen_ = nullptr;
        }

        if (dungeon_) {
            set_dungeon(nullptr);
        }

        if (pet_system_) {
            pet_system_->destroy();
            pet_system_.reset();
        }

        horse_summon(false);

        if (rider())
            rider()->clear_horse_info();

        if (get_desc()) {
            get_desc()->bind_character(nullptr);
        }

        if (exchange_)
            exchange_->cancel();

        set_victim(nullptr);

        if (get_shop()) {
            get_shop()->remove_guest(this);
            set_shop(nullptr);
        }

        clear_stone();
        clear_sync();
        clear_target();

        if (nullptr == mob_data) {
            dragon_soul_clean_up();
            clear_item();
        }

        party *party = party_;
        if (party) {
            if (party->leader_pid() == get_vid() && !is_pc()) {
                delete party;
            } else {
                party->unlink(this);

                if (!is_pc())
                    party->quit(get_vid());
            }

            set_party(nullptr);
        }

        mob_inst.reset();

        mob_data = nullptr;

        safebox_.reset();
        mall_.reset();

        set_chr_spawned_by_.clear();

        stop_muyeong_event();
        event_cancel(&warp_npc_event);
        event_cancel(&recovery_event);
        event_cancel(&dead_event);
        event_cancel(&save_event);
        event_cancel(&timed_event);
        event_cancel(&stun_event);
        event_cancel(&fishing_event);
        event_cancel(&poison_event);
        event_cancel(&fire_event);
        event_cancel(&party_request_event_);
        event_cancel(&warp_event);
        event_cancel(&check_speed_hack_event);

        event_cancel(&mining_event);

        for (auto it = mob_skill_event.begin(); it != mob_skill_event.end(); ++it) {
            event_ptr pkEvent = it->second;
            event_cancel(&pkEvent);
        }
        mob_skill_event.clear();

        clear_affect();

        map_buff_on_attrs_.clear();

        event_cancel(&destroy_when_idle_event);

        skill_levels_.reset();

        entity::destroy();

        if (get_sectree())
            get_sectree()->remove_entity(this);

        if (monster_log_)
            character_manager::instance().unregister_for_monster_log(this);
    }

    auto character::name() const -> const char * {
        return name_.empty() ? (mob_data ? mob_data->table.locale_name : "") : name_.c_str();
    }

    void character::open_my_shop(const char *sign_value, shop_item_table *table, uint8_t item_count) {
        if (part(PART_MAIN) > 2) {
            chat_packet(CHAT_TYPE_INFO, lc_text("갑옷을 벗어야 개인 상점을 열 수 있습니다."));
            return;
        }

        if (my_shop()) {
            close_my_shop();
            return;
        }

        quest::pc *pPC = quest::quest_manager::instance().pc_force(player_id());

        if (pPC->is_running())
            return;

        if (item_count == 0)
            return;

        int64_t nTotalMoney = 0;

        for (int n = 0; n < item_count; ++n) {
            nTotalMoney += static_cast<int64_t>((table + n)->price);
        }

        nTotalMoney += static_cast<int64_t>(gold());

        if (GOLD_MAX <= nTotalMoney) {
            metin2::log::warn("Cannot open shop for {} (pid {:d}): total price exceeds the gold limit", name(), player_id());
            chat_packet(CHAT_TYPE_INFO, lc_text("20억 냥을 초과하여 상점을 열수가 없습니다"));
            return;
        }

        char szSign[SHOP_SIGN_MAX_LEN + 1];
        utf8_strlcpy(szSign, sign_value);

        shop_sign_ = szSign;

        if (shop_sign_.length() == 0)
            return;

        if (banword_manager::instance().check_string(shop_sign_.c_str(), shop_sign_.length())) {
            chat_packet(CHAT_TYPE_INFO, lc_text("비속어나 은어가 포함된 상점 이름으로 상점을 열 수 없습니다."));
            return;
        }

        std::map<uint32_t, uint32_t> itemkind;

        std::set<item_pos> cont;
        for (uint8_t i = 0; i < item_count; ++i) {
            if (cont.find((table + i)->pos) != cont.end()) {
                metin2::log::warn("Duplicate item in shop of {}", name());
                return;
            }

            item *pkItem = get_item((table + i)->pos);

            if (pkItem) {
                const item_table *item_table = pkItem->proto();

                if (item_table && (is_set(item_table->anti_flags, ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_MYSHOP))) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("유료화 아이템은 개인상점에서 판매할 수 없습니다."));
                    return;
                }

                if (pkItem->is_equipped() == true) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("장비중인 아이템은 개인상점에서 판매할 수 없습니다."));
                    return;
                }

                if (true == pkItem->is_locked()) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("사용중인 아이템은 개인상점에서 판매할 수 없습니다."));
                    return;
                }

                itemkind[pkItem->vnum()] = (table + i)->price / pkItem->count();
            }

            cont.insert((table + i)->pos);
        }

        if (count_specify_item(71049)) {

            item_price_list_table header;
            memset(&header, 0, sizeof(item_price_list_table));

            header.owner_id = player_id();
            header.count = itemkind.size();

            size_t idx = 0;
            for (auto it = itemkind.begin(); it != itemkind.end(); ++it) {
                header.price_info[idx].vnum = it->first;
                header.price_info[idx].price = it->second;
                idx++;
            }

            db_clientdesc->db_packet(HEADER_GD_MYSHOP_PRICELIST_UPDATE, get_desc()->handle(), &header, sizeof(item_price_list_table));
        } else if (count_specify_item(50200))
            remove_specify_item(50200, 1);
        else
            return;

        if (exchange_)
            exchange_->cancel();

        packet_gc_shop_sign p;

        p.header = HEADER_GC_SHOP_SIGN;
        p.vid = get_vid();
        strlcpy(p.sign, sign_value, sizeof(p.sign));

        packet_around(&p, sizeof(packet_gc_shop_sign));

        my_shop_ = shop_manager::instance().create_pc_shop(this, table, item_count);

        if (is_polymorphed() == true) {
            remove_affect(AFFECT_POLYMORPH);
        }

        if (horse()) {
            horse_summon(false, true);
        } else if (get_mount_vnum()) {
            remove_affect(AFFECT_MOUNT);
            remove_affect(AFFECT_MOUNT_BONUS);
        }
        set_polymorph(30000, true);
    }

    void character::close_my_shop() {
        if (my_shop()) {
            shop_sign_.clear();
            shop_manager::instance().destroy_pc_shop(this);
            my_shop_ = nullptr;

            packet_gc_shop_sign p;

            p.header = HEADER_GC_SHOP_SIGN;
            p.vid = get_vid();
            p.sign[0] = '\0';

            packet_around(&p, sizeof(p));

            set_polymorph(job(), true);
        }
    }

    void encode_move_packet(packet_gc_move &pack, uint32_t vid_value, uint8_t func_value, uint8_t arg_value, uint32_t x, uint32_t y, uint32_t duration_value, uint32_t time_value, uint8_t rot_value) {
        pack.header = HEADER_GC_MOVE;
        pack.func = func_value;
        pack.arg = arg_value;
        pack.vid = vid_value;
        pack.time = time_value ? time_value : get_dword_time();
        pack.rot = rot_value;
        pack.x = x;
        pack.y = y;
        pack.duration = duration_value;
    }

    void character::restart_at_same_pos() {
        if (is_observer_)
            return;

        encode_remove_packet(this);
        encode_insert_packet(this);

        entity_map::iterator it = map_view_.begin();

        while (it != map_view_.end()) {
            entity *entity = (it++)->first;

            encode_remove_packet(entity);
            if (!is_observer_)
                encode_insert_packet(entity);

            if (entity->is_type(ENTITY_CHARACTER)) {
                character *lpChar = (character *)entity;
                if (lpChar->is_pc() || lpChar->is_npc() || lpChar->is_monster()) {
                    if (!entity->is_observer_mode())
                        entity->encode_insert_packet(this);
                }
            } else {
                if (!entity->is_observer_mode()) {
                    entity->encode_insert_packet(this);
                }
            }
        }
    }

    void character::encode_insert_packet(entity *entity) {

        desc *d;

        if (!(d = entity->get_desc()))
            return;

        character *ch = (character *)entity;
        ch->send_guild_name(get_guild());

        packet_gc_character_add pack;

        pack.header = HEADER_GC_CHARACTER_ADD;
        pack.vid = vid_;
        pack.type = get_char_type();
        pack.angle = rotation();
        pack.x = x();
        pack.y = y();
        pack.z = z();
        pack.race_num = race_num();
        if (is_pet()) {
            pack.moving_speed = 150;
        } else {
            pack.moving_speed = limit_point(POINT_MOV_SPEED);
        }
        pack.attack_speed = limit_point(POINT_ATT_SPEED);
        pack.affect_flag[0] = af_affect_flag_.bits[0];
        pack.affect_flag[1] = af_affect_flag_.bits[1];

        pack.state_flag = add_chr_state_;

        int iDur = 0;

        if (pos_dest_.x != pack.x || pos_dest_.y != pack.y) {
            iDur = (move_start_time_ + move_duration_) - get_dword_time();

            if (iDur <= 0) {
                pack.x = pos_dest_.x;
                pack.y = pos_dest_.y;
            }
        }

        d->packet(&pack, sizeof(pack));

        if (is_pc() == true || char_type_ == CHAR_TYPE_NPC) {
            packet_gc_character_additional_info addPacket;
            memset(&addPacket, 0, sizeof(packet_gc_character_additional_info));

            addPacket.header = HEADER_GC_CHAR_ADDITIONAL_INFO;
            addPacket.vid = vid_;

            addPacket.part[CHR_EQUIPPART_ARMOR] = part(PART_MAIN);
            addPacket.part[CHR_EQUIPPART_WEAPON] = part(PART_WEAPON);
            addPacket.part[CHR_EQUIPPART_HEAD] = part(PART_HEAD);
            addPacket.part[CHR_EQUIPPART_HAIR] = part(PART_HAIR);

            addPacket.pk_mode = pk_mode_;
            addPacket.mount_vnum = get_mount_vnum();
            addPacket.empire = empire_;

            if (is_pc() == true) {
                addPacket.level = level();
            } else {
                addPacket.level = 0;
            }

            strlcpy(addPacket.name, name(), sizeof(addPacket.name));

            if (get_guild() != nullptr) {
                addPacket.guild_id = get_guild()->id();
            } else {
                addPacket.guild_id = 0;
            }

            addPacket.s_alignment = alignment_ / 10;

            d->packet(&addPacket, sizeof(packet_gc_character_additional_info));
        }

        if (iDur) {
            packet_gc_move pack;
            encode_move_packet(pack, get_vid(), FUNC_MOVE, 0, pos_dest_.x, pos_dest_.y, iDur, 0, (uint8_t)(rotation() / 5));
            d->packet(&pack, sizeof(pack));

            packet_gc_walk_mode p;
            p.vid = get_vid();
            p.header = HEADER_GC_WALK_MODE;
            p.mode = now_walking_ ? WALKMODE_WALK : WALKMODE_RUN;

            d->packet(&p, sizeof(p));
        }

        if (entity->is_type(ENTITY_CHARACTER) && get_desc()) {
            character *ch = (character *)entity;
            if (ch->is_walking()) {
                packet_gc_walk_mode p;
                p.vid = ch->get_vid();
                p.header = HEADER_GC_WALK_MODE;
                p.mode = ch->now_walking_ ? WALKMODE_WALK : WALKMODE_RUN;
                get_desc()->packet(&p, sizeof(p));
            }
        }

        if (my_shop()) {
            packet_gc_shop_sign p;

            p.header = HEADER_GC_SHOP_SIGN;
            p.vid = get_vid();
            strlcpy(p.sign, shop_sign_.c_str(), sizeof(p.sign));

            d->packet(&p, sizeof(packet_gc_shop_sign));
        }

        if (entity->is_type(ENTITY_CHARACTER)) {
            metin2::log::debug("{} (race {:d}) at sectree {:d} {:d} enters view of {}",
                name(), race_num(), x() / SECTREE_SIZE, y() / SECTREE_SIZE, ((character *)entity)->name());
        }
    }

    void character::encode_remove_packet(entity *entity) {
        if (entity->type() != ENTITY_CHARACTER)
            return;

        desc *d;

        if (!(d = entity->get_desc()))
            return;

        packet_gc_character_delete pack;

        pack.header = HEADER_GC_CHARACTER_DEL;
        pack.id = vid_;

        d->packet(&pack, sizeof(packet_gc_character_delete));

        if (entity->is_type(ENTITY_CHARACTER))
            metin2::log::debug("{} (vid {:d}) leaves view of {}", name(), (uint32_t)vid_, ((character *)entity)->name());
    }

    void character::update_packet() {
        if (get_sectree() == nullptr) return;

        packet_gc_character_update pack;

        pack.header = HEADER_GC_CHARACTER_UPDATE;
        pack.vid = vid_;

        pack.part[CHR_EQUIPPART_ARMOR] = part(PART_MAIN);
        pack.part[CHR_EQUIPPART_WEAPON] = part(PART_WEAPON);
        pack.part[CHR_EQUIPPART_HEAD] = part(PART_HEAD);
        pack.part[CHR_EQUIPPART_HAIR] = part(PART_HAIR);

        pack.moving_speed = limit_point(POINT_MOV_SPEED);
        pack.attack_speed = limit_point(POINT_ATT_SPEED);
        pack.state_flag = add_chr_state_;
        pack.affect_flag[0] = af_affect_flag_.bits[0];
        pack.affect_flag[1] = af_affect_flag_.bits[1];
        pack.guild_id = 0;
        pack.s_alignment = alignment_ / 10;
        pack.pk_mode = pk_mode_;

        if (get_guild())
            pack.guild_id = get_guild()->id();

        pack.mount_vnum = get_mount_vnum();

        packet_around(&pack, sizeof(pack));
    }

    auto character::find_character_in_view(const char *name_value, bool find_pc_only) -> character * {
        entity_map::iterator it = map_view_.begin();

        for (; it != map_view_.end(); ++it) {
            if (!it->first->is_type(ENTITY_CHARACTER))
                continue;

            character *tch = (character *)it->first;

            if (find_pc_only && tch->is_npc())
                continue;

            if (!strcasecmp(tch->name(), name_value))
                return (tch);
        }

        return nullptr;
    }

    void character::set_position(int pos) {
        if (pos == POS_STANDING) {
            remove_bit(add_chr_state_, ADD_CHARACTER_STATE_DEAD);
            remove_bit(points_instant_.instant_flag, INSTANT_FLAG_STUN);

            event_cancel(&dead_event);
            event_cancel(&stun_event);
        } else if (pos == POS_DEAD)
            set_bit(add_chr_state_, ADD_CHARACTER_STATE_DEAD);

        if (!is_stone()) {
            switch (pos) {
            case POS_FIGHTING:
                if (!is_state(state_battle_))
                    monster_log("[BATTLE] 싸우는 상태");

                goto_state(state_battle_);
                break;

            default:
                if (!is_state(state_idle_))
                    monster_log("[IDLE] 쉬는 상태");

                goto_state(state_idle_);
                break;
            }
        }

        points_instant_.position = pos;
    }

    void character::save() {
        if (!skip_save_)
            character_manager::instance().delayed_save(this);
    }

    void character::create_player_proto(player_table &tab) {
        memset(&tab, 0, sizeof(player_table));

        if (new_name().empty()) {
            strlcpy(tab.name, name(), sizeof(tab.name));
        } else {
            strlcpy(tab.name, new_name().c_str(), sizeof(tab.name));
        }

        strlcpy(tab.ip, get_desc()->host_name(), sizeof(tab.ip));

        tab.id = player_id_;
        tab.voice = point(POINT_VOICE);
        tab.level = level();
        tab.level_step = point(POINT_LEVEL_STEP);
        tab.exp = get_exp();
        tab.gold = gold();
        tab.job = points_.job;
        tab.part_base = points_instant_.base_part;
        tab.skill_group = points_.skill_group;

        uint32_t dwPlayedTime = (get_dword_time() - play_start_time_);

        if (dwPlayedTime > 60000) {
            if (get_sectree() && !get_sectree()->is_attr(x(), y(), ATTR_BANPK)) {
                if (real_alignment() < 0) {
                    if (is_equip_unique_item(UNIQUE_ITEM_FASTER_ALIGNMENT_UP_BY_TIME))
                        update_alignment(120 * (dwPlayedTime / 60000));
                    else
                        update_alignment(60 * (dwPlayedTime / 60000));
                } else
                    update_alignment(5 * (dwPlayedTime / 60000));
            }

            set_real_point(POINT_PLAYTIME, real_point(POINT_PLAYTIME) + dwPlayedTime / 60000);
            reset_play_time(dwPlayedTime % 60000);
        }

        tab.playtime = real_point(POINT_PLAYTIME);
        tab.alignment = real_alignment_;

        if (pos_warp_.x != 0 || pos_warp_.y != 0) {
            tab.x = pos_warp_.x;
            tab.y = pos_warp_.y;
            tab.z = 0;
            tab.map_index = warp_map_index_;
        } else {
            tab.x = x();
            tab.y = y();
            tab.z = z();
            tab.map_index = map_index();
        }

        if (exit_map_index_ == 0) {
            tab.exit_map_index = tab.map_index;
            tab.exit_x = tab.x;
            tab.exit_y = tab.y;
        } else {
            tab.exit_map_index = exit_map_index_;
            tab.exit_x = pos_exit_.x;
            tab.exit_y = pos_exit_.y;
        }

        metin2::log::debug("Saving {} at {:d}x{:d}", name(), tab.x, tab.y);

        tab.st = real_point(POINT_ST);
        tab.ht = real_point(POINT_HT);
        tab.dx = real_point(POINT_DX);
        tab.iq = real_point(POINT_IQ);

        tab.stat_point = point(POINT_STAT);
        tab.skill_point = point(POINT_SKILL);
        tab.sub_skill_point = point(POINT_SUB_SKILL);
        tab.horse_skill_point = point(POINT_HORSE_SKILL);

        tab.stat_reset_count = point(POINT_STAT_RESET_COUNT);

        tab.hp = hp();
        tab.sp = sp();

        tab.stamina = stamina();

        tab.s_random_hp = points_.random_hp;
        tab.s_random_sp = points_.random_sp;

        for (int i = 0; i < QUICKSLOT_MAX_NUM; ++i)
            tab.quickslot[i] = quickslot_[i];

        if (mobile_.length() && mobile_auth_.empty())
            strlcpy(tab.mobile, mobile_.c_str(), sizeof(tab.mobile));

        std::memcpy(tab.parts, points_instant_.parts, sizeof(tab.parts));

        std::memcpy(tab.skills, skill_levels_.get(), sizeof(player_skill) * SKILL_MAX_NUM);

        tab.horse = horse_data();
    }

    void character::save_real() {
        if (skip_save_)
            return;

        if (!get_desc()) {
            metin2::log::error("Cannot save {}: no descriptor", name());
            return;
        }

        player_table table;
        create_player_proto(table);

        db_clientdesc->db_packet(HEADER_GD_PLAYER_SAVE, get_desc()->handle(), &table, sizeof(player_table));

        quest::pc *pkQuestPC = quest::quest_manager::instance().pc_force(player_id());

        if (!pkQuestPC)
            metin2::log::error("Cannot save quest state of {}: no quest pc", name());
        else {
            pkQuestPC->save();
        }

        marriage::marriage *pMarriage = marriage::manager::instance().get(player_id());
        if (pMarriage)
            pMarriage->save();
    }

    void character::flush_delayed_save_item() {
        item *item;

        for (int i = 0; i < INVENTORY_AND_EQUIP_SLOT_MAX; ++i)
            if ((item = inventory_item(i)))
                item_manager::instance().flush_delayed_save(item);
    }

    void character::disconnect(const char *reason) {
        assert(get_desc() != nullptr);

        metin2::log::info("{} disconnected ({})", name(), reason ? reason : "no reason");

        if (get_shop()) {
            get_shop()->remove_guest(this);
            set_shop(nullptr);
        }

        if (get_arena() != nullptr) {
            get_arena()->on_disconnect(player_id());
        }

        if (get_party() != nullptr) {
            get_party()->update_offline_state(player_id());
        }

        marriage::manager::instance().logout(this);

        packet_gg_logout p;
        p.header = HEADER_GG_LOGOUT;
        strlcpy(p.name, name(), sizeof(p.name));
        p2p_manager::instance().send(&p, sizeof(packet_gg_logout));
        const std::string buf = std::format("{} {} {} {} {}",
            get_desc()->host_name(), gold(), metin2::config::server().channel, map_index(), alignment());

        log_manager::instance().char_log(this, 0, "LOGOUT", buf.c_str());

        if (war_map_)
            set_war_map(nullptr);

        if (wedding_map_) {
            set_wedding_map(nullptr);
        }

        if (get_guild())
            get_guild()->logout_member(this);

        quest::quest_manager::instance().logout_pc(this);

        if (get_party())
            get_party()->unlink(this);

        if (is_stun() || is_dead()) {
            death_penalty(0);
            point_change(POINT_HP, 50 - hp());
        }

        if (!character_manager::instance().flush_delayed_save(this)) {
            save_real();
        }

        flush_delayed_save_item();

        save_affect();

        skip_save_ = true; // Nothing may be saved after this point.

        quest::quest_manager::instance().disconnect_pc(this);

        close_safebox();

        close_mall();

        pvp_manager::instance().disconnect(this);

        target_manager::instance().logout(player_id());

        messenger_manager::instance().logout(name());

        if (get_desc()) {
            get_desc()->bind_character(nullptr);
        }

        character_manager::instance().destroy_character(this);
    }

    auto character::show(int32_t map_index, int32_t x, int32_t y, int32_t z, bool show_spawn_motion) -> bool {
        sectree *sectree = sectree_manager::instance().get(map_index, x, y);

        if (!sectree) {
            metin2::log::warn("Cannot show character: no sectree at {:d}x{:d} (map {:d})", x, y, map_index);
            return false;
        }

        set_map_index(map_index);

        bool bChangeTree = false;

        if (!get_sectree() || get_sectree() != sectree)
            bChangeTree = true;

        if (bChangeTree) {
            if (get_sectree())
                get_sectree()->remove_entity(this);

            view_cleanup();
        }

        if (!is_npc()) {
            metin2::log::debug("Showing {} at {:d}x{:d}x{:d}", name(), x, y, z);
            if (stamina() < max_stamina())
                start_affect_event();
        } else if (mob_data) {
            mob_inst->pos_last_attacked.x = x;
            mob_inst->pos_last_attacked.y = y;
            mob_inst->pos_last_attacked.z = z;
        }

        if (show_spawn_motion) {
            set_bit(add_chr_state_, ADD_CHARACTER_STATE_SPAWN);
            af_affect_flag_.set(AFF_SPAWN);
        }

        set_xyz(x, y, z);

        pos_dest_.x = x;
        pos_dest_.y = y;
        pos_dest_.z = z;

        pos_start_.x = x;
        pos_start_.y = y;
        pos_start_.z = z;

        if (bChangeTree) {
            encode_insert_packet(this);
            sectree->insert_entity(this);

            update_sectree();
        } else {
            view_reencode();
        }

        remove_bit(add_chr_state_, ADD_CHARACTER_STATE_SPAWN);

        set_valid_combo_interval(0);
        return true;
    }

    struct bgm_info {
        std::string name;
        float vol;
    };

    typedef std::map<unsigned, bgm_info> bgm_info_map;

    static bgm_info_map gs_bgm_info_map;
    static bool bgm_vol_enable = false;

    void character_set_bgm_volume_enable() {
        bgm_vol_enable = true;
        metin2::log::info("Bgm: volume control enabled");
    }

    void character_add_bgm_info(unsigned index, const char *name, float vol) {
        bgm_info newInfo;
        newInfo.name = name;
        newInfo.vol = vol;

        gs_bgm_info_map[index] = newInfo;

        metin2::log::debug("Bgm: map {:d} uses {} at volume {:f}", index, name, vol);
    }

    auto character_get_bgm_info(unsigned index) -> const bgm_info & {
        bgm_info_map::iterator f = gs_bgm_info_map.find(index);
        if (gs_bgm_info_map.end() == f) {
            static bgm_info s_empty = {"", 0.0f};
            return s_empty;
        }
        return f->second;
    }

    auto character_is_bgm_volume_enable() -> bool {
        return bgm_vol_enable;
    }

    void character::main_character_packet() {
        const unsigned mapIndex = map_index();
        const bgm_info &bgmInfo = character_get_bgm_info(mapIndex);

        if (!bgmInfo.name.empty()) {
            if (character_is_bgm_volume_enable()) {
                metin2::log::debug("Bgm: map {:d} plays {} at volume {:f}", mapIndex, bgmInfo.name, bgmInfo.vol);
                packet_gc_main_character4_bgm_vol mainChrPacket;
                mainChrPacket.header = HEADER_GC_MAIN_CHARACTER4_BGM_VOL;
                mainChrPacket.vid = vid_;
                mainChrPacket.race_num = race_num();
                mainChrPacket.lx = x();
                mainChrPacket.ly = y();
                mainChrPacket.lz = z();
                mainChrPacket.empire = get_desc()->empire();
                mainChrPacket.skill_group = skill_group();
                strlcpy(mainChrPacket.chr_name, name(), sizeof(mainChrPacket.chr_name));

                mainChrPacket.bgm_vol = bgmInfo.vol;
                strlcpy(mainChrPacket.bgm_name, bgmInfo.name.c_str(), sizeof(mainChrPacket.bgm_name));
                get_desc()->packet(&mainChrPacket, sizeof(packet_gc_main_character4_bgm_vol));
            } else {
                metin2::log::debug("Bgm: map {:d} plays {}", mapIndex, bgmInfo.name);
                packet_gc_main_character3_bgm mainChrPacket;
                mainChrPacket.header = HEADER_GC_MAIN_CHARACTER3_BGM;
                mainChrPacket.vid = vid_;
                mainChrPacket.race_num = race_num();
                mainChrPacket.lx = x();
                mainChrPacket.ly = y();
                mainChrPacket.lz = z();
                mainChrPacket.empire = get_desc()->empire();
                mainChrPacket.skill_group = skill_group();
                strlcpy(mainChrPacket.chr_name, name(), sizeof(mainChrPacket.chr_name));
                strlcpy(mainChrPacket.bgm_name, bgmInfo.name.c_str(), sizeof(mainChrPacket.bgm_name));
                get_desc()->packet(&mainChrPacket, sizeof(packet_gc_main_character3_bgm));
            }
        } else {
            metin2::log::debug("Bgm: map {:d} plays the default bgm", mapIndex);

            packet_gc_main_character pack;
            pack.header = HEADER_GC_MAIN_CHARACTER;
            pack.vid = vid_;
            pack.race_num = race_num();
            pack.lx = x();
            pack.ly = y();
            pack.lz = z();
            pack.empire = get_desc()->empire();
            pack.skill_group = skill_group();
            strlcpy(pack.name, name(), sizeof(pack.name));
            get_desc()->packet(&pack, sizeof(packet_gc_main_character));

            if (mobile_.length())
                chat_packet(CHAT_TYPE_COMMAND, "sms");
        }
    }

    void character::points_packet() {
        if (!get_desc())
            return;

        packet_gc_points pack;

        pack.header = HEADER_GC_CHARACTER_POINTS;

        pack.points[POINT_LEVEL] = level();
        pack.points[POINT_EXP] = get_exp();
        pack.points[POINT_NEXT_EXP] = next_exp();
        pack.points[POINT_HP] = hp();
        pack.points[POINT_MAX_HP] = max_hp();
        pack.points[POINT_SP] = sp();
        pack.points[POINT_MAX_SP] = max_sp();
        pack.points[POINT_GOLD] = gold();
        pack.points[POINT_STAMINA] = stamina();
        pack.points[POINT_MAX_STAMINA] = max_stamina();

        for (int i = POINT_ST; i < POINT_MAX_NUM; ++i)
            pack.points[i] = point(i);

        get_desc()->packet(&pack, sizeof(packet_gc_points));
    }

    auto character::change_sex() -> bool {
        int src_race = race_num();

        switch (src_race) {
        case MAIN_RACE_WARRIOR_M:
            points_.job = MAIN_RACE_WARRIOR_W;
            break;

        case MAIN_RACE_WARRIOR_W:
            points_.job = MAIN_RACE_WARRIOR_M;
            break;

        case MAIN_RACE_ASSASSIN_M:
            points_.job = MAIN_RACE_ASSASSIN_W;
            break;

        case MAIN_RACE_ASSASSIN_W:
            points_.job = MAIN_RACE_ASSASSIN_M;
            break;

        case MAIN_RACE_SURA_M:
            points_.job = MAIN_RACE_SURA_W;
            break;

        case MAIN_RACE_SURA_W:
            points_.job = MAIN_RACE_SURA_M;
            break;

        case MAIN_RACE_SHAMAN_M:
            points_.job = MAIN_RACE_SHAMAN_W;
            break;

        case MAIN_RACE_SHAMAN_W:
            points_.job = MAIN_RACE_SHAMAN_M;
            break;

        default:
            metin2::log::error("Cannot change sex of {}: unknown race {:d}", name(), src_race);
            return false;
        }

        metin2::log::info("{}: sex changed (race {:d} -> {:d})", name(), src_race, points_.job);
        return true;
    }

    auto character::race_num() const -> uint16_t {
        if (polymorph_race_)
            return polymorph_race_;

        if (mob_data)
            return mob_data->table.vnum;

        return points_.job;
    }

    void character::set_race(uint8_t race) {
        if (race >= MAIN_RACE_MAX_NUM) {
            metin2::log::error("Cannot set race of {}: race {:d} out of range", name(), race);
            return;
        }

        points_.job = race;
    }

    auto character::job() const -> uint8_t {
        unsigned race = points_.job;
        unsigned job;

        if (race_to_job(race, &job))
            return job;

        metin2::log::error("Cannot get job of {}: race {:d} out of range", name(), race);
        return JOB_WARRIOR;
    }

    void character::set_level(uint8_t level) {
        points_.level = level;

        if (is_pc()) {
            if (level < metin2::config::game().pk_protect_level)
                set_mode(PK_MODE_PROTECT);
            else if (gm_level() != GM_PLAYER)
                set_mode(PK_MODE_PROTECT);
            else if (pk_mode_ == PK_MODE_PROTECT)
                set_mode(PK_MODE_PEACE);
        }
    }

    void character::set_empire(uint8_t empire) {
        empire_ = empire;
    }

    void character::set_player_proto(const player_table *t) {
        if (!get_desc() || !*get_desc()->host_name())
            metin2::log::error("Loading player without a descriptor or host name");
        else
            set_gm_level();

        char_type_ = CHAR_TYPE_PC;

        player_id_ = t->id;

        alignment_ = t->alignment;
        real_alignment_ = t->alignment;

        points_.voice = t->voice;

        points_.skill_group = t->skill_group;

        points_instant_.base_part = t->part_base;
        set_part(PART_HAIR, t->parts[PART_HAIR]);

        points_.random_hp = t->s_random_hp;
        points_.random_sp = t->s_random_sp;

        skill_levels_ = std::make_unique<player_skill[]>(SKILL_MAX_NUM);
        std::memcpy(skill_levels_.get(), t->skills, sizeof(player_skill) * SKILL_MAX_NUM);

        if (t->map_index >= 10000) {
            pos_warp_.x = t->exit_x;
            pos_warp_.y = t->exit_y;
            warp_map_index_ = t->exit_map_index;
        }

        set_real_point(POINT_PLAYTIME, t->playtime);
        set_real_point(POINT_ST, t->st);
        set_real_point(POINT_HT, t->ht);
        set_real_point(POINT_DX, t->dx);
        set_real_point(POINT_IQ, t->iq);

        set_point(POINT_ST, t->st);
        set_point(POINT_HT, t->ht);
        set_point(POINT_DX, t->dx);
        set_point(POINT_IQ, t->iq);

        set_point(POINT_STAT, t->stat_point);
        set_point(POINT_SKILL, t->skill_point);
        set_point(POINT_SUB_SKILL, t->sub_skill_point);
        set_point(POINT_HORSE_SKILL, t->horse_skill_point);

        set_point(POINT_STAT_RESET_COUNT, t->stat_reset_count);

        set_point(POINT_LEVEL_STEP, t->level_step);
        set_real_point(POINT_LEVEL_STEP, t->level_step);

        set_race(t->job);

        set_level(t->level);
        set_exp(t->exp);
        set_gold(t->gold);

        set_map_index(t->map_index);
        set_xyz(t->x, t->y, t->z);

        compute_points();

        set_hp(t->hp);
        set_sp(t->sp);
        set_stamina(t->stamina);

        if (!metin2::config::test_server()) {
            if (gm_level() > GM_LOW_WIZARD) {
                af_affect_flag_.set(AFF_YMIR);
                pk_mode_ = PK_MODE_PROTECT;
            }
        }

        if (level() < metin2::config::game().pk_protect_level)
            pk_mode_ = PK_MODE_PROTECT;

        mobile_ = t->mobile;

        set_horse_data(t->horse);

        if (horse_level() > 0)
            update_horse_data_by_logoff(t->logoff_interval);

        std::memcpy(premium_times_, t->premium_times, sizeof(t->premium_times));

        log_off_interval_ = t->logoff_interval;

        metin2::log::info("Player {} loaded: premium {:d} {:d}, logoff interval {:d} ({})", t->name, premium_times_[0], premium_times_[1], t->logoff_interval, static_cast<const void *>(this));

        if (gm_level() != GM_PLAYER) {
            log_manager::instance().char_log(this, gm_level(), "GM_LOGIN", "");
            metin2::log::info("GM {} (pid {:d}, level {:d}) logged in at {:d} {:d}", name(), player_id(), gm_level(), x(), y());
        }

        if (pet_system_) {
            pet_system_->destroy();
        }

        pet_system_ = std::make_unique<pet_system>(this);
    }

    EVENTFUNC(kill_ore_load_event) {
        char_event_info *info = dynamic_cast<char_event_info *>(event->info);
        if (info == nullptr) {
            metin2::log::error("Ore kill load event fired without event info");
            return 0;
        }

        character *ch = info->ch;
        if (ch == nullptr) {
            return 0;
        }

        ch->mining_event = nullptr;
        character_manager::instance().destroy_character(ch);
        return 0;
    }

    void character::set_proto(const mob *mob_value) {
        mob_data = mob_value;
        mob_inst = std::make_unique<mob_instance>();

        pk_mode_ = PK_MODE_FREE;

        const mob_table *t = &mob_data->table;

        char_type_ = t->type;

        set_level(t->level);
        set_empire(t->empire);

        set_exp(t->exp);
        set_real_point(POINT_ST, t->str);
        set_real_point(POINT_DX, t->dex);
        set_real_point(POINT_HT, t->con);
        set_real_point(POINT_IQ, t->iq);

        compute_points();

        set_hp(max_hp());
        set_sp(max_sp());

        points_instant_.ai_flag = t->ai_flag;
        set_immune_flag(t->immune_flag);

        assign_triggers(t);

        apply_mob_attribute(t);

        if (is_stone()) {
            determine_drop_metin_stone();
        }

        if (is_warp() || is_goto()) {
            start_warp_npc_event();
        }

        character_manager::instance().register_race_num_map(this);

        if (race_num() == xmas::MOB_SANTA_VNUM) {
            set_point(POINT_ATT_GRADE_BONUS, 10);
            set_point(POINT_DEF_GRADE_BONUS, 6);

            play_start_time_ = get_dword_time() + 30 * 1000;
            if (metin2::config::test_server())
                play_start_time_ = get_dword_time() + 30 * 1000;
        }

        if (warmap::is_war_flag(race_num())) {
            state_idle_.set(this, &character::begin_state_empty, &character::state_flag, &character::end_state_empty);
            state_move_.set(this, &character::begin_state_empty, &character::state_flag, &character::end_state_empty);
            state_battle_.set(this, &character::begin_state_empty, &character::state_flag, &character::end_state_empty);
        }

        if (warmap::is_war_flag_base(race_num())) {
            state_idle_.set(this, &character::begin_state_empty, &character::state_flag_base, &character::end_state_empty);
            state_move_.set(this, &character::begin_state_empty, &character::state_flag_base, &character::end_state_empty);
            state_battle_.set(this, &character::begin_state_empty, &character::state_flag_base, &character::end_state_empty);
        }

        if (char_type_ == CHAR_TYPE_HORSE ||
            race_num() == 20101 ||
            race_num() == 20102 ||
            race_num() == 20103 ||
            race_num() == 20104 ||
            race_num() == 20105 ||
            race_num() == 20106 ||
            race_num() == 20107 ||
            race_num() == 20108 ||
            race_num() == 20109) {
            state_idle_.set(this, &character::begin_state_empty, &character::state_horse, &character::end_state_empty);
            state_move_.set(this, &character::begin_state_empty, &character::state_move, &character::end_state_empty);
            state_battle_.set(this, &character::begin_state_empty, &character::state_horse, &character::end_state_empty);
        }

        if (mining::is_vein_of_ore(race_num())) {
            char_event_info *info = alloc_event_info<char_event_info>();

            info->ch = this;

            mining_event = event_create(kill_ore_load_event, info, passes_per_sec(number(7 * 60, 15 * 60)));
        }
    }

    auto character::get_mob_table() const -> const mob_table & {
        return mob_data->table;
    }

    auto character::is_race_flag(uint32_t bit) const -> bool {
        return mob_data ? is_set(mob_data->table.race_flag, bit) : 0;
    }

    auto character::mob_damage_min() const -> uint32_t {
        return mob_data->table.damage_range[0];
    }

    auto character::mob_damage_max() const -> uint32_t {
        return mob_data->table.damage_range[1];
    }

    auto character::mob_damage_multiply() const -> float {
        float fDamMultiply = get_mob_table().dam_multiply;

        if (is_berserk())
            fDamMultiply = fDamMultiply * 2.0f;

        return fDamMultiply;
    }

    auto character::mob_drop_item_vnum() const -> uint32_t {
        return mob_data->table.drop_item_vnum;
    }

    auto character::is_summon_monster() const -> bool {
        return summon_vnum() != 0;
    }

    auto character::summon_vnum() const -> uint32_t {
        return mob_data ? mob_data->table.summon_vnum : 0;
    }

    auto character::polymorph_item_vnum() const -> uint32_t {
        return mob_data ? mob_data->table.polymorph_item_vnum : 0;
    }

    auto character::monster_drain_sp_point() const -> uint32_t {
        return mob_data ? mob_data->table.drain_sp : 0;
    }

    auto character::get_mob_rank() const -> uint8_t {
        if (!mob_data)
            return MOB_RANK_KNIGHT;

        return mob_data->table.rank;
    }

    auto character::mob_attack_range() const -> uint16_t {
        switch (mob_battle_type()) {
        case BATTLE_TYPE_RANGE:
        case BATTLE_TYPE_MAGIC:
            return mob_data->table.attack_range + point(POINT_BOW_DISTANCE);
        default:
            return mob_data->table.attack_range;
        }
    }

    auto character::mob_battle_type() const -> uint8_t {
        if (!mob_data)
            return BATTLE_TYPE_MELEE;

        return (mob_data->table.battle_type);
    }

    void character::compute_battle_points() {
        if (is_polymorphed()) {
            uint32_t dwMobVnum = polymorph_vnum();
            const mob *pMob = mob_manager::instance().get(dwMobVnum);
            int iAtt = 0;
            int iDef = 0;

            if (pMob) {
                iAtt = level() * 2 + polymorph_point(POINT_ST) * 2;
                iDef = level() + polymorph_point(POINT_HT) + pMob->table.def;
            }

            set_point(POINT_ATT_GRADE, iAtt);
            set_point(POINT_DEF_GRADE, iDef);
            set_point(POINT_MAGIC_ATT_GRADE, point(POINT_ATT_GRADE));
            set_point(POINT_MAGIC_DEF_GRADE, point(POINT_DEF_GRADE));
        } else if (is_pc()) {
            set_point(POINT_ATT_GRADE, 0);
            set_point(POINT_DEF_GRADE, 0);
            set_point(POINT_CLIENT_DEF_GRADE, 0);
            set_point(POINT_MAGIC_ATT_GRADE, point(POINT_ATT_GRADE));
            set_point(POINT_MAGIC_DEF_GRADE, point(POINT_DEF_GRADE));

            int iAtk = level() * 2;
            int iStatAtk = 0;

            switch (job()) {
            case JOB_WARRIOR:
            case JOB_SURA:
                iStatAtk = (2 * point(POINT_ST));
                break;

            case JOB_ASSASSIN:
                iStatAtk = (4 * point(POINT_ST) + 2 * point(POINT_DX)) / 3;
                break;

            case JOB_SHAMAN:
                iStatAtk = (4 * point(POINT_ST) + 2 * point(POINT_IQ)) / 3;
                break;

            default:
                metin2::log::error("{} has invalid job {:d}", name(), job());
                iStatAtk = (2 * point(POINT_ST));
                break;
            }

            if (get_mount_vnum() && iStatAtk < 2 * point(POINT_ST))
                iStatAtk = (2 * point(POINT_ST));

            iAtk += iStatAtk;

            if (get_mount_vnum()) {
                if (job() == JOB_SURA && skill_group() == 1) {
                    iAtk += (iAtk * horse_level()) / 60;
                } else {
                    iAtk += (iAtk * horse_level()) / 30;
                }
            }

            iAtk += point(POINT_ATT_GRADE_BONUS);

            point_change(POINT_ATT_GRADE, iAtk);

            int iShowDef = level() + point(POINT_HT);
            int iDef = level() + (int)(point(POINT_HT) / 1.25);
            int iArmor = 0;

            item *pkItem;

            for (int i = 0; i < WEAR_MAX_NUM; ++i)
                if ((pkItem = wear(i)) && pkItem->type() == ITEM_ARMOR) {
                    if (pkItem->sub_type() == ARMOR_BODY || pkItem->sub_type() == ARMOR_HEAD || pkItem->sub_type() == ARMOR_FOOTS || pkItem->sub_type() == ARMOR_SHIELD) {
                        iArmor += pkItem->value(1);
                        iArmor += (2 * pkItem->value(5));
                    }
                }

            if (true == is_horse_riding()) {
                if (iArmor < horse_armor())
                    iArmor = horse_armor();

                const char *pHorseName = horse_name_manager::instance().horse_name(player_id());

                if (pHorseName != nullptr && strlen(pHorseName)) {
                    iArmor += 20;
                }
            }

            iArmor += point(POINT_DEF_GRADE_BONUS);
            iArmor += point(POINT_PARTY_DEFENDER_BONUS);

            point_change(POINT_DEF_GRADE, iDef + iArmor);
            point_change(POINT_CLIENT_DEF_GRADE, (iShowDef + iArmor) - point(POINT_DEF_GRADE));

            point_change(POINT_MAGIC_ATT_GRADE, level() * 2 + point(POINT_IQ) * 2 + point(POINT_MAGIC_ATT_GRADE_BONUS));
            point_change(POINT_MAGIC_DEF_GRADE, level() + (point(POINT_IQ) * 3 + point(POINT_HT)) / 3 + iArmor / 2 + point(POINT_MAGIC_DEF_GRADE_BONUS));
        } else {
            int iAtt = level() * 2 + point(POINT_ST) * 2;
            int iDef = level() + point(POINT_HT) + get_mob_table().def;

            set_point(POINT_ATT_GRADE, iAtt);
            set_point(POINT_DEF_GRADE, iDef);
            set_point(POINT_MAGIC_ATT_GRADE, point(POINT_ATT_GRADE));
            set_point(POINT_MAGIC_DEF_GRADE, point(POINT_DEF_GRADE));
        }
    }

    void character::compute_points() {
        int32_t lStat = point(POINT_STAT);
        int32_t lStatResetCount = point(POINT_STAT_RESET_COUNT);
        int32_t lSkillActive = point(POINT_SKILL);
        int32_t lSkillSub = point(POINT_SUB_SKILL);
        int32_t lSkillHorse = point(POINT_HORSE_SKILL);
        int32_t lLevelStep = point(POINT_LEVEL_STEP);

        int32_t lAttackerBonus = point(POINT_PARTY_ATTACKER_BONUS);
        int32_t lTankerBonus = point(POINT_PARTY_TANKER_BONUS);
        int32_t lBufferBonus = point(POINT_PARTY_BUFFER_BONUS);
        int32_t lSkillMasterBonus = point(POINT_PARTY_SKILL_MASTER_BONUS);
        int32_t lHasteBonus = point(POINT_PARTY_HASTE_BONUS);
        int32_t lDefenderBonus = point(POINT_PARTY_DEFENDER_BONUS);

        int32_t lHPRecovery = point(POINT_HP_RECOVERY);
        int32_t lSPRecovery = point(POINT_SP_RECOVERY);

        memset(points_instant_.points, 0, sizeof(points_instant_.points));
        buff_on_attr_clear_all();
        skill_damage_bonus_.clear();

        set_point(POINT_STAT, lStat);
        set_point(POINT_SKILL, lSkillActive);
        set_point(POINT_SUB_SKILL, lSkillSub);
        set_point(POINT_HORSE_SKILL, lSkillHorse);
        set_point(POINT_LEVEL_STEP, lLevelStep);
        set_point(POINT_STAT_RESET_COUNT, lStatResetCount);

        set_point(POINT_ST, real_point(POINT_ST));
        set_point(POINT_HT, real_point(POINT_HT));
        set_point(POINT_DX, real_point(POINT_DX));
        set_point(POINT_IQ, real_point(POINT_IQ));

        set_part(PART_MAIN, original_part(PART_MAIN));
        set_part(PART_WEAPON, original_part(PART_WEAPON));
        set_part(PART_HEAD, original_part(PART_HEAD));
        set_part(PART_HAIR, original_part(PART_HAIR));

        set_point(POINT_PARTY_ATTACKER_BONUS, lAttackerBonus);
        set_point(POINT_PARTY_TANKER_BONUS, lTankerBonus);
        set_point(POINT_PARTY_BUFFER_BONUS, lBufferBonus);
        set_point(POINT_PARTY_SKILL_MASTER_BONUS, lSkillMasterBonus);
        set_point(POINT_PARTY_HASTE_BONUS, lHasteBonus);
        set_point(POINT_PARTY_DEFENDER_BONUS, lDefenderBonus);

        set_point(POINT_HP_RECOVERY, lHPRecovery);
        set_point(POINT_SP_RECOVERY, lSPRecovery);

        set_point(POINT_PC_BANG_EXP_BONUS, 0);
        set_point(POINT_PC_BANG_DROP_BONUS, 0);

        int iMaxHP, iMaxSP;
        int iMaxStamina;

        if (is_pc()) {
            iMaxHP = job_initial_points[job()].max_hp + points_.random_hp + point(POINT_HT) * job_initial_points[job()].hp_per_ht;
            iMaxSP = job_initial_points[job()].max_sp + points_.random_sp + point(POINT_IQ) * job_initial_points[job()].sp_per_iq;
            iMaxStamina = job_initial_points[job()].max_stamina + point(POINT_HT) * job_initial_points[job()].stamina_per_con;

            {
                skill_proto *pkSk = skill_manager::instance().get(SKILL_ADD_HP);

                if (nullptr != pkSk) {
                    pkSk->set_point_var("k", 1.0f * skill_power(SKILL_ADD_HP) / 100.0f);

                    iMaxHP += static_cast<int>(pkSk->point_poly.eval());
                }
            }

            set_point(POINT_MOV_SPEED, 100);
            set_point(POINT_ATT_SPEED, 100);
            point_change(POINT_ATT_SPEED, point(POINT_PARTY_HASTE_BONUS));
            set_point(POINT_CASTING_SPEED, 100);
        } else {
            iMaxHP = mob_data->table.max_hp;
            iMaxSP = 0;
            iMaxStamina = 0;

            set_point(POINT_ATT_SPEED, mob_data->table.s_attack_speed);
            set_point(POINT_MOV_SPEED, mob_data->table.s_moving_speed);
            set_point(POINT_CASTING_SPEED, mob_data->table.s_attack_speed);
        }

        if (is_pc()) {
            if (get_mount_vnum()) {
                if (horse_st() > point(POINT_ST))
                    point_change(POINT_ST, horse_st() - point(POINT_ST));

                if (horse_dx() > point(POINT_DX))
                    point_change(POINT_DX, horse_dx() - point(POINT_DX));

                if (horse_ht() > point(POINT_HT))
                    point_change(POINT_HT, horse_ht() - point(POINT_HT));

                if (horse_iq() > point(POINT_IQ))
                    point_change(POINT_IQ, horse_iq() - point(POINT_IQ));
            }
        }

        compute_battle_points();

        if (iMaxHP != max_hp()) {
            set_real_point(POINT_MAX_HP, iMaxHP);
        }

        point_change(POINT_MAX_HP, 0);

        if (iMaxSP != max_sp()) {
            set_real_point(POINT_MAX_SP, iMaxSP);
        }

        point_change(POINT_MAX_SP, 0);

        set_max_stamina(iMaxStamina);

        points_instant_.immune_flag = 0;

        for (int i = 0; i < WEAR_MAX_NUM; i++) {
            item *pItem = wear(i);
            if (pItem) {
                pItem->modify_points(true);
                set_bit(points_instant_.immune_flag, wear(i)->immune_flag());
            }
        }

        // compute_points reset every point, so the active dragon soul deck must be reapplied.
        if (dragon_soul_is_deck_activated()) {
            for (int i = WEAR_MAX_NUM + DS_SLOT_MAX * dragon_soul_get_active_deck();
                i < WEAR_MAX_NUM + DS_SLOT_MAX * (dragon_soul_get_active_deck() + 1); i++) {
                item *pItem = wear(i);
                if (pItem) {
                    if (ds_manager::instance().is_time_left_dragon_soul(pItem))
                        pItem->modify_points(true);
                }
            }
        }

        if (hp() > max_hp())
            point_change(POINT_HP, max_hp() - hp());

        if (sp() > max_sp())
            point_change(POINT_SP, max_sp() - sp());

        compute_skill_points();

        refresh_affect();
        pet_system *pPetSystem = get_pet_system();
        if (nullptr != pPetSystem) {
            pPetSystem->refresh_buff();
        }

        for (map_buff_on_attrs::iterator it = map_buff_on_attrs_.begin(); it != map_buff_on_attrs_.end(); it++) {
            it->second->give_all_attributes();
        }

        update_packet();
    }

    void character::reset_play_time(uint32_t time_remain) {
        play_start_time_ = get_dword_time() - time_remain;
    }

    const int recovery_percents[10] = {1, 5, 5, 5, 5, 5, 5, 5, 5, 5};

    EVENTFUNC(recovery_event) {
        char_event_info *info = dynamic_cast<char_event_info *>(event->info);
        if (info == nullptr) {
            metin2::log::error("Recovery event fired without event info");
            return 0;
        }

        character *ch = info->ch;

        if (ch == nullptr) {
            return 0;
        }

        if (!ch->is_pc()) {
            if (ch->is_affect_flag(AFF_POISON))
                return passes_per_sec(std::max<int>(1, ch->get_mob_table().regen_cycle));

            if (2493 == ch->get_mob_table().vnum) {
                int regenPct = blue_dragon_get_range_factor("hp_regen", ch->hp_pct());
                regenPct += ch->get_mob_table().regen_percent;

                for (int i = 1; i <= 4; ++i) {
                    if (REGEN_PECT_BONUS == blue_dragon_get_index_factor("DragonStone", i, "effect_type")) {
                        uint32_t dwDragonStoneID = blue_dragon_get_index_factor("DragonStone", i, "vnum");
                        size_t val = blue_dragon_get_index_factor("DragonStone", i, "val");
                        size_t cnt = sectree_manager::instance().monster_count_in_map(ch->map_index(), dwDragonStoneID);

                        regenPct += (val * cnt);

                        break;
                    }
                }

                ch->point_change(POINT_HP, std::max(1, (ch->max_hp() * regenPct) / 100));
            } else if (!ch->is_door()) {
                ch->monster_log("HP_REGEN +%d", std::max(1, (ch->max_hp() * ch->get_mob_table().regen_percent) / 100));
                ch->point_change(POINT_HP, std::max(1, (ch->max_hp() * ch->get_mob_table().regen_percent) / 100));
            }

            if (ch->hp() >= ch->max_hp()) {
                ch->recovery_event = nullptr;
                return 0;
            }

            if (2493 == ch->get_mob_table().vnum) {
                for (int i = 1; i <= 4; ++i) {
                    if (REGEN_TIME_BONUS == blue_dragon_get_index_factor("DragonStone", i, "effect_type")) {
                        uint32_t dwDragonStoneID = blue_dragon_get_index_factor("DragonStone", i, "vnum");
                        size_t val = blue_dragon_get_index_factor("DragonStone", i, "val");
                        size_t cnt = sectree_manager::instance().monster_count_in_map(ch->map_index(), dwDragonStoneID);

                        return passes_per_sec(std::max<int>(1, (ch->get_mob_table().regen_cycle - (val * cnt))));
                    }
                }
            }

            return passes_per_sec(std::max<int>(1, ch->get_mob_table().regen_cycle));
        } else {
            ch->check_target();
            ch->update_killer_mode();

            if (ch->is_affect_flag(AFF_POISON) == true) {
                return 3;
            }

            int iSec = (get_dword_time() - ch->last_move_time()) / 3000;

            ch->distribute_sp(ch);

            if (ch->max_hp() <= ch->hp())
                return passes_per_sec(3);

            int iPercent = 0;
            int iAmount = 0;

            {
                iPercent = recovery_percents[std::min(9, iSec)];
                iAmount = 15 + (ch->max_hp() * iPercent) / 100;
            }

            iAmount += (iAmount * ch->point(POINT_HP_REGEN)) / 100;

            metin2::log::debug("{}: recovery {:d}%, hp regen {:d}, hp +{:d}", ch->name(), iPercent, ch->point(POINT_HP_REGEN), iAmount);

            ch->point_change(POINT_HP, iAmount, false);
            return passes_per_sec(3);
        }
    }

    void character::start_recovery_event() {
        if (recovery_event)
            return;

        if (is_dead() || is_stun())
            return;

        if (is_npc() && hp() >= max_hp())
            return;

        char_event_info *info = alloc_event_info<char_event_info>();

        info->ch = this;

        int iSec = is_pc() ? 3 : (std::max<int>(1, get_mob_table().regen_cycle));
        recovery_event = event_create(game::characters::recovery_event, info, passes_per_sec(iSec));
    }

    void character::standup() {
        struct packet_position pack_position;

        if (!is_position(POS_SITTING))
            return;

        set_position(POS_STANDING);

        metin2::log::debug("{} stands up", name());

        pack_position.header = HEADER_GC_CHARACTER_POSITION;
        pack_position.vid = get_vid();
        pack_position.position = POSITION_GENERAL;

        packet_around(&pack_position, sizeof(pack_position));
    }

    void character::sitdown(int is_ground) {
        struct packet_position pack_position;

        if (is_position(POS_SITTING))
            return;

        set_position(POS_SITTING);
        metin2::log::debug("{} sits down", name());

        pack_position.header = HEADER_GC_CHARACTER_POSITION;
        pack_position.vid = get_vid();
        pack_position.position = POSITION_SITTING_GROUND;
        packet_around(&pack_position, sizeof(pack_position));
    }

    void character::set_rotation(float rot_value) {
        points_instant_.rot = rot_value;
    }

    void character::set_rotation_to_xy(int32_t x_value, int32_t y_value) {
        set_rotation(get_degree_from_position_xy(x(), y(), x_value, y_value));
    }

    auto character::cannot_move_by_affect() const -> bool {
        return (is_affect_flag(AFF_STUN));
    }

    auto character::can_move() const -> bool {
        if (cannot_move_by_affect())
            return false;

        if (my_shop())
            return false;

        return true;
    }

    auto character::sync(int32_t x, int32_t y) -> bool {
        if (!get_sectree())
            return false;

        sectree *new_tree = sectree_manager::instance().get(map_index(), x, y);

        if (!new_tree) {
            if (get_desc()) {
                metin2::log::error("No sectree at {:d} {:d} for {}", x, y, name());
                get_desc()->set_phase(PHASE_CLOSE);
            } else {
                metin2::log::error("{} is outside any sectree at {:d} {:d} (map {:d})", name(), x, y, map_index());
                dead();
            }

            return false;
        }

        set_rotation_to_xy(x, y);
        set_xyz(x, y, 0);

        if (get_dungeon()) {
            int iLastEventAttr = event_attr_;
            event_attr_ = new_tree->event_attribute(x, y);

            if (event_attr_ != iLastEventAttr) {
                if (get_party()) {
                    quest::quest_manager::instance().attr_out(get_party()->leader_pid(), this, iLastEventAttr);
                    quest::quest_manager::instance().attr_in(get_party()->leader_pid(), this, event_attr_);
                } else {
                    quest::quest_manager::instance().attr_out(player_id(), this, iLastEventAttr);
                    quest::quest_manager::instance().attr_in(player_id(), this, event_attr_);
                }
            }
        }

        if (get_sectree() != new_tree) {
            if (!is_npc()) {
                sectreeid id = new_tree->id();
                sectreeid old_id = get_sectree()->id();

                metin2::log::debug("{} moved to sectree {:d}x{:d} from {:d}x{:d}",
                    name(),
                    id.coord.x,
                    id.coord.y,
                    old_id.coord.x,
                    old_id.coord.y);
            }

            new_tree->insert_entity(this);
        }

        return true;
    }

    void character::stop() {
        if (!is_state(state_idle_))
            monster_log("[IDLE] 정지");

        goto_state(state_idle_);

        pos_dest_.x = pos_start_.x = x();
        pos_dest_.y = pos_start_.y = y();
    }

    auto character::go_to(int32_t x_value, int32_t y_value) -> bool {
        if (x() == x_value && y() == y_value)
            return false;

        if (pos_dest_.x == x_value && pos_dest_.y == y_value) {
            if (!is_state(state_move_)) {
                state_duration_ = 4;
                goto_state(state_move_);
            }
            return false;
        }

        pos_dest_.x = x_value;
        pos_dest_.y = y_value;

        calculate_move_duration();

        state_duration_ = 4;

        if (!is_state(state_move_)) {
            monster_log("[MOVE] %s", victim() ? "대상추적" : "그냥이동");

            if (victim()) {
                monster_chat(MONSTER_CHAT_ATTACK);
            }
        }

        goto_state(state_move_);

        return true;
    }

    auto character::get_motion_mode() const -> uint32_t {
        uint32_t dwMode = MOTION_MODE_GENERAL;

        if (is_polymorphed())
            return dwMode;

        item *pkItem;

        if ((pkItem = wear(WEAR_WEAPON))) {
            switch (pkItem->proto()->sub_type) {
            case WEAPON_SWORD:
                dwMode = MOTION_MODE_ONEHAND_SWORD;
                break;

            case WEAPON_TWO_HANDED:
                dwMode = MOTION_MODE_TWOHAND_SWORD;
                break;

            case WEAPON_DAGGER:
                dwMode = MOTION_MODE_DUALHAND_SWORD;
                break;

            case WEAPON_BOW:
                dwMode = MOTION_MODE_BOW;
                break;

            case WEAPON_BELL:
                dwMode = MOTION_MODE_BELL;
                break;

            case WEAPON_FAN:
                dwMode = MOTION_MODE_FAN;
                break;
            }
        }
        return dwMode;
    }

    auto character::move_motion_speed() const -> float {
        uint32_t dwMode = get_motion_mode();

        const motion *pkMotion = nullptr;

        if (!get_mount_vnum())
            pkMotion = motion_manager::instance().get_motion(race_num(), make_motion_key(dwMode, (is_walking() && is_pc()) ? MOTION_WALK : MOTION_RUN));
        else {
            pkMotion = motion_manager::instance().get_motion(get_mount_vnum(), make_motion_key(MOTION_MODE_GENERAL, (is_walking() && is_pc()) ? MOTION_WALK : MOTION_RUN));

            if (!pkMotion)
                pkMotion = motion_manager::instance().get_motion(race_num(), make_motion_key(MOTION_MODE_HORSE, (is_walking() && is_pc()) ? MOTION_WALK : MOTION_RUN));
        }

        if (pkMotion)
            return -pkMotion->accum_vector().y / pkMotion->duration();
        else {
            metin2::log::error("No motion for {} (race {:d}, mode {:d})", name(), race_num(), dwMode);
            return 300.0f;
        }
    }

    auto character::move_speed() const -> float {
        return move_motion_speed() * 10000 / calculate_duration(limit_point(POINT_MOV_SPEED), 10000);
    }

    void character::calculate_move_duration() {
        pos_start_.x = x();
        pos_start_.y = y();

        float fDist = distance_sqrt(pos_start_.x - pos_dest_.x, pos_start_.y - pos_dest_.y);

        float motionSpeed = move_motion_speed();

        move_duration_ = calculate_duration(limit_point(POINT_MOV_SPEED),
            (int)((fDist / motionSpeed) * 1000.0f));

        if (is_npc())
            metin2::log::debug("{}: goto distance {:f}, speed {:d}, duration {:d}, motion speed {:f}, {:d} {:d} -> {:d} {:d}",
                name(), fDist, limit_point(POINT_MOV_SPEED), move_duration_, motionSpeed,
                pos_start_.x, pos_start_.y, pos_dest_.x, pos_dest_.y);

        move_start_time_ = get_dword_time();
    }

    auto character::move(int32_t x_value, int32_t y_value) -> bool {
        if (x() == x_value && y() == y_value)
            return true;

        if (metin2::config::test_server())
            if (detail_log_)
                metin2::log::debug("{} moves to {:d} {:d}", name(), x_value, y_value);

        on_move();
        return sync(x_value, y_value);
    }

    void character::send_move_packet(uint8_t func, uint8_t arg, uint32_t x, uint32_t y, uint32_t duration, uint32_t time, int rot) {
        packet_gc_move pack;

        if (func == FUNC_WAIT) {
            x = pos_dest_.x;
            y = pos_dest_.y;
            duration = move_duration_;
        }

        encode_move_packet(pack, get_vid(), func, arg, x, y, duration, time, rot == -1 ? (int)rotation() / 5 : rot);
        packet_view(&pack, sizeof(packet_gc_move), this);
    }

    auto character::real_point(uint8_t type) const -> int {
        return points_.points[type];
    }

    void character::set_real_point(uint8_t type, int val) {
        points_.points[type] = val;
    }

    auto character::polymorph_point(uint8_t type) const -> int {
        if (is_polymorphed() && !is_poly_maintain_stat()) {
            uint32_t dwMobVnum = polymorph_vnum();
            const mob *pMob = mob_manager::instance().get(dwMobVnum);
            int iPower = polymorph_power();

            if (pMob) {
                switch (type) {
                case POINT_ST:
                    if (job() == JOB_SHAMAN || (job() == JOB_SURA && skill_group() == 2))
                        return pMob->table.str * iPower / 100 + point(POINT_IQ);
                    return pMob->table.str * iPower / 100 + point(POINT_ST);

                case POINT_HT:
                    return pMob->table.con * iPower / 100 + point(POINT_HT);

                case POINT_IQ:
                    return pMob->table.iq * iPower / 100 + point(POINT_IQ);

                case POINT_DX:
                    return pMob->table.dex * iPower / 100 + point(POINT_DX);
                }
            }
        }

        return point(type);
    }

    auto character::point(uint8_t type) const -> int {
        if (type >= POINT_MAX_NUM) {
            metin2::log::error("Point type {:d} out of range", type);
            return 0;
        }

        int val = points_instant_.points[type];
        int max_val = INT_MAX;

        switch (type) {
        case POINT_STEAL_HP:
        case POINT_STEAL_SP:
            max_val = 50;
            break;
        }

        if (val > max_val)
            metin2::log::error("{}: point {:d} value {:d} exceeds max {:d}", name(), type, val, max_val);

        return (val);
    }

    auto character::limit_point(uint8_t type) const -> int {
        if (type >= POINT_MAX_NUM) {
            metin2::log::error("Point type {:d} out of range", type);
            return 0;
        }

        int val = points_instant_.points[type];
        int max_val = INT_MAX;
        int limit = INT_MAX;
        int min_limit = -INT_MAX;

        switch (type) {
        case POINT_ATT_SPEED:
            min_limit = 0;

            if (is_pc())
                limit = 170;
            else
                limit = 250;
            break;

        case POINT_MOV_SPEED:
            min_limit = 0;

            if (is_pc())
                limit = 200;
            else
                limit = 250;
            break;

        case POINT_STEAL_HP:
        case POINT_STEAL_SP:
            limit = 50;
            max_val = 50;
            break;

        case POINT_MALL_ATTBONUS:
        case POINT_MALL_DEFBONUS:
            limit = 20;
            max_val = 50;
            break;
        }

        if (val > max_val)
            metin2::log::error("{}: point {:d} value {:d} exceeds max {:d}", name(), type, val, max_val);

        if (val > limit)
            val = limit;

        if (val < min_limit)
            val = min_limit;

        return (val);
    }

    void character::set_point(uint8_t type, int val) {
        if (type >= POINT_MAX_NUM) {
            metin2::log::error("Point type {:d} out of range", type);
            return;
        }

        points_instant_.points[type] = val;

        if (type == POINT_MOV_SPEED && get_dword_time() < move_start_time_ + move_duration_) {
            calculate_move_duration();
        }
    }

    void character::check_maximum_points() {
        if (max_hp() < hp())
            point_change(POINT_HP, max_hp() - hp());

        if (max_sp() < sp())
            point_change(POINT_SP, max_sp() - sp());
    }

    namespace {
        auto grade_point_of_bonus(uint8_t bonus_type) -> uint8_t {
            switch (bonus_type) {
            case POINT_ATT_GRADE_BONUS:
                return POINT_ATT_GRADE;
            case POINT_DEF_GRADE_BONUS:
                return POINT_DEF_GRADE;
            case POINT_MAGIC_ATT_GRADE_BONUS:
                return POINT_MAGIC_ATT_GRADE;
            default:
                return POINT_MAGIC_DEF_GRADE;
            }
        }
    }

    void character::point_change(uint8_t type, int amount, bool amount_value, bool broadcast) {
        int val = 0;

        switch (type) {
        case POINT_NONE:
            return;

        case POINT_LEVEL:
            if ((level() + amount) > metin2::config::game().max_level)
                return;

            set_level(level() + amount);
            val = level();

            metin2::log::info("{} reached level {:d} (next exp {:d})", name(), level(), next_exp());

            point_change(POINT_NEXT_EXP, next_exp(), false);

            if (amount) {
                quest::quest_manager::instance().level_up(player_id());

                log_manager::instance().level_log(this, val, real_point(POINT_PLAYTIME) + (get_dword_time() - play_start_time_) / 60000);

                if (get_guild()) {
                    get_guild()->level_change(player_id(), level());
                }

                if (get_party()) {
                    get_party()->request_set_member_level(player_id(), level());
                }
            }
            break;

        case POINT_NEXT_EXP:
            val = next_exp();
            amount_value = false;
            break;

        case POINT_EXP:
            {
                uint32_t exp = get_exp();
                uint32_t next_exp_value = next_exp();

                if (amount < 0 && exp < static_cast<uint32_t>(-amount)) {
                    metin2::log::debug("{}: exp loss {:d} exceeds current exp {:d}", name(), -amount, exp);
                    amount = -exp;

                    set_exp(exp + amount);
                    val = get_exp();
                } else {
                    if (metin2::config::game().max_level <= level())
                        return;

                    if (metin2::config::test_server())
                        chat_packet(CHAT_TYPE_INFO, "You have gained %d exp.", amount);

                    uint32_t iExpBalance = 0;

                    if (exp + amount >= next_exp_value) {
                        iExpBalance = (exp + amount) - next_exp_value;
                        amount = next_exp_value - exp;

                        set_exp(0);
                        exp = next_exp_value;
                    } else {
                        set_exp(exp + amount);
                        exp = get_exp();
                    }

                    uint32_t q = uint32_t(next_exp_value / 4.0f);
                    int iLevStep = real_point(POINT_LEVEL_STEP);

                    if (iLevStep >= 4) {
                        metin2::log::error("{}: level step {:d} is above 4, capping", name(), iLevStep);
                        iLevStep = 4;
                    }

                    if (exp >= next_exp_value && iLevStep < 4) {
                        for (int i = 0; i < 4 - iLevStep; ++i)
                            point_change(POINT_LEVEL_STEP, 1, false, true);
                    } else if (exp >= q * 3 && iLevStep < 3) {
                        for (int i = 0; i < 3 - iLevStep; ++i)
                            point_change(POINT_LEVEL_STEP, 1, false, true);
                    } else if (exp >= q * 2 && iLevStep < 2) {
                        for (int i = 0; i < 2 - iLevStep; ++i)
                            point_change(POINT_LEVEL_STEP, 1, false, true);
                    } else if (exp >= q && iLevStep < 1)
                        point_change(POINT_LEVEL_STEP, 1);

                    if (iExpBalance) {
                        point_change(POINT_EXP, iExpBalance);
                    }

                    val = get_exp();
                }
            }
            break;

        case POINT_LEVEL_STEP:
            if (amount > 0) {
                val = point(POINT_LEVEL_STEP) + amount;

                switch (val) {
                case 1:
                case 2:
                case 3:
                    if (level() < 91) point_change(POINT_STAT, 1);
                    break;

                case 4:
                    {
                        int iHP = number(job_initial_points[job()].hp_per_lv_begin, job_initial_points[job()].hp_per_lv_end);
                        int iSP = number(job_initial_points[job()].sp_per_lv_begin, job_initial_points[job()].sp_per_lv_end);

                        points_.random_hp += iHP;
                        points_.random_sp += iSP;

                        if (skill_group()) {
                            if (level() >= 5)
                                point_change(POINT_SKILL, 1);

                            if (level() >= 9)
                                point_change(POINT_SUB_SKILL, 1);
                        }

                        point_change(POINT_MAX_HP, iHP);
                        point_change(POINT_MAX_SP, iSP);
                        point_change(POINT_LEVEL, 1, false, true);

                        val = 0;
                    }
                    break;
                }

                if (level() <= 10)
                    auto_give_item(27001, 2);
                else if (level() <= 30)
                    auto_give_item(27002, 2);
                else {
                    auto_give_item(27002, 2);
                }

                point_change(POINT_HP, max_hp() - hp());
                point_change(POINT_SP, max_sp() - sp());
                point_change(POINT_STAMINA, max_stamina() - stamina());

                set_point(POINT_LEVEL_STEP, val);
                set_real_point(POINT_LEVEL_STEP, val);

                save();
            } else
                val = point(POINT_LEVEL_STEP);

            break;

        case POINT_HP:
            {
                if (is_dead() || is_stun())
                    return;

                int prev_hp = hp();

                amount = std::min(max_hp() - hp(), amount);
                set_hp(hp() + amount);
                val = hp();

                broadcast_target_packet();

                if (get_party() && is_pc() && val != prev_hp)
                    get_party()->send_party_info_one_to_all(this);
            }
            break;

        case POINT_SP:
            {
                if (is_dead() || is_stun())
                    return;

                amount = std::min(max_sp() - sp(), amount);
                set_sp(sp() + amount);
                val = sp();
            }
            break;

        case POINT_STAMINA:
            {
                if (is_dead() || is_stun())
                    return;

                int prev_val = stamina();
                amount = std::min(max_stamina() - stamina(), amount);
                set_stamina(stamina() + amount);
                val = stamina();

                if (val == 0) {
                    set_now_walking(true);
                } else if (prev_val == 0) {
                    reset_walking();
                }

                if (amount < 0 && val != 0)
                    return;
            }
            break;

        case POINT_MAX_HP:
            {
                set_point(type, point(type) + amount);

                int hp_value = real_point(POINT_MAX_HP);
                int add_hp = std::min(3500, hp_value * point(POINT_MAX_HP_PCT) / 100);
                add_hp += point(POINT_MAX_HP);
                add_hp += point(POINT_PARTY_TANKER_BONUS);

                set_max_hp(hp_value + add_hp);

                val = max_hp();
            }
            break;

        case POINT_MAX_SP:
            {
                set_point(type, point(type) + amount);

                int sp_value = real_point(POINT_MAX_SP);
                int add_sp = std::min(800, sp_value * point(POINT_MAX_SP_PCT) / 100);
                add_sp += point(POINT_MAX_SP);
                add_sp += point(POINT_PARTY_SKILL_MASTER_BONUS);

                set_max_sp(sp_value + add_sp);

                val = max_sp();
            }
            break;

        case POINT_MAX_HP_PCT:
            set_point(type, point(type) + amount);
            val = point(type);

            point_change(POINT_MAX_HP, 0);
            break;

        case POINT_MAX_SP_PCT:
            set_point(type, point(type) + amount);
            val = point(type);

            point_change(POINT_MAX_SP, 0);
            break;

        case POINT_MAX_STAMINA:
            set_max_stamina(max_stamina() + amount);
            val = max_stamina();
            break;

        case POINT_GOLD:
            {
                const int64_t nTotalMoney = static_cast<int64_t>(gold()) + static_cast<int64_t>(amount);

                if (GOLD_MAX <= nTotalMoney) {
                    metin2::log::error("Gold overflow for {} (pid {:d}): has {:d}, adding {:d}", name(), player_id(), gold(), amount);
                    log_manager::instance().char_log(this, gold() + amount, "OVERFLOW_GOLD", "");
                    return;
                }

                set_gold(gold() + amount);
                val = gold();
            }
            break;

        case POINT_SKILL:
        case POINT_STAT:
        case POINT_SUB_SKILL:
        case POINT_STAT_RESET_COUNT:
        case POINT_HORSE_SKILL:
            set_point(type, point(type) + amount);
            val = point(type);

            set_real_point(type, val);
            break;

        case POINT_DEF_GRADE:
            set_point(type, point(type) + amount);
            val = point(type);

            point_change(POINT_CLIENT_DEF_GRADE, amount);
            break;

        case POINT_CLIENT_DEF_GRADE:
        case POINT_ST:
        case POINT_HT:
        case POINT_DX:
        case POINT_IQ:
        case POINT_HP_REGEN:
        case POINT_SP_REGEN:
        case POINT_ATT_SPEED:
        case POINT_ATT_GRADE:
        case POINT_MOV_SPEED:
        case POINT_CASTING_SPEED:
        case POINT_MAGIC_ATT_GRADE:
        case POINT_MAGIC_DEF_GRADE:
        case POINT_BOW_DISTANCE:
        case POINT_HP_RECOVERY:
        case POINT_SP_RECOVERY:

        case POINT_ATTBONUS_HUMAN:
        case POINT_ATTBONUS_ANIMAL:
        case POINT_ATTBONUS_ORC:
        case POINT_ATTBONUS_MILGYO:
        case POINT_ATTBONUS_UNDEAD:
        case POINT_ATTBONUS_DEVIL:

        case POINT_ATTBONUS_MONSTER:
        case POINT_ATTBONUS_SURA:
        case POINT_ATTBONUS_ASSASSIN:
        case POINT_ATTBONUS_WARRIOR:
        case POINT_ATTBONUS_SHAMAN:

        case POINT_POISON_PCT:
        case POINT_STUN_PCT:
        case POINT_SLOW_PCT:

        case POINT_BLOCK:
        case POINT_DODGE:

        case POINT_CRITICAL_PCT:
        case POINT_RESIST_CRITICAL:
        case POINT_PENETRATE_PCT:
        case POINT_RESIST_PENETRATE:
        case POINT_CURSE_PCT:

        case POINT_STEAL_HP:
        case POINT_STEAL_SP:

        case POINT_MANA_BURN_PCT:
        case POINT_DAMAGE_SP_RECOVER:
        case POINT_RESIST_NORMAL_DAMAGE:
        case POINT_RESIST_SWORD:
        case POINT_RESIST_TWOHAND:
        case POINT_RESIST_DAGGER:
        case POINT_RESIST_BELL:
        case POINT_RESIST_FAN:
        case POINT_RESIST_BOW:
        case POINT_RESIST_FIRE:
        case POINT_RESIST_ELEC:
        case POINT_RESIST_MAGIC:
        case POINT_RESIST_WIND:
        case POINT_RESIST_ICE:
        case POINT_RESIST_EARTH:
        case POINT_RESIST_DARK:
        case POINT_REFLECT_MELEE:
        case POINT_REFLECT_CURSE:
        case POINT_POISON_REDUCE:
        case POINT_KILL_SP_RECOVER:
        case POINT_KILL_HP_RECOVERY:
        case POINT_HIT_HP_RECOVERY:
        case POINT_HIT_SP_RECOVERY:
        case POINT_MANASHIELD:
        case POINT_ATT_BONUS:
        case POINT_DEF_BONUS:
        case POINT_SKILL_DAMAGE_BONUS:
        case POINT_NORMAL_HIT_DAMAGE_BONUS:

        case POINT_SKILL_DEFEND_BONUS:
        case POINT_NORMAL_HIT_DEFEND_BONUS:

        case POINT_PARTY_ATTACKER_BONUS:
        case POINT_PARTY_TANKER_BONUS:
        case POINT_PARTY_BUFFER_BONUS:
        case POINT_PARTY_SKILL_MASTER_BONUS:
        case POINT_PARTY_HASTE_BONUS:
        case POINT_PARTY_DEFENDER_BONUS:

        case POINT_RESIST_WARRIOR:
        case POINT_RESIST_ASSASSIN:
        case POINT_RESIST_SURA:
        case POINT_RESIST_SHAMAN:
            set_point(type, point(type) + amount);
            val = point(type);
            break;

        case POINT_MALL_ATTBONUS:
        case POINT_MALL_DEFBONUS:
        case POINT_MALL_EXPBONUS:
        case POINT_MALL_ITEMBONUS:
        case POINT_MALL_GOLDBONUS:
        case POINT_MELEE_MAGIC_ATT_BONUS_PER:
        case POINT_EXP_DOUBLE_BONUS:
        case POINT_GOLD_DOUBLE_BONUS:
        case POINT_ITEM_DROP_BONUS:
        case POINT_POTION_BONUS:
            if (point(type) + amount > 100) {
                const bool mall = type != POINT_EXP_DOUBLE_BONUS && type != POINT_GOLD_DOUBLE_BONUS && type != POINT_ITEM_DROP_BONUS && type != POINT_POTION_BONUS;
                metin2::log::error("{} point {:d} of {} would exceed 100 (adding {:d})", mall ? "Mall bonus" : "Bonus", type, name(), amount);
                amount = 100 - point(type);
            }

            set_point(type, point(type) + amount);
            val = point(type);
            break;

        case POINT_PC_BANG_EXP_BONUS:
        case POINT_PC_BANG_DROP_BONUS:
        case POINT_RAMADAN_CANDY_BONUS_EXP:
            set_point(type, amount);
            val = point(type);
            break;

        case POINT_IMMUNE_STUN:
        case POINT_IMMUNE_SLOW:
        case POINT_IMMUNE_FALL:
            {
                const auto bit = type == POINT_IMMUNE_STUN ? IMMUNE_STUN : type == POINT_IMMUNE_SLOW ? IMMUNE_SLOW
                                                                                                     : IMMUNE_FALL;
                set_point(type, point(type) + amount);
                val = point(type);
                if (val) {
                    set_bit(points_instant_.immune_flag, bit);
                } else {
                    remove_bit(points_instant_.immune_flag, bit);
                }
            }
            break;

        case POINT_ATT_GRADE_BONUS:
        case POINT_DEF_GRADE_BONUS:
        case POINT_MAGIC_ATT_GRADE_BONUS:
        case POINT_MAGIC_DEF_GRADE_BONUS:
            set_point(type, point(type) + amount);
            point_change(grade_point_of_bonus(type), amount);
            val = point(type);
            break;

        case POINT_VOICE:
        case POINT_EMPIRE_POINT:
            val = real_point(type);
            break;

        case POINT_POLYMORPH:
            set_point(type, point(type) + amount);
            val = point(type);
            set_polymorph(val);
            break;

        case POINT_MOUNT:
            set_point(type, point(type) + amount);
            val = point(type);
            mount_vnum(val);
            break;

        case POINT_ENERGY:
        case POINT_COSTUME_ATTR_BONUS:
            {
                int old_val = point(type);
                set_point(type, old_val + amount);
                val = point(type);
                buff_on_attr_value_change(type, old_val, val);
            }
            break;

        default:
            metin2::log::error("{}: unknown point change type {:d}", name(), type);
            return;
        }

        switch (type) {
        case POINT_LEVEL:
        case POINT_ST:
        case POINT_DX:
        case POINT_IQ:
        case POINT_HT:
            compute_battle_points();
            break;
        case POINT_MAX_HP:
        case POINT_MAX_SP:
        case POINT_MAX_STAMINA:
            break;
        }

        if (type == POINT_HP && amount == 0)
            return;

        if (get_desc()) {
            struct packet_gc_point_change pack;

            pack.header = HEADER_GC_CHARACTER_POINT_CHANGE;
            pack.vid = vid_;
            pack.type = type;
            pack.value = val;

            if (amount_value)
                pack.amount = amount;
            else
                pack.amount = 0;

            if (!broadcast)
                get_desc()->packet(&pack, sizeof(struct packet_gc_point_change));
            else
                packet_around(&pack, sizeof(pack));
        }
    }

    void character::apply_point(uint8_t apply_type, int val) {
        switch (apply_type) {
        case APPLY_NONE:
            break;

        case APPLY_CON:
            point_change(POINT_HT, val);
            point_change(POINT_MAX_HP, (val * job_initial_points[job()].hp_per_ht));
            point_change(POINT_MAX_STAMINA, (val * job_initial_points[job()].stamina_per_con));
            break;

        case APPLY_INT:
            point_change(POINT_IQ, val);
            point_change(POINT_MAX_SP, (val * job_initial_points[job()].sp_per_iq));
            break;

        case APPLY_SKILL:
            {
                uint8_t bSkillVnum = (uint8_t)(((uint32_t)val) >> 24);
                int iAdd = val & 0x00800000;
                int iChange = val & 0x007fffff;

                metin2::log::debug("Apply skill {:d}: add {:d}, change {:d}", bSkillVnum, iAdd ? 1 : 0, iChange);

                if (0 == iAdd)
                    iChange = -iChange;

                std::unordered_map<uint8_t, int>::iterator iter = skill_damage_bonus_.find(bSkillVnum);

                if (iter == skill_damage_bonus_.end())
                    skill_damage_bonus_.insert(std::make_pair(bSkillVnum, iChange));
                else
                    iter->second += iChange;
            }
            break;

        default:
            if (apply_type >= MAX_APPLY_NUM || apply_type == APPLY_EXTRACT_HP_PCT) {
                metin2::log::error("Unknown apply type {:d} for {}", apply_type, name());
                break;
            }
            point_change(a_apply_info[apply_type].point_type, val);
            break;
        }
    }

    EVENTFUNC(save_event) {
        char_event_info *info = dynamic_cast<char_event_info *>(event->info);
        if (info == nullptr) {
            metin2::log::error("Save event fired without event info");
            return 0;
        }

        character *ch = info->ch;

        if (ch == nullptr) {
            return 0;
        }
        metin2::log::debug("Save event for {}", ch->name());
        ch->save();
        ch->flush_delayed_save_item();
        return (passes_per_sec(metin2::config::game().save_seconds));
    }

    void character::start_save_event() {
        if (save_event)
            return;

        char_event_info *info = alloc_event_info<char_event_info>();

        info->ch = this;
        save_event = event_create(game::characters::save_event, info, passes_per_sec(metin2::config::game().save_seconds));
    }

    void character::monster_log(const char *format, ...) {
        if (!metin2::config::test_server())
            return;

        if (is_pc())
            return;

        char chatbuf[CHAT_MAX_LEN + 1];
        int len = snprintf(chatbuf, sizeof(chatbuf), "%u)", (uint32_t)get_vid());

        if (len < 0 || len >= (int)sizeof(chatbuf))
            len = sizeof(chatbuf) - 1;

        va_list args;

        va_start(args, format);

        int len2 = vsnprintf(chatbuf + len, sizeof(chatbuf) - len, format, args);

        if (len2 < 0 || len2 >= (int)sizeof(chatbuf) - len)
            len += (sizeof(chatbuf) - len) - 1;
        else
            len += len2;

        ++len;

        va_end(args);

        packet_gc_chat pack_chat;

        pack_chat.header = HEADER_GC_CHAT;
        pack_chat.size = sizeof(packet_gc_chat) + len;
        pack_chat.type = CHAT_TYPE_TALKING;
        pack_chat.id = (uint32_t)get_vid();
        pack_chat.empire = 0;

        temp_buffer buf;
        buf.write(&pack_chat, sizeof(packet_gc_chat));
        buf.write(chatbuf, len);

        character_manager::instance().packet_monster_log(this, buf.read_peek(), buf.size());
    }

    void character::chat_packet(uint8_t type, const char *format, ...) {
        desc *d = get_desc();

        if (!d || !format)
            return;

        char chatbuf[CHAT_MAX_LEN + 1];
        va_list args;

        va_start(args, format);
        int len = vsnprintf(chatbuf, sizeof(chatbuf), format, args);
        va_end(args);

        if (len < 0)
            return;

        if (len >= (int)sizeof(chatbuf)) {
            len = (int)utf8_truncated_size({chatbuf, sizeof(chatbuf) - 1}, sizeof(chatbuf) - 1);
            chatbuf[len] = '\0';
        }

        struct packet_gc_chat pack_chat;

        pack_chat.header = HEADER_GC_CHAT;
        pack_chat.size = sizeof(struct packet_gc_chat) + len;
        pack_chat.type = type;
        pack_chat.id = 0;
        pack_chat.empire = d->empire();

        temp_buffer buf;
        buf.write(&pack_chat, sizeof(struct packet_gc_chat));
        buf.write(chatbuf, len);

        d->packet(buf.read_peek(), buf.size());

        if (type == CHAT_TYPE_COMMAND && metin2::config::test_server())
            metin2::log::debug("Command to {}: {}", name(), chatbuf);
    }

    void character::mining_take() {
        mining_event = nullptr;
    }

    void character::mining_cancel() {
        if (mining_event) {
            metin2::log::debug("{}: mining cancelled", name());
            event_cancel(&mining_event);
            chat_packet(CHAT_TYPE_INFO, lc_text("채광을 중단하였습니다."));
        }
    }

    void character::mining(character *ch_load) {
        if (mining_event) {
            mining_cancel();
            return;
        }

        if (!ch_load)
            return;

        if (mining::get_raw_ore_from_load(ch_load->race_num()) == 0)
            return;

        item *pick = wear(WEAR_WEAPON);

        if (!pick || pick->type() != ITEM_PICK) {
            chat_packet(CHAT_TYPE_INFO, lc_text("곡괭이를 장착하세요."));
            return;
        }

        int count = number(5, 15);

        packet_gc_dig_motion p;
        p.header = HEADER_GC_DIG_MOTION;
        p.vid = get_vid();
        p.target_vid = ch_load->get_vid();
        p.count = count;

        packet_around(&p, sizeof(p));

        mining_event = mining::create_mining_event(this, ch_load, count);
    }

    void character::fishing() {
        if (fishing_event) {
            fishing_take();
            return;
        }

        {
            sectree_map *pkSectreeMap = sectree_manager::instance().map(map_index());

            int x_value = x();
            int y_value = y();

            sectree *tree = pkSectreeMap->find(x_value, y_value);
            uint32_t dwAttr = tree->get_attribute(x_value, y_value);

            if (is_set(dwAttr, ATTR_BLOCK)) {
                chat_packet(CHAT_TYPE_INFO, lc_text("낚시를 할 수 있는 곳이 아닙니다"));
                return;
            }
        }

        item *rod = wear(WEAR_WEAPON);

        if (!rod || rod->type() != ITEM_ROD) {
            chat_packet(CHAT_TYPE_INFO, lc_text("낚시대를 장착 하세요."));
            return;
        }

        if (0 == rod->get_socket(2)) {
            chat_packet(CHAT_TYPE_INFO, lc_text("미끼를 끼고 던져 주세요."));
            return;
        }

        float fx, fy;
        get_delta_by_degree(rotation(), 400.0f, &fx, &fy);

        fishing_event = fishing::create_fishing_event(this);
    }

    void character::fishing_take() {
        item *rod = wear(WEAR_WEAPON);
        if (rod && rod->type() == ITEM_ROD) {
            using fishing::fishing_event_info;
            if (fishing_event) {
                struct fishing_event_info *info = dynamic_cast<struct fishing_event_info *>(fishing_event->info);

                if (info)
                    fishing::take(info, this);
            }
        } else {
            chat_packet(CHAT_TYPE_INFO, lc_text("낚시대가 아닌 물건으로 낚시를 할 수 없습니다!"));
        }

        event_cancel(&fishing_event);
    }

    auto character::start_state_machine(int next_pulse) -> bool {
        if (character_manager::instance().add_to_state_list(this)) {
            next_state_pulse_ = thecore_pulse() + next_pulse;
            return true;
        }

        return false;
    }

    void character::stop_state_machine() {
        character_manager::instance().remove_from_state_list(this);
    }

    void character::update_state_machine(uint32_t pulse) {
        if (pulse < next_state_pulse_)
            return;

        if (is_dead())
            return;

        update();
        next_state_pulse_ = pulse + state_duration_;
    }

    void character::set_next_state_pulse(int next_pulse) {
        character_manager::instance().add_to_state_list(this);
        next_state_pulse_ = next_pulse;

        if (next_pulse < 10)
            monster_log("다음상태로어서가자");
    }

    void character::update_character(uint32_t pulse) {
        fsm::update();
    }

    void character::set_shop(shop *shop_value) {
        if ((shop_ = shop_value))
            set_bit(points_instant_.instant_flag, INSTANT_FLAG_SHOP);
        else {
            remove_bit(points_instant_.instant_flag, INSTANT_FLAG_SHOP);
            set_shop_owner(nullptr);
        }
    }

    void character::set_exchange(exchange *exchange_value) {
        exchange_ = exchange_value;
    }

    void character::set_part(uint8_t part_pos, uint16_t val) {
        assert(part_pos < PART_MAX_NUM);
        points_instant_.parts[part_pos] = val;
    }

    auto character::part(uint8_t part_pos) const -> uint16_t {
        assert(part_pos < PART_MAX_NUM);
        return points_instant_.parts[part_pos];
    }

    auto character::original_part(uint8_t part_pos) const -> uint16_t {
        switch (part_pos) {
        case PART_MAIN:
            if (!is_pc())
                return part(PART_MAIN);
            else
                return points_instant_.base_part;

        case PART_HAIR:
            return part(PART_HAIR);

        default:
            return 0;
        }
    }

    auto character::get_char_type() const -> uint8_t {
        return char_type_;
    }

    auto character::set_sync_owner(character *ch, bool remove_from_list) -> bool {
        if (is_set(points_instant_.ai_flag, AIFLAG_NOMOVE))
            return false;

        if (ch == this) {
            metin2::log::error("Character cannot be its own sync owner ({})", static_cast<const void *>(this));
            return false;
        }

        if (!ch) {
            if (remove_from_list && chr_sync_owner_) {
                chr_sync_owner_->lst_chr_sync_owned_.remove(this);
            }

            if (chr_sync_owner_)
                metin2::log::debug("{} ({}): sync released from {}", name(), static_cast<const void *>(this), chr_sync_owner_->name());

            chr_sync_owner_ = nullptr;
        } else {
            if (!is_sync_owner(ch))
                return false;

            if (distance_approx(x() - ch->x(), y() - ch->y()) > 250) {
                metin2::log::debug("{}: sync owner {} is over 250 away", name(), ch->name());

                if (chr_sync_owner_ == ch)
                    return true;

                return false;
            }

            if (chr_sync_owner_ != ch) {
                if (chr_sync_owner_) {
                    metin2::log::debug("{} ({}): sync released from {}", name(), static_cast<const void *>(this), chr_sync_owner_->name());
                    chr_sync_owner_->lst_chr_sync_owned_.remove(this);
                }

                chr_sync_owner_ = ch;
                chr_sync_owner_->lst_chr_sync_owned_.push_back(this);

                static const timeval zero_tv = {0, 0};
                set_last_sync_time(zero_tv);

                metin2::log::debug("{} ({}): sync owner set to {}", name(), static_cast<const void *>(this), ch->name());
            }

            sync_time_ = get_float_time();
        }

        packet_gc_ownership pack;

        pack.header = HEADER_GC_OWNERSHIP;
        pack.owner_vid = ch ? ch->get_vid() : 0;
        pack.victim_vid = get_vid();

        packet_around(&pack, sizeof(packet_gc_ownership));
        return true;
    }

    void character::clear_sync() {
        set_sync_owner(nullptr);

        for (character *ch : lst_chr_sync_owned_) {
            assert(ch != nullptr);
            ch->set_sync_owner(nullptr, false); // Must be false for the for_each to work.
        }
        lst_chr_sync_owned_.clear();
    }

    auto character::is_sync_owner(character *ch) const -> bool {
        if (chr_sync_owner_ == ch)
            return true;

        if (get_float_time() - sync_time_ >= 3.0f)
            return true;

        return false;
    }

    void character::set_party(party *party_value) {
        if (party_value == party_)
            return;

        if (party_value && party_)
            metin2::log::error("{} is already in party {}, cannot assign party {}", name(), static_cast<const void *>(party_), static_cast<const void *>(party_value));

        metin2::log::debug("{}: party set to {}", name(), static_cast<const void *>(party_value));

        if (dungeon_ && is_pc() && !party_value)
            set_dungeon(nullptr);

        party_ = party_value;

        if (is_pc()) {
            if (party_)
                set_bit(add_chr_state_, ADD_CHARACTER_STATE_PARTY);
            else
                remove_bit(add_chr_state_, ADD_CHARACTER_STATE_PARTY);

            update_packet();
        }
    }

    EVENTINFO(party_join_event_info) {
        uint32_t guest_pid;
        uint32_t leader_pid;

        party_join_event_info()
            : guest_pid(0), leader_pid(0) {
        }
    };

    EVENTFUNC(party_request_event) {
        party_join_event_info *info = dynamic_cast<party_join_event_info *>(event->info);

        if (info == nullptr) {
            metin2::log::error("Party request event fired without event info");
            return 0;
        }

        character *ch = character_manager::instance().find_by_pid(info->guest_pid);

        if (ch) {
            metin2::log::debug("Party request by {} expired", ch->name());
            ch->chat_packet(CHAT_TYPE_COMMAND, "PartyRequestDenied");
            ch->set_party_request_event(nullptr);
        }

        return 0;
    }

    auto character::request_to_party(character *leader) -> bool {
        if (leader->get_party())
            leader = leader->get_party()->leader_character();

        if (!leader) {
            chat_packet(CHAT_TYPE_INFO, lc_text("파티장이 접속 상태가 아니라서 요청을 할 수 없습니다."));
            return false;
        }

        if (party_request_event_)
            return false;

        if (!is_pc() || !leader->is_pc())
            return false;

        if (leader->is_block_mode(BLOCK_PARTY_REQUEST))
            return false;

        party_join_err_code errcode = is_party_joinable_condition(leader, this);

        switch (errcode) {
        case PERR_NONE:
            break;

        case PERR_SERVER:
            chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 서버 문제로 파티 관련 처리를 할 수 없습니다."));
            return false;

        case PERR_DIFFEMPIRE:
            chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 다른 제국과 파티를 이룰 수 없습니다."));
            return false;

        case PERR_DUNGEON:
            chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 던전 안에서는 파티 초대를 할 수 없습니다."));
            return false;

        case PERR_OBSERVER:
            chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 관전 모드에선 파티 초대를 할 수 없습니다."));
            return false;

        case PERR_LVBOUNDARY:
            chat_packet(CHAT_TYPE_INFO, lc_text("<파티> -30 ~ +30 레벨 이내의 상대방만 초대할 수 있습니다."));
            return false;

        case PERR_LOWLEVEL:
            chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 파티내 최고 레벨 보다 30레벨이 낮아 초대할 수 없습니다."));
            return false;

        case PERR_HILEVEL:
            chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 파티내 최저 레벨 보다 30레벨이 높아 초대할 수 없습니다."));
            return false;

        case PERR_ALREADYJOIN:
            return false;

        case PERR_PARTYISFULL:
            chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 더 이상 파티원을 초대할 수 없습니다."));
            return false;

        default:
            metin2::log::error("Unhandled party join error {:d}", errcode);
            return false;
        }

        party_join_event_info *info = alloc_event_info<party_join_event_info>();

        info->guest_pid = player_id();
        info->leader_pid = leader->player_id();

        set_party_request_event(event_create(party_request_event, info, passes_per_sec(10)));

        leader->chat_packet(CHAT_TYPE_COMMAND, "PartyRequest %u", (uint32_t)get_vid());
        chat_packet(CHAT_TYPE_INFO, lc_text("%s 님에게 파티가입 신청을 했습니다."), leader->name());
        return true;
    }

    void character::deny_to_party(character *member) {
        metin2::log::debug("{} denies party request of {} ({})", name(), member->name(), static_cast<const void *>(member->party_request_event_.get()));

        if (!member->party_request_event_)
            return;

        party_join_event_info *info = dynamic_cast<party_join_event_info *>(member->party_request_event_->info);

        if (!info) {
            metin2::log::error("Party request event of member has no info on deny");
            return;
        }

        if (info->guest_pid != member->player_id())
            return;

        if (info->leader_pid != player_id())
            return;

        event_cancel(&member->party_request_event_);

        member->chat_packet(CHAT_TYPE_COMMAND, "PartyRequestDenied");
    }

    void character::accept_to_party(character *member) {
        metin2::log::debug("{} accepts party request of {} ({})", name(), member->name(), static_cast<const void *>(member->party_request_event_.get()));

        if (!member->party_request_event_)
            return;

        party_join_event_info *info = dynamic_cast<party_join_event_info *>(member->party_request_event_->info);

        if (!info) {
            metin2::log::error("Party request event of member has no info on accept");
            return;
        }

        if (info->guest_pid != member->player_id())
            return;

        if (info->leader_pid != player_id())
            return;

        event_cancel(&member->party_request_event_);

        if (!get_party())
            member->chat_packet(CHAT_TYPE_INFO, lc_text("상대방이 파티에 속해있지 않습니다."));
        else {
            if (player_id() != get_party()->leader_pid())
                return;

            party_join_err_code errcode = is_party_joinable_condition(this, member);
            switch (errcode) {
            case PERR_NONE:        member->party_join(this); return;
            case PERR_SERVER:      member->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 서버 문제로 파티 관련 처리를 할 수 없습니다.")); break;
            case PERR_DUNGEON:     member->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 던전 안에서는 파티 초대를 할 수 없습니다.")); break;
            case PERR_OBSERVER:    member->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 관전 모드에선 파티 초대를 할 수 없습니다.")); break;
            case PERR_LVBOUNDARY:  member->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> -30 ~ +30 레벨 이내의 상대방만 초대할 수 있습니다.")); break;
            case PERR_LOWLEVEL:    member->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 파티내 최고 레벨 보다 30레벨이 낮아 초대할 수 없습니다.")); break;
            case PERR_HILEVEL:     member->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 파티내 최저 레벨 보다 30레벨이 높아 초대할 수 없습니다.")); break;
            case PERR_ALREADYJOIN: break;
            case PERR_PARTYISFULL:
                {
                    chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 더 이상 파티원을 초대할 수 없습니다."));
                    member->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 파티의 인원제한이 초과하여 파티에 참가할 수 없습니다."));
                    break;
                }
            default: metin2::log::error("Unhandled party join error {:d}", errcode);
            }
        }

        member->chat_packet(CHAT_TYPE_COMMAND, "PartyRequestDenied");
    }

    EVENTFUNC(party_invite_event) {
        party_join_event_info *pInfo = dynamic_cast<party_join_event_info *>(event->info);

        if (pInfo == nullptr) {
            metin2::log::error("Party invite event fired without event info");
            return 0;
        }

        character *pchInviter = character_manager::instance().find_by_pid(pInfo->leader_pid);

        if (pchInviter) {
            metin2::log::debug("Party invite from {} expired", pchInviter->name());
            pchInviter->party_invite_deny(pInfo->guest_pid);
        }

        return 0;
    }

    void character::party_invite(character *pch_invitee) {
        if (get_party() && get_party()->leader_pid() != player_id()) {
            chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 파티원을 초대할 수 있는 권한이 없습니다."));
            return;
        } else if (pch_invitee->is_block_mode(BLOCK_PARTY_INVITE)) {
            chat_packet(CHAT_TYPE_INFO, lc_text("<파티> %s 님이 파티 거부 상태입니다."), pch_invitee->name());
            return;
        }

        party_join_err_code errcode = is_party_joinable_condition(this, pch_invitee);

        switch (errcode) {
        case PERR_NONE:
            break;

        case PERR_SERVER:
            chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 서버 문제로 파티 관련 처리를 할 수 없습니다."));
            return;

        case PERR_DIFFEMPIRE:
            chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 다른 제국과 파티를 이룰 수 없습니다."));
            return;

        case PERR_DUNGEON:
            chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 던전 안에서는 파티 초대를 할 수 없습니다."));
            return;

        case PERR_OBSERVER:
            chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 관전 모드에선 파티 초대를 할 수 없습니다."));
            return;

        case PERR_LVBOUNDARY:
            chat_packet(CHAT_TYPE_INFO, lc_text("<파티> -30 ~ +30 레벨 이내의 상대방만 초대할 수 있습니다."));
            return;

        case PERR_LOWLEVEL:
            chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 파티내 최고 레벨 보다 30레벨이 낮아 초대할 수 없습니다."));
            return;

        case PERR_HILEVEL:
            chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 파티내 최저 레벨 보다 30레벨이 높아 초대할 수 없습니다."));
            return;

        case PERR_ALREADYJOIN:
            chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 이미 %s님은 파티에 속해 있습니다."), pch_invitee->name());
            return;

        case PERR_PARTYISFULL:
            chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 더 이상 파티원을 초대할 수 없습니다."));
            return;

        default:
            metin2::log::error("Unhandled party join error {:d}", errcode);
            return;
        }

        if (party_invite_event_map_.end() != party_invite_event_map_.find(pch_invitee->player_id()))
            return;

        party_join_event_info *info = alloc_event_info<party_join_event_info>();

        info->guest_pid = pch_invitee->player_id();
        info->leader_pid = player_id();

        party_invite_event_map_.insert(event_map::value_type(pch_invitee->player_id(), event_create(party_invite_event, info, passes_per_sec(10))));

        packet_gc_party_invite p;
        p.header = HEADER_GC_PARTY_INVITE;
        p.leader_vid = get_vid();
        pch_invitee->get_desc()->packet(&p, sizeof(p));
    }

    void character::party_invite_accept(character *pch_invitee) {
        event_map::iterator itFind = party_invite_event_map_.find(pch_invitee->player_id());

        if (itFind == party_invite_event_map_.end()) {
            metin2::log::debug("Party invite accepted by {} without an invite", pch_invitee->name());
            return;
        }

        event_cancel(&itFind->second);
        party_invite_event_map_.erase(itFind);

        if (get_party() && get_party()->leader_pid() != player_id()) {
            chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 파티원을 초대할 수 있는 권한이 없습니다."));
            return;
        }

        party_join_err_code errcode = is_party_joinable_mutable_condition(this, pch_invitee);

        switch (errcode) {
        case PERR_NONE:
            break;

        case PERR_SERVER:
            pch_invitee->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 서버 문제로 파티 관련 처리를 할 수 없습니다."));
            return;

        case PERR_DUNGEON:
            pch_invitee->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 던전 안에서는 파티 초대에 응할 수 없습니다."));
            return;

        case PERR_OBSERVER:
            pch_invitee->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 관전 모드에선 파티 초대를 할 수 없습니다."));
            return;

        case PERR_LVBOUNDARY:
            pch_invitee->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> -30 ~ +30 레벨 이내의 상대방만 초대할 수 있습니다."));
            return;

        case PERR_LOWLEVEL:
            pch_invitee->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 파티내 최고 레벨 보다 30레벨이 낮아 초대할 수 없습니다."));
            return;

        case PERR_HILEVEL:
            pch_invitee->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 파티내 최저 레벨 보다 30레벨이 높아 초대할 수 없습니다."));
            return;

        case PERR_ALREADYJOIN:
            pch_invitee->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 파티 초대에 응할 수 없습니다."));
            return;

        case PERR_PARTYISFULL:
            chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 더 이상 파티원을 초대할 수 없습니다."));
            pch_invitee->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 파티의 인원제한이 초과하여 파티에 참가할 수 없습니다."));
            return;

        default:
            metin2::log::error("Unhandled party join error {:d}", errcode);
            return;
        }

        if (get_party())
            pch_invitee->party_join(this);
        else {
            party *pParty = party_manager::instance().create_party(this);

            pParty->join(pch_invitee->player_id());
            pParty->link(pch_invitee);
            pParty->send_party_info_all_to_one(this);
        }
    }

    void character::party_invite_deny(uint32_t pid) {
        event_map::iterator itFind = party_invite_event_map_.find(pid);

        if (itFind == party_invite_event_map_.end()) {
            metin2::log::debug("Party invite deny without a pending invite (inviter pid {:d}, invitee pid {:d})", player_id(), pid);
            return;
        }

        event_cancel(&itFind->second);
        party_invite_event_map_.erase(itFind);

        character *pchInvitee = character_manager::instance().find_by_pid(pid);
        if (pchInvitee)
            chat_packet(CHAT_TYPE_INFO, lc_text("<파티> %s님이 파티 초대를 거절하셨습니다."), pchInvitee->name());
    }

    void character::party_join(character *leader) {
        leader->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> %s님이 파티에 참가하셨습니다."), name());
        chat_packet(CHAT_TYPE_INFO, lc_text("<파티> %s님의 파티에 참가하셨습니다."), leader->name());

        leader->get_party()->join(player_id());
        leader->get_party()->link(this);
    }

    auto character::is_party_joinable_condition(character *const pch_leader, character *const pch_guest) -> character::party_join_err_code {
        if (pch_leader->empire() != pch_guest->empire())
            return PERR_DIFFEMPIRE;

        return is_party_joinable_mutable_condition(pch_leader, pch_guest);
    }

    static auto party_can_join_by_level(character *leader, character *quest) -> bool {
        int level_limit = 30;

        level_limit = 30;

        return (abs(leader->level() - quest->level()) <= level_limit);
    }

    auto character::is_party_joinable_mutable_condition(character *const pch_leader, character *const pch_guest) -> character::party_join_err_code {
        if (!party_manager::instance().is_enable_pc_party())
            return PERR_SERVER;
        else if (pch_leader->get_dungeon())
            return PERR_DUNGEON;
        else if (pch_guest->is_observer_mode())
            return PERR_OBSERVER;
        else if (false == party_can_join_by_level(pch_leader, pch_guest))
            return PERR_LVBOUNDARY;
        else if (pch_guest->get_party())
            return PERR_ALREADYJOIN;
        else if (pch_leader->get_party()) {
            if (pch_leader->get_party()->member_count() == PARTY_MAX_MEMBER)
                return PERR_PARTYISFULL;
        }

        return PERR_NONE;
    }

    void character::set_dungeon(dungeon *dungeon_value) {
        if (dungeon_value && dungeon_)
            metin2::log::error("{} is already in dungeon {}, cannot assign dungeon {}", name(), static_cast<const void *>(dungeon_), static_cast<const void *>(dungeon_value));

        if (dungeon_ == dungeon_value) {
            return;
        }

        if (dungeon_) {
            if (is_pc()) {
                if (get_party())
                    dungeon_->dec_party_member(get_party(), this);
                else
                    dungeon_->dec_member(this);
            } else if (is_monster() || is_stone()) {
                dungeon_->dec_monster();
            }
        }

        dungeon_ = dungeon_value;

        if (dungeon_value) {
            metin2::log::debug("{}: dungeon set to {}, party {}", name(), static_cast<const void *>(dungeon_value), static_cast<const void *>(party_));

            if (is_pc()) {
                if (get_party())
                    dungeon_->inc_party_member(get_party(), this);
                else
                    dungeon_->inc_member(this);
            } else if (is_monster() || is_stone()) {
                dungeon_->inc_monster();
            }
        }
    }

    void character::set_war_map(war_map *war_map_value) {
        if (war_map_)
            war_map_->dec_member(this);

        war_map_ = war_map_value;

        if (war_map_)
            war_map_->inc_member(this);
    }

    void character::set_wedding_map(marriage::wedding_map *map) {
        if (wedding_map_)
            wedding_map_->dec_member(this);

        wedding_map_ = map;

        if (wedding_map_)
            wedding_map_->inc_member(this);
    }

    void character::set_regen(regen *regen_value) {
        regen_ = regen_value;
        if (regen_value != nullptr) {
            regen_id_ = regen_value->id;
        }
        pos_regen_ = xyz();
    }

    void character::on_move(bool is_attack) {
        last_move_time_ = get_dword_time();

        if (is_attack) {
            last_attack_time_ = last_move_time_;

            if (is_affect_flag(AFF_REVIVE_INVISIBLE))
                remove_affect(AFFECT_REVIVE_INVISIBLE);

            if (is_affect_flag(AFF_EUNHYUNG)) {
                remove_affect(SKILL_EUNHYUNG);
                set_affected_eunhyung();
            } else {
                clear_affected_eunhyung();
            }
        }

        mining_cancel();
    }

    void character::on_click(character *chr_causer) {
        if (!chr_causer) {
            metin2::log::error("{} clicked by a null character", name());
            return;
        }

        uint32_t vid = get_vid();
        metin2::log::debug("{} (vnum {:d}, vid {:d}, pid {:d}) clicked by {}", name(), race_num(), vid, player_id(), chr_causer->name());

        {
            if (chr_causer->my_shop() && chr_causer != this) {
                metin2::log::warn("{} cannot click {}: has a shop open", chr_causer->name(), name());
                return;
            }
        }

        {
            if (chr_causer->get_exchange()) {
                metin2::log::warn("{} cannot click {}: is exchanging", chr_causer->name(), name());
                return;
            }
        }

        if (is_pc()) {
            if (!target_manager::instance().get_target_info(chr_causer->player_id(), TARGET_TYPE_VID, get_vid())) {
                if (my_shop()) {
                    if (chr_causer->is_dead() == true) return;

                    if (chr_causer == this) {
                        if ((get_exchange() || is_open_safebox() || shop_owner()) || is_cube_open()) {
                            chr_causer->chat_packet(CHAT_TYPE_INFO, lc_text("다른 거래중(창고,교환,상점)에는 개인상점을 사용할 수 없습니다."));
                            return;
                        }
                    } else {
                        if ((chr_causer->get_exchange() || chr_causer->is_open_safebox() || chr_causer->my_shop() || chr_causer->shop_owner()) || chr_causer->is_cube_open()) {
                            chr_causer->chat_packet(CHAT_TYPE_INFO, lc_text("다른 거래중(창고,교환,상점)에는 개인상점을 사용할 수 없습니다."));
                            return;
                        }

                        if ((get_exchange() || is_open_safebox() || is_cube_open())) {
                            chr_causer->chat_packet(CHAT_TYPE_INFO, lc_text("상대방이 다른 거래를 하고 있는 중입니다."));
                            return;
                        }
                    }

                    if (chr_causer->get_shop()) {
                        chr_causer->get_shop()->remove_guest(chr_causer);
                        chr_causer->set_shop(nullptr);
                    }

                    my_shop()->add_guest(chr_causer, get_vid(), false);
                    chr_causer->set_shop_owner(this);
                    return;
                }

                if (metin2::config::test_server())
                    metin2::log::debug("{} clicked {}: target is a player", chr_causer->name(), name());

                return;
            }
        }

        chr_causer->set_quest_npcid(get_vid());

        if (quest::quest_manager::instance().click(chr_causer->player_id(), this)) {
            return;
        }

        if (!is_pc()) {
            if (!trigger_on_click_.func) {
                return;
            }

            trigger_on_click_.func(this, chr_causer);
        }
    }

    auto character::gm_level() const -> uint8_t {
        if (metin2::config::test_server())
            return GM_IMPLEMENTOR;
        return points_instant_.gm_level;
    }

    void character::set_gm_level() {
        if (get_desc()) {
            points_instant_.gm_level = gm_get_level(name(), get_desc()->get_account_table().login);
        } else {
            points_instant_.gm_level = GM_PLAYER;
        }
    }

    auto character::is_gm() const -> bool {
        if (points_instant_.gm_level != GM_PLAYER)
            return true;
        if (metin2::config::test_server())
            return true;
        return false;
    }

    void character::set_stone(character *chr_stone) {
        chr_stone_ = chr_stone;

        if (chr_stone_) {
            if (chr_stone->set_chr_spawned_by_.find(this) == chr_stone->set_chr_spawned_by_.end())
                chr_stone->set_chr_spawned_by_.insert(this);
        }
    }

    void character::clear_stone() {
        if (!set_chr_spawned_by_.empty()) {
            for (character *ch : set_chr_spawned_by_) {
                ch->dead(nullptr);
                ch->set_stone(nullptr);
            }
            set_chr_spawned_by_.clear();
        }

        if (!chr_stone_)
            return;

        chr_stone_->set_chr_spawned_by_.erase(this);
        chr_stone_ = nullptr;
    }

    void character::clear_target() {
        if (chr_target_) {
            chr_target_->set_chr_targeted_by_.erase(this);
            chr_target_ = nullptr;
        }

        packet_gc_target p;

        p.header = HEADER_GC_TARGET;
        p.vid = 0;
        p.hp_percent = 0;

        character_set::iterator it = set_chr_targeted_by_.begin();

        while (it != set_chr_targeted_by_.end()) {
            character *pkChr = *(it++);
            pkChr->chr_target_ = nullptr;

            if (!pkChr->get_desc()) {
                metin2::log::error("{} ({}) has no descriptor", pkChr->name(), static_cast<const void *>(pkChr));
                abort();
            }

            pkChr->get_desc()->packet(&p, sizeof(packet_gc_target));
        }

        set_chr_targeted_by_.clear();
    }

    void character::set_target(character *chr_target) {
        if (chr_target_ == chr_target)
            return;

        if (is_castle_map(map_index()) && !is_gm())
            return;

        if (chr_target_)
            chr_target_->set_chr_targeted_by_.erase(this);

        chr_target_ = chr_target;

        packet_gc_target p;

        p.header = HEADER_GC_TARGET;

        if (chr_target_) {
            chr_target_->set_chr_targeted_by_.insert(this);

            p.vid = chr_target_->get_vid();

            if ((chr_target_->is_pc() && !chr_target_->is_polymorphed()) || chr_target_->max_hp() <= 0)
                p.hp_percent = 0;
            else {
                if (chr_target_->race_num() == 20101 ||
                    chr_target_->race_num() == 20102 ||
                    chr_target_->race_num() == 20103 ||
                    chr_target_->race_num() == 20104 ||
                    chr_target_->race_num() == 20105 ||
                    chr_target_->race_num() == 20106 ||
                    chr_target_->race_num() == 20107 ||
                    chr_target_->race_num() == 20108 ||
                    chr_target_->race_num() == 20109) {
                    character *owner = chr_target_->victim();

                    if (owner) {
                        int iHorseHealth = owner->horse_health();
                        int iHorseMaxHealth = owner->horse_max_health();

                        if (iHorseMaxHealth)
                            p.hp_percent = std::clamp(iHorseHealth * 100 / iHorseMaxHealth, 0, 100);
                        else
                            p.hp_percent = 100;
                    } else
                        p.hp_percent = 100;
                } else
                    p.hp_percent = std::clamp((chr_target_->hp() * 100) / chr_target_->max_hp(), 0, 100);
            }
        } else {
            p.vid = 0;
            p.hp_percent = 0;
        }

        get_desc()->packet(&p, sizeof(packet_gc_target));
    }

    void character::broadcast_target_packet() {
        if (set_chr_targeted_by_.empty())
            return;

        packet_gc_target p;

        p.header = HEADER_GC_TARGET;
        p.vid = get_vid();

        if (is_pc())
            p.hp_percent = 0;
        else
            p.hp_percent = std::clamp((hp() * 100) / max_hp(), 0, 100);

        character_set::iterator it = set_chr_targeted_by_.begin();

        while (it != set_chr_targeted_by_.end()) {
            character *pkChr = *it++;

            if (!pkChr->get_desc()) {
                metin2::log::error("{} ({}) has no descriptor", pkChr->name(), static_cast<const void *>(pkChr));
                abort();
            }

            pkChr->get_desc()->packet(&p, sizeof(packet_gc_target));
        }
    }

    void character::check_target() {
        if (!chr_target_)
            return;

        if (distance_approx(x() - chr_target_->x(), y() - chr_target_->y()) >= 4800)
            set_target(nullptr);
    }

    void character::set_warp_location(int32_t map_index, int32_t x, int32_t y) {
        pos_warp_.x = x * 100;
        pos_warp_.y = y * 100;
        warp_map_index_ = map_index;
    }

    void character::save_exit_location() {
        pos_exit_ = xyz();
        exit_map_index_ = map_index();
    }

    void character::exit_to_saved_location() {
        metin2::log::debug("{}: exit to saved location", name());
        warp_set(pos_warp_.x, pos_warp_.y, warp_map_index_);

        pos_exit_.x = pos_exit_.y = pos_exit_.z = 0;
        exit_map_index_ = 0;
    }

    auto character::warp_set(int32_t x, int32_t y, int32_t private_map_index) -> bool {
        if (!is_pc())
            return false;

        int32_t lAddr;
        int32_t lMapIndex;
        uint16_t wPort;

        if (!map_locations::instance().get(x, y, lMapIndex, lAddr, wPort)) {
            metin2::log::error("Cannot find map {:d} at {:d} {:d} for {}", lMapIndex, x, y, name());
            return false;
        }

        if (private_map_index >= 10000) {
            if (private_map_index / 10000 != lMapIndex) {
                metin2::log::error("Invalid map index {:d}: must be a child of {:d}", private_map_index, lMapIndex);
                return false;
            }

            lMapIndex = private_map_index;
        }

        stop();
        save();

        if (get_sectree()) {
            get_sectree()->remove_entity(this);
            view_cleanup();

            encode_remove_packet(this);
        }

        warp_map_index_ = lMapIndex;
        pos_warp_.x = x;
        pos_warp_.y = y;

        metin2::log::info("{} warps to {:d} {:d} (map {:d} -> {:d})", name(), x, y, map_index(), lMapIndex);

        packet_gc_warp p;

        p.header = HEADER_GC_WARP;
        p.x = x;
        p.y = y;
        p.addr = lAddr;

        if (!proxy_ip.empty())
            p.addr = inet_addr(proxy_ip.c_str());

        p.port = wPort;

        get_desc()->packet(&p, sizeof(packet_gc_warp));

        {
            const std::string buf = std::format("{} MapIdx {} DestMapIdx{} DestX{} DestY{} Empire{}", name(), map_index(), private_map_index, x, y, empire());
            log_manager::instance().char_log(this, 0, "WARP", buf.c_str());
        }

        return true;
    }

    void character::warp_end() {
        if (metin2::config::test_server())
            metin2::log::debug("{}: warp end", name());

        if (pos_warp_.x == 0 && pos_warp_.y == 0)
            return;

        int index = warp_map_index_;

        if (index > 10000)
            index /= 10000;

        if (!map_allow_find(index)) {
            metin2::log::error("Location {:d} {:d} is not served by this server", pos_warp_.x, pos_warp_.y);
            get_desc()->set_phase(PHASE_CLOSE);
            return;
        }

        metin2::log::info("{}: warp ended on map {:d} at {:d} {:d}", name(), warp_map_index_, pos_warp_.x, pos_warp_.y);

        show(warp_map_index_, pos_warp_.x, pos_warp_.y, 0);
        stop();

        warp_map_index_ = 0;
        pos_warp_.x = pos_warp_.y = pos_warp_.z = 0;

        {
            packet_gg_login p;

            p.header = HEADER_GG_LOGIN;
            strlcpy(p.name, name(), sizeof(p.name));
            p.pid = player_id();
            p.empire = empire();
            p.map_index = sectree_manager::instance().map_index(x(), y());
            p.channel = metin2::config::server().channel;

            p2p_manager::instance().send(&p, sizeof(packet_gg_login));
        }
    }

    auto character::return_home() -> bool {
        if (!is_npc())
            return false;

        int x, y;
        set_victim(nullptr);

        x = mob_inst->pos_last_attacked.x;
        y = mob_inst->pos_last_attacked.y;

        set_rotation_to_xy(x, y);

        if (!go_to(x, y))
            return false;

        send_move_packet(FUNC_WAIT, 0, 0, 0, 0);

        if (metin2::config::test_server())
            metin2::log::debug("{} ({}) gives up the chase and returns to {:d} {:d}", name(), static_cast<const void *>(this), x, y);

        if (get_party())
            get_party()->send_message(this, PM_RETURN, x, y);

        return true;
    }

    auto character::follow(character *chr, float min_distance) -> bool {
        if (is_pc()) {
            metin2::log::error("Follow called on player {}", name());
            return false;
        }

        if (is_set(points_instant_.ai_flag, AIFLAG_NOMOVE)) {
            if (chr->is_pc()) {
                if (!get_party() || !get_party()->leader() || get_party()->leader() == this) {
                    if (get_dword_time() - mob_inst->last_attacked_time >= 15000) {
                        if (mob_data->table.attack_range < distance_approx(chr->x() - x(), chr->y() - y()))
                            if (return_home())
                                return true;
                    }
                }
            }
            return false;
        }

        int32_t x_value = chr->x();
        int32_t y_value = chr->y();

        if (chr->is_pc()) {
            if (!get_party() || !get_party()->leader() || get_party()->leader() == this) {
                if (get_dword_time() - mob_inst->last_attacked_time >= 15000) {
                    if (5000 < distance_approx(mob_inst->pos_last_attacked.x - x(), mob_inst->pos_last_attacked.y - y()))
                        if (return_home())
                            return true;
                }
            }
        }

        if (is_guard_npc()) {
            if (5000 < distance_approx(mob_inst->pos_last_attacked.x - x(), mob_inst->pos_last_attacked.y - y()))
                if (return_home())
                    return true;
        }

        if (chr->is_state(chr->state_move_) &&
            mob_battle_type() != BATTLE_TYPE_RANGE &&
            mob_battle_type() != BATTLE_TYPE_MAGIC &&
            false == is_pet()) {
            float rot = chr->rotation();
            float rot_delta = get_degree_delta(rot, get_degree_from_position_xy(x(), y(), chr->x(), chr->y()));

            float yourSpeed = chr->move_speed();
            float mySpeed = move_speed();

            float fDist = distance_sqrt(x_value - x(), y_value - y());
            float fFollowSpeed = mySpeed - yourSpeed * cos(rot_delta * std::numbers::pi / 180);

            if (fFollowSpeed >= 0.1f) {
                float fMeetTime = fDist / fFollowSpeed;
                float fYourMoveEstimateX, fYourMoveEstimateY;

                if (fMeetTime * yourSpeed <= 100000.0f) {
                    get_delta_by_degree(chr->rotation(), fMeetTime * yourSpeed, &fYourMoveEstimateX, &fYourMoveEstimateY);

                    x_value += (int32_t)fYourMoveEstimateX;
                    y_value += (int32_t)fYourMoveEstimateY;

                    float fDistNew = sqrt(((double)x_value - x()) * (x_value - x()) + ((double)y_value - y()) * (y_value - y()));
                    if (fDist < fDistNew) {
                        x_value = (int32_t)(x() + (x_value - x()) * fDist / fDistNew);
                        y_value = (int32_t)(y() + (y_value - y()) * fDist / fDistNew);
                    }
                }
            }
        }

        set_rotation_to_xy(x_value, y_value);

        float fDist = distance_sqrt(x_value - x(), y_value - y());

        if (fDist <= min_distance)
            return false;

        float fx, fy;

        if (is_change_attack_position(chr) && get_mob_rank() < MOB_RANK_BOSS) {
            set_change_attack_position_time();

            int retry = 16;
            int dx, dy;
            int rot = (int)get_degree_from_position_xy(x_value, y_value, x(), y());

            while (--retry) {
                if (fDist < 500.0f)
                    get_delta_by_degree((rot + number(-90, 90) + number(-90, 90)) % 360, min_distance, &fx, &fy);
                else
                    get_delta_by_degree(number(0, 359), min_distance, &fx, &fy);

                dx = x_value + (int)fx;
                dy = y_value + (int)fy;

                sectree *tree = sectree_manager::instance().get(map_index(), dx, dy);

                if (nullptr == tree)
                    break;

                if (0 == (tree->get_attribute(dx, dy) & (ATTR_BLOCK | ATTR_OBJECT)))
                    break;
            }

            if (!go_to(dx, dy))
                return false;
        } else {
            float fDistToGo = fDist - min_distance;
            get_delta_by_degree(rotation(), fDistToGo, &fx, &fy);

            if (!go_to(x() + (int)fx, y() + (int)fy))
                return false;
        }

        send_move_packet(FUNC_WAIT, 0, 0, 0, 0);
        return true;
    }

    auto character::distance_from_safebox_open() const -> float {
        return distance_approx(x() - pos_safebox_open_.x, y() - pos_safebox_open_.y);
    }

    void character::set_safebox_open_position() {
        pos_safebox_open_ = xyz();
    }

    auto character::get_safebox() const -> safebox * {
        return safebox_.get();
    }

    void character::req_safebox_load(const char *password_value) {
        if (!*password_value || strlen(password_value) > SAFEBOX_PASSWORD_MAX_LEN) {
            chat_packet(CHAT_TYPE_INFO, lc_text("<창고> 잘못된 암호를 입력하셨습니다."));
            return;
        } else if (safebox_) {
            chat_packet(CHAT_TYPE_INFO, lc_text("<창고> 창고가 이미 열려있습니다."));
            return;
        }

        int iPulse = thecore_pulse();

        if (iPulse - safebox_load_time() < passes_per_sec(10)) {
            chat_packet(CHAT_TYPE_INFO, lc_text("<창고> 창고를 닫은지 10초 안에는 열 수 없습니다."));
            return;
        } else if (distance_from_safebox_open() > 1000) {
            chat_packet(CHAT_TYPE_INFO, lc_text("<창고> 거리가 멀어서 창고를 열 수 없습니다."));
            return;
        } else if (opening_safebox_) {
            metin2::log::warn("Duplicate safebox load request from {}", name());
            return;
        }

        set_safebox_load_time();
        opening_safebox_ = true;

        safebox_load_packet p;
        p.id = get_desc()->get_account_table().id;
        strlcpy(p.login, get_desc()->get_account_table().login, sizeof(p.login));
        strlcpy(p.password, password_value, sizeof(p.password));

        db_clientdesc->db_packet(HEADER_GD_SAFEBOX_LOAD, get_desc()->handle(), &p, sizeof(p));
    }

    void character::load_safebox(int size_value, uint32_t gold, int item_count, player_item *items) {
        bool bLoaded = false;

        set_open_safebox(true);

        if (safebox_)
            bLoaded = true;

        if (!safebox_)
            safebox_ = std::make_unique<safebox>(this, size_value, gold);
        else
            safebox_->change_size(size_value);

        safebox_size_ = size_value;

        packet_cg_safebox_size p;

        p.header = HEADER_GC_SAFEBOX_SIZE;
        p.size = size_value;

        get_desc()->packet(&p, sizeof(packet_cg_safebox_size));

        if (!bLoaded) {
            for (int i = 0; i < item_count; ++i, ++items) {
                if (!safebox_->is_valid_position(items->pos))
                    continue;

                item *item = item_manager::instance().create_item(items->vnum, items->count, items->id);

                if (!item) {
                    metin2::log::error("Cannot create item vnum {:d} id {:d} for {}", items->vnum, items->id, name());
                    continue;
                }

                item->set_skip_save(true);
                item->set_sockets(items->sockets);
                item->set_attributes(items->attr);

                if (!safebox_->add(items->pos, item)) {
                    item_manager::instance().destroy_item(item);
                } else
                    item->set_skip_save(false);
            }
        }
    }

    void character::change_safebox_size(uint8_t size_value) {

        packet_cg_safebox_size p;

        p.header = HEADER_GC_SAFEBOX_SIZE;
        p.size = size_value;

        get_desc()->packet(&p, sizeof(packet_cg_safebox_size));

        if (safebox_)
            safebox_->change_size(size_value);

        safebox_size_ = size_value;
    }

    void character::close_safebox() {
        if (!safebox_)
            return;

        set_open_safebox(false);

        safebox_->save();

        safebox_.reset();

        chat_packet(CHAT_TYPE_COMMAND, "CloseSafebox");

        set_safebox_load_time();
        opening_safebox_ = false;

        save();
    }

    auto character::mall() const -> safebox * {
        return mall_.get();
    }

    void character::load_mall(int item_count, player_item *items) {
        bool bLoaded = false;

        if (mall_)
            bLoaded = true;

        if (!mall_)
            mall_ = std::make_unique<safebox>(this, 3 * SAFEBOX_PAGE_SIZE, 0);
        else
            mall_->change_size(3 * SAFEBOX_PAGE_SIZE);

        mall_->set_window_mode(MALL);

        packet_cg_safebox_size p;

        p.header = HEADER_GC_MALL_OPEN;
        p.size = 3 * SAFEBOX_PAGE_SIZE;

        get_desc()->packet(&p, sizeof(packet_cg_safebox_size));

        if (!bLoaded) {
            for (int i = 0; i < item_count; ++i, ++items) {
                if (!mall_->is_valid_position(items->pos))
                    continue;

                item *item = item_manager::instance().create_item(items->vnum, items->count, items->id);

                if (!item) {
                    metin2::log::error("Cannot create item vnum {:d} id {:d} for {}", items->vnum, items->id, name());
                    continue;
                }

                item->set_skip_save(true);
                item->set_sockets(items->sockets);
                item->set_attributes(items->attr);

                if (!mall_->add(items->pos, item))
                    item_manager::instance().destroy_item(item);
                else
                    item->set_skip_save(false);
            }
        }
    }

    void character::close_mall() {
        if (!mall_)
            return;

        mall_->save();

        mall_.reset();

        chat_packet(CHAT_TYPE_COMMAND, "CloseMall");
    }

    auto character::build_update_party_packet(packet_gc_party_update &out) -> bool {
        if (!get_party())
            return false;

        memset(&out, 0, sizeof(out));

        out.header = HEADER_GC_PARTY_UPDATE;
        out.pid = player_id();
        out.percent_hp = std::clamp(hp() * 100 / max_hp(), 0, 100);
        out.role = get_party()->role(player_id());

        metin2::log::debug("Party role of {} is {:d}", name(), out.role);

        character *l = get_party()->leader_character();

        if (l && distance_approx(x() - l->x(), y() - l->y()) < PARTY_DEFAULT_RANGE) {
            out.affects[0] = get_party()->party_bonus_exp_percent();
            out.affects[1] = point(POINT_PARTY_ATTACKER_BONUS);
            out.affects[2] = point(POINT_PARTY_TANKER_BONUS);
            out.affects[3] = point(POINT_PARTY_BUFFER_BONUS);
            out.affects[4] = point(POINT_PARTY_SKILL_MASTER_BONUS);
            out.affects[5] = point(POINT_PARTY_HASTE_BONUS);
            out.affects[6] = point(POINT_PARTY_DEFENDER_BONUS);
        }

        return true;
    }

    auto character::leadership_skill_level() const -> int {
        return skill_level(SKILL_LEADERSHIP);
    }

    void character::query_safebox_size() {
        if (safebox_size_ == -1) {
            db_manager::instance().func_query([pid = player_id()](sql_msg &msg) {
                character *ch = character_manager::instance().find_by_pid(pid);

                if (ch) {
                    if (msg.get()->num_rows > 0) {
                        MYSQL_ROW row = mysql_fetch_row(msg.get()->res.get());
                        int size = 0;
                        str_to_number(size, row[0]);
                        ch->set_safebox_size(SAFEBOX_PAGE_SIZE * size);
                    }
                }
            },
                sql::format("SELECT size FROM safebox{} WHERE account_id = {}", sql::raw(get_table_postfix()), get_desc()->get_account_table().id));
        }
    }

    void character::set_safebox_size(int size) {
        metin2::log::debug("{}: safebox size set to {:d}", name(), size);
        safebox_size_ = size;
        db_manager::instance().query(sql::format("UPDATE safebox{} SET size = {} WHERE account_id = {}", sql::raw(get_table_postfix()), size / SAFEBOX_PAGE_SIZE, get_desc()->get_account_table().id));
    }

    auto character::safebox_size() const -> int {
        return safebox_size_;
    }

    void character::set_now_walking(bool walk_flag) {
        if (now_walking_ != walk_flag) {
            if (walk_flag) {
                now_walking_ = true;
                walk_start_time_ = get_dword_time();
            } else {
                now_walking_ = false;
            }

            {
                packet_gc_walk_mode p;
                p.vid = get_vid();
                p.header = HEADER_GC_WALK_MODE;
                p.mode = now_walking_ ? WALKMODE_WALK : WALKMODE_RUN;

                packet_view(&p, sizeof(p));
            }

            if (is_npc()) {
                if (now_walking_)
                    monster_log("걷는다");
                else
                    monster_log("뛴다");
            }
        }
    }

    void character::start_stamina_consume() {
        if (stamina_consume_)
            return;
        point_change(POINT_STAMINA, 0);
        stamina_consume_ = true;
        if (is_stamina_half_consume())
            chat_packet(CHAT_TYPE_COMMAND, "StartStaminaConsume %d %d", STAMINA_PER_STEP * metin2::config::server().passes_per_sec / 2, stamina());
        else
            chat_packet(CHAT_TYPE_COMMAND, "StartStaminaConsume %d %d", STAMINA_PER_STEP * metin2::config::server().passes_per_sec, stamina());
    }

    void character::stop_stamina_consume() {
        if (!stamina_consume_)
            return;
        point_change(POINT_STAMINA, 0);
        stamina_consume_ = false;
        chat_packet(CHAT_TYPE_COMMAND, "StopStaminaConsume %d", stamina());
    }

    auto character::is_stamina_consume() const -> bool {
        return stamina_consume_;
    }

    auto character::is_stamina_half_consume() const -> bool {
        return is_equip_unique_item(UNIQUE_ITEM_HALF_STAMINA);
    }

    void character::reset_stop_time() {
        stop_time_ = get_dword_time();
    }

    auto character::stop_time() const -> uint32_t {
        return stop_time_;
    }

    void character::reset_point(int lv) {
        uint8_t bJob = job();

        point_change(POINT_LEVEL, lv - level());

        set_real_point(POINT_ST, job_initial_points[bJob].st);
        set_point(POINT_ST, real_point(POINT_ST));

        set_real_point(POINT_HT, job_initial_points[bJob].ht);
        set_point(POINT_HT, real_point(POINT_HT));

        set_real_point(POINT_DX, job_initial_points[bJob].dx);
        set_point(POINT_DX, real_point(POINT_DX));

        set_real_point(POINT_IQ, job_initial_points[bJob].iq);
        set_point(POINT_IQ, real_point(POINT_IQ));

        set_random_hp((lv - 1) * number(job_initial_points[job()].hp_per_lv_begin, job_initial_points[job()].hp_per_lv_end));
        set_random_sp((lv - 1) * number(job_initial_points[job()].sp_per_lv_begin, job_initial_points[job()].sp_per_lv_end));

        if (lv <= 90)
            point_change(POINT_STAT, ((std::clamp(lv, 1, 90) - 1) * 3) + point(POINT_LEVEL_STEP) - point(POINT_STAT));
        else
            point_change(POINT_STAT, 270 - point(POINT_STAT));

        compute_points();

        point_change(POINT_HP, max_hp() - hp());
        point_change(POINT_SP, max_sp() - sp());

        points_packet();

        log_manager::instance().char_log(this, 0, "RESET_POINT", "");
    }

    auto character::is_change_attack_position(character *target) const -> bool {
        if (!is_npc())
            return true;

        uint32_t dwChangeTime = AI_CHANGE_ATTACK_POISITION_TIME_NEAR;

        if (distance_approx(x() - target->x(), y() - target->y()) >
            AI_CHANGE_ATTACK_POISITION_DISTANCE + mob_attack_range())
            dwChangeTime = AI_CHANGE_ATTACK_POISITION_TIME_FAR;

        return get_dword_time() - last_change_attack_position_time_ > dwChangeTime;
    }

    void character::give_random_skill_book() {
        item *item = auto_give_item(50300);

        if (nullptr != item) {
            uint8_t bJob = 0;

            if (!number(0, 1))
                bJob = job() + 1;

            uint32_t dwSkillVnum = 0;

            do {
                dwSkillVnum = number(1, 111);
                const skill_proto *pkSk = skill_manager::instance().get(dwSkillVnum);

                if (nullptr == pkSk)
                    continue;

                if (bJob && bJob != pkSk->type)
                    continue;

                break;
            } while (true);

            item->set_socket(0, dwSkillVnum);
        }
    }

    void character::revive_invisible(int dur) {
        add_affect(AFFECT_REVIVE_INVISIBLE, POINT_NONE, 0, AFF_REVIVE_INVISIBLE, dur, 0, true);
    }

    void character::toggle_monster_log() {
        monster_log_ = !monster_log_;

        if (monster_log_) {
            character_manager::instance().register_for_monster_log(this);
        } else {
            character_manager::instance().unregister_for_monster_log(this);
        }
    }

    void character::set_guild(guild *guild_value) {
        if (guild_ != guild_value) {
            guild_ = guild_value;
            update_packet();
        }
    }

    void character::send_greet_message() {
        __typeof(db_manager::instance().greet_message()) v = db_manager::instance().greet_message();

        for (auto it = v.begin(); it != v.end(); ++it) {
            chat_packet(CHAT_TYPE_NOTICE, it->c_str());
        }
    }

    void character::begin_state_empty() {
        monster_log("!");
    }

    void character::effect_packet(int enum_effect_type) {
        packet_gc_special_effect p;

        p.header = HEADER_GC_SEPCIAL_EFFECT;
        p.type = enum_effect_type;
        p.vid = get_vid();

        packet_around(&p, sizeof(packet_gc_special_effect));
    }

    void character::specific_effect_packet(const char filename[MAX_EFFECT_FILE_NAME]) {
        packet_gc_specific_effect p;

        p.header = HEADER_GC_SPECIFIC_EFFECT;
        p.vid = get_vid();
        memcpy(p.effect_file, filename, MAX_EFFECT_FILE_NAME);

        packet_around(&p, sizeof(packet_gc_specific_effect));
    }

    void character::monster_chat(uint8_t monster_chat_type) {
        if (is_pc())
            return;

        std::string sbuf;

        if (is_monster()) {
            if (number(0, 60))
                return;

            sbuf = std::format("(locale.monster_chat[{}] and locale.monster_chat[{}][{}] or '')",
                race_num(), race_num(), monster_chat_type * 3 + number(1, 3));
        } else {
            if (monster_chat_type != MONSTER_CHAT_WAIT)
                return;

            if (is_guard_npc()) {
                if (number(0, 6))
                    return;
            } else {
                if (number(0, 30))
                    return;
            }

            sbuf = std::format("(locale.monster_chat[{}] and locale.monster_chat[{}][number(1, table.getn(locale.monster_chat[{}]))] or '')", race_num(), race_num(), race_num());
        }

        std::string text = quest::script_to_string(sbuf);

        if (text.empty())
            return;

        struct packet_gc_chat pack_chat;

        pack_chat.header = HEADER_GC_CHAT;
        pack_chat.size = sizeof(struct packet_gc_chat) + text.size() + 1;
        pack_chat.type = CHAT_TYPE_TALKING;
        pack_chat.id = get_vid();
        pack_chat.empire = 0;

        temp_buffer buf;
        buf.write(&pack_chat, sizeof(struct packet_gc_chat));
        buf.write(text.c_str(), text.size() + 1);

        packet_around(buf.read_peek(), buf.size());
    }

    void character::set_quest_npcid(uint32_t vid) {
        quest_npcvid_ = vid;
    }

    auto character::quest_npc() const -> character * {
        return character_manager::instance().find(quest_npcvid_);
    }

    void character::set_quest_item_ptr(item *item) {
        quest_item_ = item;
    }

    void character::clear_quest_item_ptr() {
        quest_item_ = nullptr;
    }

    auto character::quest_item_ptr() const -> item * {
        return quest_item_;
    }

    auto character::dungeon_force() const -> dungeon * {
        if (warp_map_index_ > 10000)
            return dungeon_manager::instance().find_by_map_index(warp_map_index_);

        return dungeon_;
    }

    void character::set_block_mode(uint8_t flag) {
        points_instant_.block_mode = flag;

        chat_packet(CHAT_TYPE_COMMAND, "setblockmode %d", points_instant_.block_mode);

        set_quest_flag("game_option.block_exchange", flag & BLOCK_EXCHANGE ? 1 : 0);
        set_quest_flag("game_option.block_party_invite", flag & BLOCK_PARTY_INVITE ? 1 : 0);
        set_quest_flag("game_option.block_guild_invite", flag & BLOCK_GUILD_INVITE ? 1 : 0);
        set_quest_flag("game_option.block_whisper", flag & BLOCK_WHISPER ? 1 : 0);
        set_quest_flag("game_option.block_messenger_invite", flag & BLOCK_MESSENGER_INVITE ? 1 : 0);
        set_quest_flag("game_option.block_party_request", flag & BLOCK_PARTY_REQUEST ? 1 : 0);
    }

    void character::set_block_mode_force(uint8_t flag) {
        points_instant_.block_mode = flag;
        chat_packet(CHAT_TYPE_COMMAND, "setblockmode %d", points_instant_.block_mode);
    }

    auto character::is_guard_npc() const -> bool {
        return is_npc() && (race_num() == 11000 || race_num() == 11002 || race_num() == 11004);
    }

    auto character::polymorph_power() const -> int {
        if (metin2::config::test_server()) {
            int value = quest::quest_manager::instance().event_flag("poly");
            if (value)
                return value;
        }
        return polymorph_power_by_level[std::clamp(skill_level(SKILL_POLYMORPH), 0, 40)];
    }

    void character::set_polymorph(uint32_t race_num, bool maintain_stat) {
        if (race_num < JOB_MAX_NUM) {
            race_num = 0;
            maintain_stat = false;
        }

        if (polymorph_race_ == race_num)
            return;

        poly_maintain_stat_ = maintain_stat;
        polymorph_race_ = race_num;

        metin2::log::info("{} polymorphed into race {:d}", name(), race_num);

        if (race_num != 0)
            stop_riding();

        set_bit(add_chr_state_, ADD_CHARACTER_STATE_SPAWN);
        af_affect_flag_.set(AFF_SPAWN);

        view_reencode();

        remove_bit(add_chr_state_, ADD_CHARACTER_STATE_SPAWN);

        if (!maintain_stat) {
            point_change(POINT_ST, 0);
            point_change(POINT_DX, 0);
            point_change(POINT_IQ, 0);
            point_change(POINT_HT, 0);
        }

        set_valid_combo_interval(0);
        set_combo_sequence(0);

        compute_battle_points();
    }

    auto character::quest_flag(const std::string &flag) const -> int {
        quest::quest_manager &q = quest::quest_manager::instance();
        quest::pc *pPC = q.get_pc(player_id());
        return pPC->flag(flag);
    }

    void character::set_quest_flag(const std::string &flag, int value) {
        quest::quest_manager &q = quest::quest_manager::instance();
        quest::pc *pPC = q.get_pc(player_id());
        pPC->set_flag(flag, value);
    }

    void character::determine_drop_metin_stone() {
        const int METIN_STONE_NUM = 14;
        static uint32_t c_adwMetin[METIN_STONE_NUM] =
            {
                28030,
                28031,
                28032,
                28033,
                28034,
                28035,
                28036,
                28037,
                28038,
                28039,
                28040,
                28041,
                28042,
                28043,
        };
        uint32_t stone_num = race_num();
        int idx = std::lower_bound(stone_drop, stone_drop + STONE_INFO_MAX_NUM, stone_num) - stone_drop;
        if (idx >= STONE_INFO_MAX_NUM || stone_drop[idx].mob_vnum != stone_num) {
            drop_metin_stone_ = 0;
        } else {
            const stone_drop_info &info = stone_drop[idx];
            drop_metin_stone_pct_ = info.drop_pct;
            {
                drop_metin_stone_ = c_adwMetin[number(0, METIN_STONE_NUM - 1)];
                int iGradePct = number(1, 100);
                for (int iStoneLevel = 0; iStoneLevel < STONE_LEVEL_MAX_NUM; iStoneLevel++) {
                    int iLevelGradePortion = info.level_pct[iStoneLevel];
                    if (iGradePct <= iLevelGradePortion) {
                        break;
                    } else {
                        iGradePct -= iLevelGradePortion;
                        drop_metin_stone_ += 100;
                    }
                }
            }
        }
    }

    void character::send_equipment(character *ch) {
        packet_view_equip p;
        p.header = HEADER_GC_VIEW_EQUIP;
        p.vid = get_vid();
        for (int i = 0; i < WEAR_MAX_NUM; i++) {
            item *item = wear(i);
            if (item) {
                p.equips[i].vnum = item->vnum();
                p.equips[i].count = item->count();

                std::memcpy(p.equips[i].sockets, item->sockets(), sizeof(p.equips[i].sockets));
                std::memcpy(p.equips[i].attr, item->attributes(), sizeof(p.equips[i].attr));
            } else {
                p.equips[i].vnum = 0;
            }
        }
        ch->get_desc()->packet(&p, sizeof(p));
    }

    auto character::can_summon(int leader_ship) -> bool {
        return (leader_ship >= 20 || (leader_ship >= 12 && last_dead_time_ + 180 > get_dword_time()));
    }

    void character::mount_vnum(uint32_t vnum) {
        if (mount_vnum_ == vnum)
            return;

        mount_vnum_ = vnum;
        mount_time_ = get_dword_time();

        if (is_observer_)
            return;

        // Mounting keeps the position: the client adjusts it for collisions, and moving to the
        // server position skips that check and can leave the character stuck.
        pos_dest_.x = pos_start_.x = x();
        pos_dest_.y = pos_start_.y = y();
        encode_insert_packet(this);

        entity_map::iterator it = map_view_.begin();

        while (it != map_view_.end()) {
            entity *entity = (it++)->first;

            encode_insert_packet(entity);
        }

        set_valid_combo_interval(0);
        set_combo_sequence(0);

        compute_points();
    }

    EVENTFUNC(warp_npc_event) {
        char_event_info *info = dynamic_cast<char_event_info *>(event->info);
        if (info == nullptr) {
            metin2::log::error("Warp NPC event fired without event info");
            return 0;
        }

        character *ch = info->ch;

        if (ch == nullptr) {
            return 0;
        }

        if (!ch->get_sectree()) {
            ch->warp_npc_event = nullptr;
            return 0;
        }

        int32_t target_x = 0;
        int32_t target_y = 0;
        const int32_t x = ch->x();
        const int32_t y = ch->y();
        const uint8_t empire = ch->empire();

        char szTmp[64];

        if (3 != sscanf(ch->name(), " %s %d %d ", szTmp, &target_x, &target_y)) {
            if (number(1, 100) < 5)
                metin2::log::error("Warp NPC {:d} has an invalid name: {}", ch->race_num(), ch->name());

            return metin2::config::server().passes_per_sec / 2;
        }

        target_x *= 100;
        target_y *= 100;

        bool use_warp = true;

        if (ch->is_goto()) {
            sectree_map *pkSectreeMap = sectree_manager::instance().map(ch->map_index());
            target_x += pkSectreeMap->setting.base_x;
            target_y += pkSectreeMap->setting.base_y;
            use_warp = false;
        }

        ch->get_sectree()->for_each_around([&](entity *ent) {
            if (!ent->is_type(ENTITY_CHARACTER))
                return;

            character *pkChr = (character *)ent;

            if (!pkChr->is_pc())
                return;

            int iDist = distance_approx(pkChr->x() - x, pkChr->y() - y);

            if (iDist > 300)
                return;

            if (empire && pkChr->empire() && empire != pkChr->empire())
                return;

            if (pkChr->is_hack())
                return;

            if (!pkChr->can_handle_item(false, true))
                return;

            if (use_warp)
                pkChr->warp_set(target_x, target_y);
            else {
                pkChr->show(pkChr->map_index(), target_x, target_y);
                pkChr->stop();
            }
        });

        return metin2::config::server().passes_per_sec / 2;
    }

    void character::start_warp_npc_event() {
        if (warp_npc_event)
            return;

        if (!is_warp() && !is_goto())
            return;

        char_event_info *info = alloc_event_info<char_event_info>();

        info->ch = this;

        warp_npc_event = event_create(game::characters::warp_npc_event, info, metin2::config::server().passes_per_sec / 2);
    }

    void character::sync_packet() {
        temp_buffer buf;

        packet_cg_sync_position_element elem;

        elem.vid = get_vid();
        elem.x = x();
        elem.y = y();

        packet_gc_sync_position pack;

        pack.header = HEADER_GC_SYNC_POSITION;
        pack.size = sizeof(packet_gc_sync_position) + sizeof(elem);

        buf.write(&pack, sizeof(pack));
        buf.write(&elem, sizeof(elem));

        packet_around(buf.read_peek(), buf.size());
    }

    auto character::marry_partner() const -> character * {
        return chr_married_;
    }

    void character::set_marry_partner(character *ch) {
        chr_married_ = ch;
    }

    auto character::marriage_bonus(uint32_t item_vnum, bool sum) -> int {
        if (is_npc())
            return 0;

        marriage::marriage *pMarriage = marriage::manager::instance().get(player_id());

        if (!pMarriage)
            return 0;

        return pMarriage->bonus(item_vnum, sum, this);
    }

    void character::confirm_with_msg(const char *msg_value, int timeout_value, uint32_t request_pid_value) {
        if (!is_pc())
            return;

        packet_gc_quest_confirm p;

        p.header = HEADER_GC_QUEST_CONFIRM;
        p.request_pid = request_pid_value;
        p.timeout = timeout_value;
        strlcpy(p.msg, msg_value, sizeof(p.msg));

        get_desc()->packet(&p, sizeof(p));
    }

    auto character::premium_remain_seconds(uint8_t type) const -> int {
        if (type >= PREMIUM_MAX_NUM)
            return 0;

        return premium_times_[type] - get_global_time();
    }

    auto character::warp_to_pid(uint32_t pid) -> bool {
        character *victim;
        if ((victim = (character_manager::instance().find_by_pid(pid)))) {
            int mapIdx = victim->map_index();
            if (is_summonable_zone(mapIdx)) {
                if (can_enter_zone(this, mapIdx)) {
                    warp_set(victim->x(), victim->y());
                } else {
                    chat_packet(CHAT_TYPE_INFO, lc_text("상대방이 있는 곳으로 워프할 수 없습니다."));
                    return false;
                }
            } else {
                chat_packet(CHAT_TYPE_INFO, lc_text("상대방이 있는 곳으로 워프할 수 없습니다."));
                return false;
            }
        } else {
            cci *pcci = p2p_manager::instance().find_by_pid(pid);

            if (!pcci) {
                chat_packet(CHAT_TYPE_INFO, lc_text("상대방이 온라인 상태가 아닙니다."));
                return false;
            }

            if (pcci->channel != metin2::config::server().channel) {
                chat_packet(CHAT_TYPE_INFO, lc_text("상대방이 %d 채널에 있습니다. (현재 채널 %d)"), pcci->channel, metin2::config::server().channel);
                return false;
            } else if (false == is_summonable_zone(pcci->map_index)) {
                chat_packet(CHAT_TYPE_INFO, lc_text("상대방이 있는 곳으로 워프할 수 없습니다."));
                return false;
            } else {
                if (!can_enter_zone(this, pcci->map_index)) {
                    chat_packet(CHAT_TYPE_INFO, lc_text("상대방이 있는 곳으로 워프할 수 없습니다."));
                    return false;
                }

                packet_gg_find_position p;
                p.header = HEADER_GG_FIND_POSITION;
                p.from_pid = player_id();
                p.target_pid = pid;
                pcci->desc->packet(&p, sizeof(packet_gg_find_position));

                if (metin2::config::test_server())
                    chat_packet(CHAT_TYPE_PARTY, "sent find position packet for teleport");
            }
        }
        return true;
    }

    auto character::refine_guild() const -> guild * {
        character *chRefineNPC = character_manager::instance().find(refine_npcvid_);

        return (chRefineNPC ? chRefineNPC->get_guild() : nullptr);
    }

    auto character::is_refine_through_guild() const -> bool {
        return refine_guild() != nullptr;
    }

    auto character::compute_refine_fee(int cost, int multiply) const -> int {
        guild *pGuild = refine_guild();
        if (pGuild) {
            if (pGuild == get_guild())
                return cost * multiply * 9 / 10;

            character *chRefineNPC = character_manager::instance().find(refine_npcvid_);
            if (chRefineNPC && chRefineNPC->empire() != empire())
                return cost * multiply * 3;

            return cost * multiply;
        } else
            return cost;
    }

    void character::pay_refine_fee(int total_money) {
        int iFee = total_money / 10;
        guild *pGuild = refine_guild();

        int iRemain = total_money;

        if (pGuild) {
            if (pGuild != get_guild()) {
                pGuild->request_deposit_money(this, iFee);
                iRemain -= iFee;
            }
        }

        point_change(POINT_GOLD, -iRemain);
    }

    auto character::is_hack(bool send_msg, bool check_shop_owner, int limittime) -> bool {
        const int iPulse = thecore_pulse();

        if (metin2::config::test_server())
            send_msg = true;

        if (iPulse - safebox_load_time() < passes_per_sec(limittime)) {
            if (send_msg)
                chat_packet(CHAT_TYPE_INFO, lc_text("창고를 연후 %d초 이내에는 다른곳으로 이동할수 없습니다."), limittime);

            if (metin2::config::test_server())
                chat_packet(CHAT_TYPE_INFO, "[TestOnly]Pulse %d LoadTime %d PASS %d", iPulse, safebox_load_time(), passes_per_sec(limittime));
            return true;
        }

        if (check_shop_owner) {
            if (get_exchange() || my_shop() || shop_owner() || is_open_safebox() || is_cube_open()) {
                if (send_msg)
                    chat_packet(CHAT_TYPE_INFO, lc_text("거래창,창고 등을 연 상태에서는 다른곳으로 이동,종료 할수 없습니다"));

                return true;
            }
        } else {
            if (get_exchange() || my_shop() || is_open_safebox() || is_cube_open()) {
                if (send_msg)
                    chat_packet(CHAT_TYPE_INFO, lc_text("거래창,창고 등을 연 상태에서는 다른곳으로 이동,종료 할수 없습니다"));

                return true;
            }
        }

        if (iPulse - get_exchange_time() < passes_per_sec(limittime)) {
            if (send_msg)
                chat_packet(CHAT_TYPE_INFO, lc_text("거래 후 %d초 이내에는 다른지역으로 이동 할 수 없습니다."), limittime);
            return true;
        }

        if (iPulse - get_my_shop_time() < passes_per_sec(limittime)) {
            if (send_msg)
                chat_packet(CHAT_TYPE_INFO, lc_text("거래 후 %d초 이내에는 다른지역으로 이동 할 수 없습니다."), limittime);
            return true;
        }

        if (iPulse - get_refine_time() < passes_per_sec(limittime)) {
            if (send_msg)
                chat_packet(CHAT_TYPE_INFO, lc_text("아이템 개량후 %d초 이내에는 귀환부,귀환기억부를 사용할 수 없습니다."), limittime);
            return true;
        }

        return false;
    }

    auto character::is_monarch() const -> bool {
        if (monarch::instance().is_monarch(player_id(), empire()))
            return true;

        return false;
    }

    void character::init_mc() {
        for (int n = 0; n < MI_MAX; ++n) {
            monarch_cooltime[n] = thecore_pulse();
        }

        monarch_cooltimelimit[MI_HEAL] = passes_per_sec(MC_HEAL);
        monarch_cooltimelimit[MI_WARP] = passes_per_sec(MC_WARP);
        monarch_cooltimelimit[MI_TRANSFER] = passes_per_sec(MC_TRANSFER);
        monarch_cooltimelimit[MI_TAX] = passes_per_sec(MC_TAX);
        monarch_cooltimelimit[MI_SUMMON] = passes_per_sec(MC_SUMMON);

        monarch_cooltime[MI_HEAL] -= passes_per_sec(mcl(MI_HEAL));
        monarch_cooltime[MI_WARP] -= passes_per_sec(mcl(MI_WARP));
        monarch_cooltime[MI_TRANSFER] -= passes_per_sec(mcl(MI_TRANSFER));
        monarch_cooltime[MI_TAX] -= passes_per_sec(mcl(MI_TAX));
        monarch_cooltime[MI_SUMMON] -= passes_per_sec(mcl(MI_SUMMON));
    }

    auto character::mc(enum monarch_index e) const -> uint32_t {
        return monarch_cooltime[e];
    }

    void character::set_mc(enum monarch_index e) {
        monarch_cooltime[e] = thecore_pulse();
    }

    auto character::is_mcok(enum monarch_index e) const -> bool {
        int iPulse = thecore_pulse();

        if ((iPulse - mc(e)) < mcl(e)) {
            if (metin2::config::test_server())
                metin2::log::debug("Monarch cooldown: pulse {:d}, cooltime {:d}, limit {:d}", iPulse, mc(e), mcl(e));

            return false;
        }

        if (metin2::config::test_server())
            metin2::log::debug("Monarch cooldown: pulse {:d}, cooltime {:d}, limit {:d}", iPulse, mc(e), mcl(e));

        return true;
    }

    auto character::mcl(enum monarch_index e) const -> uint32_t {
        return monarch_cooltimelimit[e];
    }

    auto character::mcl_time(enum monarch_index e) const -> uint32_t {
        int iPulse = thecore_pulse();

        if (metin2::config::test_server())
            metin2::log::debug("Monarch cooldown: pulse {:d}, cooltime {:d}, limit {:d}", iPulse, mc(e), mcl(e));

        return (mcl(e)) / metin2::config::server().passes_per_sec - (iPulse - mc(e)) / metin2::config::server().passes_per_sec;
    }

    void character::update_deposit_pulse() {
        deposit_pulse_ = thecore_pulse() + passes_per_sec(60 * 5);
    }

    auto character::can_deposit() const -> bool {
        return (deposit_pulse_ == 0 || (deposit_pulse_ < thecore_pulse()));
    }

    auto get_sex(character *ch) -> sex {
        switch (ch->race_num()) {
        case MAIN_RACE_WARRIOR_M:
        case MAIN_RACE_SURA_M:
        case MAIN_RACE_ASSASSIN_M:
        case MAIN_RACE_SHAMAN_M:
            return SEX_MALE;

        case MAIN_RACE_ASSASSIN_W:
        case MAIN_RACE_SHAMAN_W:
        case MAIN_RACE_WARRIOR_W:
        case MAIN_RACE_SURA_W:
            return SEX_FEMALE;
        }

        return SEX_MALE;
    }

    auto character::hp_pct() const -> int {
        return (hp() * 100) / max_hp();
    }

    auto character::is_berserk() const -> bool {
        if (mob_inst != nullptr)
            return mob_inst->is_berserk;
        else
            return false;
    }

    void character::set_berserk(bool mode) {
        if (mob_inst != nullptr)
            mob_inst->is_berserk = mode;
    }

    auto character::is_god_speed() const -> bool {
        if (mob_inst != nullptr) {
            return mob_inst->is_god_speed;
        } else {
            return false;
        }
    }

    void character::set_god_speed(bool mode) {
        if (mob_inst != nullptr) {
            mob_inst->is_god_speed = mode;

            if (mode == true) {
                set_point(POINT_ATT_SPEED, 250);
            } else {
                set_point(POINT_ATT_SPEED, mob_data->table.s_attack_speed);
            }
        }
    }

    auto character::is_death_blow() const -> bool {
        if (prob(mob_data->table.death_blow_point)) {
            return true;
        } else {
            return false;
        }
    }

    auto character::has_reviver_in_party() const -> bool {
        party *party = get_party();

        if (party != nullptr) {
            if (party->member_count() == 1) return false;

            character *reviver = nullptr;
            bool has_reviver = false;

            party->for_each_member_ptr([&](character *ch) {
                if (ch->is_monster() != true) {
                    return;
                }

                if (ch->is_reviver() == true && reviver != ch && ch->is_dead() != true) {
                    if (prob(ch->get_mob_table().revive_point)) {
                        has_reviver = true;
                        reviver = ch;
                    }
                }
            });

            return has_reviver;
        }

        return false;
    }

    auto character::is_revive() const -> bool {
        if (mob_inst != nullptr) {
            return mob_inst->is_revive;
        }

        return false;
    }

    void character::set_revive(bool mode) {
        if (mob_inst != nullptr) {
            mob_inst->is_revive = mode;
        }
    }

    static auto is_speed_hack_player(character *ch) -> bool { return ch->speed_hack_count > metin2::config::game().anti_cheat.speed_hack_limit; }

    EVENTFUNC(check_speedhack_event) {
        char_event_info *info = dynamic_cast<char_event_info *>(event->info);
        if (info == nullptr) {
            metin2::log::error("Speed hack check event fired without event info");
            return 0;
        }

        character *ch = info->ch;

        if (nullptr == ch || ch->is_npc())
            return 0;

        if (is_speed_hack_player(ch)) {
            log_manager::instance().speed_hack_log(ch->player_id(), ch->x(), ch->y(), ch->speed_hack_count);
        }

        ch->speed_hack_count = 0;

        ch->reset_combo_hack_count();
        return passes_per_sec(60);
    }

    void character::start_check_speed_hack_event() {
        if (check_speed_hack_event)
            return;

        char_event_info *info = alloc_event_info<char_event_info>();

        info->ch = this;

        check_speed_hack_event = event_create(check_speedhack_event, info, passes_per_sec(60));
    }

    void character::go_home() {
        warp_set(empire_start_x(empire()), empire_start_y(empire()));
    }

    void character::send_guild_name(guild *guild_value) {
        if (nullptr == guild_value) return;

        desc *desc = get_desc();

        if (nullptr == desc) return;
        if (known_guild_.find(guild_value->id()) != known_guild_.end()) return;

        known_guild_.insert(guild_value->id());

        packet_gc_guild_name pack;
        memset(&pack, 0x00, sizeof(pack));

        pack.header = HEADER_GC_GUILD;
        pack.subheader = GUILD_SUBHEADER_GC_GUILD_NAME;
        pack.size = sizeof(packet_gc_guild_name);
        pack.guild_id = guild_value->id();
        memcpy(pack.guild_name, guild_value->name(), GUILD_NAME_MAX_LEN);

        desc->packet(&pack, sizeof(pack));
    }

    void character::send_guild_name(uint32_t guild_id) {
        send_guild_name(guild_manager::instance().find_guild(guild_id));
    }

    EVENTFUNC(destroy_when_idle_event) {
        char_event_info *info = dynamic_cast<char_event_info *>(event->info);
        if (info == nullptr) {
            metin2::log::error("Destroy when idle event fired without event info");
            return 0;
        }

        character *ch = info->ch;
        if (ch == nullptr) {
            return 0;
        }

        if (ch->victim()) {
            return passes_per_sec(300);
        }

        metin2::log::debug("Destroying idle {}", ch->name());

        ch->destroy_when_idle_event = nullptr;
        character_manager::instance().destroy_character(ch);
        return 0;
    }

    void character::start_destroy_when_idle_event() {
        if (destroy_when_idle_event)
            return;

        char_event_info *info = alloc_event_info<char_event_info>();

        info->ch = this;

        destroy_when_idle_event = event_create(game::characters::destroy_when_idle_event, info, passes_per_sec(300));
    }

    void character::set_combo_sequence(uint8_t seq) {
        combo_sequence_ = seq;
    }

    auto character::combo_sequence() const -> uint8_t {
        return combo_sequence_;
    }

    void character::set_last_combo_time(uint32_t time) {
        last_combo_time_ = time;
    }

    auto character::last_combo_time() const -> uint32_t {
        return last_combo_time_;
    }

    void character::set_valid_combo_interval(int interval) {
        valid_combo_interval_ = interval;
    }

    auto character::valid_combo_interval() const -> int {
        return valid_combo_interval_;
    }

    auto character::combo_index() const -> uint8_t {
        return combo_index_;
    }

    void character::increase_combo_hack_count(int k) {
        combo_hack_count_ += k;

        if (combo_hack_count_ >= 10) {
            if (get_desc())
                if (get_desc()->delayed_disconnect(number(2, 7))) {
                    metin2::log::warn("{} disconnected for combo hacking (count {:d})", name(), combo_hack_count_);
                    log_manager::instance().hack_log("Combo", this);
                }
        }
    }

    void character::reset_combo_hack_count() {
        combo_hack_count_ = 0;
    }

    void character::skip_combo_attack_by_time(int interval) {
        skip_combo_attack_by_time_ = get_dword_time() + interval;
    }

    auto character::get_skip_combo_attack_by_time() const -> uint32_t {
        return skip_combo_attack_by_time_;
    }

    void character::reset_chat_counter() {
        chat_counter_ = 0;
    }

    auto character::increase_chat_counter() -> uint8_t {
        return ++chat_counter_;
    }

    auto character::chat_counter() const -> uint8_t {
        return chat_counter_;
    }

    auto character::is_riding() const -> bool {
        return is_horse_riding() || get_mount_vnum();
    }

    auto character::can_warp() const -> bool {
        const int iPulse = thecore_pulse();
        const int limit_time = passes_per_sec(portal_limit_time);

        if ((iPulse - safebox_load_time()) < limit_time)
            return false;

        if ((iPulse - get_exchange_time()) < limit_time)
            return false;

        if ((iPulse - get_my_shop_time()) < limit_time)
            return false;

        if ((iPulse - get_refine_time()) < limit_time)
            return false;

        if (get_exchange() || my_shop() || shop_owner() || is_open_safebox() || is_cube_open())
            return false;

        return true;
    }

    auto character::next_exp() const -> uint32_t {
        if (PLAYER_EXP_TABLE_MAX < level())
            return 2500000000;
        else
            return exp_table[level()];
    }

    auto character::skill_power_by_level(int level, bool mob) const -> int {
        return table_by_skill::instance().skill_power_by_level_from_type(job(), skill_group(), std::clamp(level, 0, SKILL_MAX_LEVEL), mob);
    }

    void character::set_last_pm_pulse(void) {
        last_pm_pulse_ = thecore_pulse() + 25;
    }
}
