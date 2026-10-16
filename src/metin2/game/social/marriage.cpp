#include <metin2/config/config.h>
#include <metin2/game/social/marriage.h>

#include <metin2/game/app/config.h>
#include <metin2/game/app/event.h>
#include <metin2/game/app/main.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/protocol/p2p.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/social/wedding.h>
#include <metin2/game/world/sectree_manager.h>

#include <metin2/log/log.h>
#include <metin2/proto/character_types.h>
#include <metin2/proto/chat_types.h>
#include <metin2/proto/tables.h>

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <memory>
#include <string>
#include <utility>

namespace metin2::game::inline social {

    namespace marriage {
        const int MAX_LOVE_GRADE = 4;
        const int MAX_MARRIAGE_UNIQUE_ITEM = 6;

        struct marriage_item_bonus_by_grade {
            uint32_t vnum;
            int value[MAX_LOVE_GRADE];
        } item_bonus[MAX_MARRIAGE_UNIQUE_ITEM] = {
            {71069, {
                        4,
                        5,
                        6,
                        8,
                    }},
            {71070, {
                        10,
                        12,
                        15,
                        20,
                    }},
            {71071, {
                        4,
                        5,
                        6,
                        8,
                    }},
            {71072, {
                        -4,
                        -5,
                        -6,
                        -8,
                    }},
            {71073, {
                        20,
                        25,
                        30,
                        40,
                    }},
            {71074, {
                        12,
                        16,
                        20,
                        30,
                    }},
        };

        const int MARRIAGE_POINT_PER_DAY = 1;
        const int MARRIAGE_POINT_PER_DAY_FAST = 2;
        void send_lover_info(character *ch, const std::string &lover_name, int love_point) {
            packet_gc_lover_info p;

            p.header = HEADER_GC_LOVER_INFO;
            strlcpy(p.name, lover_name.c_str(), sizeof(p.name));
            p.love_point = love_point;
            ch->get_desc()->packet(&p, sizeof(p));
        }

        marriage::~marriage() {
            stop_near_check_event();
            if (is_online()) {
                ch1->chat_packet(CHAT_TYPE_COMMAND, "lover_divorce");
                ch2->chat_packet(CHAT_TYPE_COMMAND, "lover_divorce");
            }
        }

        auto marriage::marriage_grade() -> int {
            int point = std::clamp(marriage_point(), 50, 100);
            if (point < 65)
                return 0;
            else if (point < 80)
                return 1;
            else if (point < 100)
                return 2;
            return 3;
        }

        auto marriage::marriage_point() -> int {
            if (metin2::config::test_server()) {
                int value = quest::get_quest_manager().event_flag("lovepoint");
                if (value)
                    return std::clamp(value, 0, 100);
            }

            int point_per_day = MARRIAGE_POINT_PER_DAY;
            int max_limit = 30;
            if (is_online()) {
                if (ch1->premium_remain_seconds(PREMIUM_MARRIAGE_FAST) > 0 ||
                    ch2->premium_remain_seconds(PREMIUM_MARRIAGE_FAST) > 0) {
                    point_per_day = MARRIAGE_POINT_PER_DAY_FAST;
                    max_limit = 40;
                }
            }

            int days = (get_global_time() - marry_time);
            if (metin2::config::test_server())
                days /= 60;
            else
                days /= 86400;

            return std::min(50 + std::min(days * point_per_day, max_limit) + std::min(love_point / 1000000, max_limit), 100);
        }

        auto marriage::is_near() -> bool {
            if (!is_married)
                return false;
            if (!is_online())
                return false;

            return ch1->map_index() == ch2->map_index();
        }

        auto marriage::bonus(uint32_t item_vnum, bool share, character *me) -> int {
            if (!is_married)
                return 0;

            int finded_bonus_index = 0;
            {
                for (finded_bonus_index = 0; finded_bonus_index < MAX_MARRIAGE_UNIQUE_ITEM; ++finded_bonus_index) {
                    if (item_bonus[finded_bonus_index].vnum == item_vnum)
                        break;
                }

                if (finded_bonus_index == MAX_MARRIAGE_UNIQUE_ITEM)
                    return 0;
            }

            if (share) {
                int count = 0;
                if (nullptr != ch1 && ch1->is_equip_unique_item(item_vnum))
                    count++;
                if (nullptr != ch2 && ch2->is_equip_unique_item(item_vnum))
                    count++;

                const marriage_item_bonus_by_grade &bonus = item_bonus[finded_bonus_index];

                if (count >= 1)
                    return bonus.value[marriage_grade()];
                return 0;
            } else {
                int count = 0;
                if (me != ch1 && nullptr != ch1 && ch1->is_equip_unique_item(item_vnum))
                    count++;
                if (me != ch2 && nullptr != ch2 && ch2->is_equip_unique_item(item_vnum))
                    count++;

                const marriage_item_bonus_by_grade &bonus = item_bonus[finded_bonus_index];

                if (count >= 1)
                    return bonus.value[marriage_grade()];
                return 0;
            }
        }

        void marriage::login(character *ch) {
            if (ch->player_id() == pid1) {
                ch1 = ch;
                if (is_married)
                    send_lover_info(ch1, name2, marriage_point());
            } else if (ch->player_id() == pid2) {
                ch2 = ch;
                if (is_married)
                    send_lover_info(ch2, name1, marriage_point());
            }

            if (is_online()) {
                ch1->set_marry_partner(ch2);
                ch2->set_marry_partner(ch1);

                start_near_check_event();
            }

            if (is_married) {
                desc *d1;
                desc *d2;
                cci *cci;

                d1 = ch1 ? ch1->get_desc() : nullptr;

                if (!d1) {
                    cci = get_p2p_manager().find_by_pid(pid1);

                    if (cci) {
                        d1 = cci->desc;
                        d1->set_relay(cci->name);
                    }
                }

                d2 = ch2 ? ch2->get_desc() : nullptr;

                if (!d2) {
                    cci = get_p2p_manager().find_by_pid(pid2);

                    if (cci) {
                        d2 = cci->desc;
                        d2->set_relay(cci->name);
                    }
                }

                if (d1 && d2) {
                    d1->chat_packet(CHAT_TYPE_COMMAND, "lover_login");
                    d2->chat_packet(CHAT_TYPE_COMMAND, "lover_login");
                    metin2::log::debug("Marriage: partners {:d} and {:d} are both online", pid1, pid2);
                }
            }
        }

        void marriage::logout(uint32_t pid) {
            if (pid == pid1)
                ch1 = nullptr;
            else if (pid == pid2)
                ch2 = nullptr;

            if (ch1 || ch2) {
                save();

                if (ch1)
                    ch1->set_marry_partner(nullptr);

                if (ch2)
                    ch2->set_marry_partner(nullptr);

                stop_near_check_event();
            }

            if (is_married) {
                desc *d1;
                desc *d2;
                cci *cci;

                d1 = ch1 ? ch1->get_desc() : nullptr;

                if (!d1) {
                    cci = get_p2p_manager().find_by_pid(pid1);

                    if (cci) {
                        d1 = cci->desc;
                        d1->set_relay(cci->name);
                    }
                }

                if (d1 && !shutting_down) {
                    d1->chat_packet(CHAT_TYPE_COMMAND, "lover_logout");
                }

                d2 = ch2 ? ch2->get_desc() : nullptr;

                if (!d2) {
                    cci = get_p2p_manager().find_by_pid(pid2);

                    if (cci) {
                        d2 = cci->desc;
                        d2->set_relay(cci->name);
                    }
                }

                if (d2 && !shutting_down) {
                    d2->chat_packet(CHAT_TYPE_COMMAND, "lover_logout");
                }
            }
        }

        void marriage::near_check() {
            if (!is_married)
                return;

            if (!is_online()) {
                stop_near_check_event();
                return;
            }
            metin2::log::debug("Marriage {:d}-{:d}: near {:d}, was near {:d}, last love point {:d}, marriage point {:d}", pid1, pid2, is_near(), is_last_near, last_love_point, marriage_point());

            if (is_near() && !is_last_near) {
                is_last_near = true;
                ch1->chat_packet(CHAT_TYPE_COMMAND, "lover_near");
                ch2->chat_packet(CHAT_TYPE_COMMAND, "lover_near");
            } else if (!is_near() && is_last_near) {
                is_last_near = false;
                ch1->chat_packet(CHAT_TYPE_COMMAND, "lover_far");
                ch2->chat_packet(CHAT_TYPE_COMMAND, "lover_far");
            }

            if (last_love_point != marriage_point()) {
                last_love_point = marriage_point();
                packet_gc_love_point_update p;
                p.header = HEADER_GC_LOVE_POINT_UPDATE;
                p.love_point = last_love_point;

                ch1->get_desc()->packet(&p, sizeof(p));
                ch2->get_desc()->packet(&p, sizeof(p));
            }
        }

        EVENTINFO(near_check_event_info) {
            game::social::marriage::marriage *marriage;

            near_check_event_info()
                : marriage(nullptr) {
            }
        };

        EVENTFUNC(near_check_event) {
            auto *info = dynamic_cast<near_check_event_info *>(event->info.get());

            if (info == nullptr) {
                metin2::log::error("Marriage near check event fired without event info");
                return 0;
            }

            marriage *marriage = info->marriage;
            marriage->near_check();
            return passes_per_sec(5);
        }

        void marriage::start_near_check_event() {
            stop_near_check_event();

            auto *info = alloc_event_info<near_check_event_info>();
            info->marriage = this;
            event_near_check = event_create(near_check_event, info, 1);
        }

        void marriage::stop_near_check_event() {
            last_love_point = 0;
            is_last_near = false;
            event_cancel(&event_near_check);
        }

        void marriage::save() {
            metin2::log::debug("Marriage: save requested (needs save {:d})", needs_save);
            if (needs_save) {
                get_manager().request_update(pid1, pid2, love_point, is_married);
                needs_save = false;
            }
        }

        void marriage::set_married() {
            is_married = 1;
            needs_save = true;
            save();

            if (is_online()) {
                send_lover_info(ch1, name2, marriage_point());
                send_lover_info(ch2, name1, marriage_point());

                ch1->chat_packet(CHAT_TYPE_COMMAND, "lover_login");
                ch2->chat_packet(CHAT_TYPE_COMMAND, "lover_login");
            }
        }

        void marriage::update(uint32_t point) {
            if (!is_online())
                return;

            if (point > 0 && is_married) {
                needs_save = true;
                love_point += point;

                love_point = std::min(love_point, 2000000000);

                if (metin2::config::test_server()) {
                    character *ch;
                    ch = get_character_manager().find_by_pid(pid1);
                    if (ch)
                        ch->chat_packet(CHAT_TYPE_PARTY, "lovepoint bykill %.3g total %d", love_point / 1000000., marriage_point());
                    ch = get_character_manager().find_by_pid(pid2);
                    if (ch)
                        ch->chat_packet(CHAT_TYPE_PARTY, "lovepoint bykill %.3g total %d", love_point / 1000000., marriage_point());
                }
            }
        }

        void marriage::warp_to_wedding_map(uint32_t pid) {
            if (!wedding_info)
                return;

            character *ch = get_character_manager().find_by_pid(pid);
            if (ch) {
                pixel_position pos;
                if (!get_sectree_manager().recall_position_by_empire(wedding_info->map_index / 10000, 0, pos)) {
                    metin2::log::error("Cannot get wedding warp position");
                    return;
                }
                ch->save_exit_location();
                ch->warp_set(pos.x, pos.y, wedding_info->map_index);
            }
        }

        void marriage::request_end_wedding() {
            if (!wedding_info)
                return;
            get_manager().request_end_wedding(pid1, pid2);
        }

        manager::manager() = default;

        manager::~manager() = default;

        auto manager::is_marriage_unique_item(uint32_t item_vnum) -> bool {
            for (int i = 0; i < MAX_MARRIAGE_UNIQUE_ITEM; i++) {
                if (item_bonus[i].vnum == item_vnum)
                    return true;
            }
            return false;
        }

        auto manager::is_married(uint32_t player_id) -> bool {
            marriage *marriage_finded = get(player_id);
            if (marriage_finded && marriage_finded->is_married)
                return true;

            return false;
        }

        auto manager::is_engaged(uint32_t player_id) -> bool {
            marriage *marriage_finded = get(player_id);
            if (marriage_finded && !marriage_finded->is_married)
                return true;

            return false;
        }

        auto manager::is_engaged_or_married(uint32_t player_id) -> bool {
            return get(player_id) != nullptr;
        }

        void align(uint32_t &pid1, uint32_t &pid2) {
            if (pid1 > pid2)
                std::swap(pid1, pid2);
        }

        auto manager::get(uint32_t player_id) -> marriage * {
            auto it = marriage_by_pid_.find(player_id);

            if (it != marriage_by_pid_.end())
                return it->second;

            return nullptr;
        }

        void manager::request_add(uint32_t pid1, uint32_t pid2, const char *name1_value, const char *name2_value) {
            if (pid1 > pid2) {
                std::swap(pid1, pid2);
                std::swap(name1_value, name2_value);
            }

            packet_marriage_add p;

            p.pid1 = pid1;
            p.pid2 = pid2;
            strlcpy(p.name1, name1_value, sizeof(p.name1));
            strlcpy(p.name2, name2_value, sizeof(p.name2));
            db_clientdesc->db_packet(HEADER_GD_MARRIAGE_ADD, 0, &p, sizeof(p));
        }

        void manager::add(uint32_t pid1, uint32_t pid2, time_t t_marry_time, const char *name1, const char *name2) {
            if (is_engaged_or_married(pid1) || is_engaged_or_married(pid2)) {
                metin2::log::error("Cannot marry {:d} and {:d}: already married", pid1, pid2);
                return;
            }

            if (pid1 > pid2) {
                std::swap(pid1, pid2);
                std::swap(name1, name2);
            }

            auto new_marriage = std::make_unique<marriage>(pid1, pid2, 0, t_marry_time, name1, name2);
            marriage *marriage = new_marriage.get();
            marriages_.emplace(marriage, std::move(new_marriage));
            marriage_by_pid_.insert(std::make_pair(pid1, marriage));
            marriage_by_pid_.insert(std::make_pair(pid2, marriage));
            {
                character *a = get_character_manager().find_by_pid(pid1);
                character *b = get_character_manager().find_by_pid(pid2);

                if (a && b) {
                    packet_wedding_request p;
                    p.pid1 = pid1;
                    p.pid2 = pid2;
                    db_clientdesc->db_packet(HEADER_GD_WEDDING_REQUEST, 0, &p, sizeof(p));
                }
            }
        }

        void manager::request_update(uint32_t pid1, uint32_t pid2, int update_point, uint8_t married_value) {
            align(pid1, pid2);

            packet_marriage_update p;
            p.pid1 = pid1;
            p.pid2 = pid2;
            p.love_point = update_point;
            p.married = married_value;
            db_clientdesc->db_packet(HEADER_GD_MARRIAGE_UPDATE, 0, &p, sizeof(p));
        }

        void manager::update(uint32_t pid1, uint32_t pid2, int32_t total_point, uint8_t married) {
            marriage *marriage = get(pid1);

            if (!marriage || marriage->other(pid1) != pid2) {
                metin2::log::error("{:d} and {:d} are not married to each other", pid1, pid2);
                return;
            }

            marriage->love_point = total_point;
            marriage->is_married = married;
        }

        void manager::request_remove(uint32_t pid1, uint32_t pid2) {
            align(pid1, pid2);

            packet_marriage_remove p;
            p.pid1 = pid1;
            p.pid2 = pid2;
            db_clientdesc->db_packet(HEADER_GD_MARRIAGE_REMOVE, 0, &p, sizeof(p));
        }

        void manager::remove(uint32_t pid1, uint32_t pid2) {
            marriage *marriage = get(pid1);
            if (!marriage || marriage->other(pid1) != pid2) {
                metin2::log::error("{:d} and {:d} are not married to each other", pid1, pid2);
                return;
            }

            marriage_by_pid_.erase(pid1);
            marriage_by_pid_.erase(pid2);
            marriages_.erase(marriage);
        }

        void manager::login(character *ch) {
            uint32_t pid = ch->player_id();

            marriage *marriage = get(pid);
            if (!marriage)
                return;

            marriage->login(ch);
        }

        void manager::logout(uint32_t pid) {
            marriage *marriage = get(pid);

            if (!marriage)
                return;

            marriage->logout(pid);
        }

        void manager::logout(character *ch) {
            logout(ch->player_id());
        }

        void manager::wedding_ready(uint32_t pid1, uint32_t pid2, uint32_t map_index_value) {
            marriage *marriage = get(pid1);
            if (!marriage || marriage->other(pid1) != pid2) {
                metin2::log::error("{:d} and {:d} are not married to each other", pid1, pid2);
                return;
            }

            if (!marriage->wedding_info)
                marriage->wedding_info = std::make_unique<wedding_info>();

            marriage->wedding_info->map_index = map_index_value;
        }

        void manager::wedding_start(uint32_t pid1, uint32_t pid2) {
            marriage *marriage = get(pid1);
            if (!marriage || marriage->other(pid1) != pid2) {
                metin2::log::error("{:d} and {:d} are not married to each other", pid1, pid2);
                return;
            }

            wedding_info *pwi = marriage->wedding_info.get();

            if (!pwi)
                return;

            marriage->warp_to_wedding_map(pid1);
            marriage->warp_to_wedding_map(pid2);

            wedding_.insert(std::make_pair(pid1, pid2));
        }

        void manager::wedding_end(uint32_t pid1, uint32_t pid2) {
            marriage *marriage = get(pid1);
            if (!marriage || marriage->other(pid1) != pid2) {
                metin2::log::error("{:d} and {:d} are not married to each other", pid1, pid2);
                return;
            }

            if (!marriage->wedding_info) {
                metin2::log::error("Marriage of {:d} and {:d} has no wedding in progress", pid1, pid2);
                return;
            }

            if (map_allow_find(WEDDING_MAP_INDEX))
                if (!get_wedding_manager().end(marriage->wedding_info->map_index)) {
                    metin2::log::error("Cannot end wedding on map {:d}", marriage->wedding_info->map_index);
                    return;
                }

            marriage->wedding_info.reset();

            wedding_.erase(std::make_pair(pid1, pid2));
        }

        void manager::request_end_wedding(uint32_t pid1, uint32_t pid2) {
            packet_wedding_end p;
            p.pid1 = pid1;
            p.pid2 = pid2;

            db_clientdesc->db_packet(HEADER_GD_WEDDING_END, 0, &p, sizeof(p));
        }
    }
}
