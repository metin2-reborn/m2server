#include <metin2/config/config.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/constants.h>
#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/characters/affect.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/characters/mob_manager.h>
#include <metin2/game/combat/battle.h>
#include <metin2/game/combat/skill.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/unique_item.h>
#include <metin2/game/parties/party.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/quest/questpc.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/world/entity.h>
#include <metin2/game/world/sectree.h>
#include <metin2/game/world/vector.h>
#include <metin2/game/world/vid.h>

#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/character_types.h>
#include <metin2/proto/chat_types.h>
#include <metin2/proto/inventory.h>
#include <metin2/proto/item_types.h>
#include <metin2/proto/mob_types.h>
#include <metin2/proto/tables.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <format>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace metin2::game::inline characters {

    static const uint32_t sub_skill_vnums[] =
        {
            SKILL_LEADERSHIP,
            SKILL_COMBO,
            SKILL_MINING,
            SKILL_LANGUAGE1,
            SKILL_LANGUAGE2,
            SKILL_LANGUAGE3,
            SKILL_POLYMORPH,
            SKILL_HORSE,
            SKILL_HORSE_SUMMON,
            SKILL_HORSE_WILDATTACK,
            SKILL_HORSE_CHARGE,
            SKILL_HORSE_ESCAPE,
            SKILL_HORSE_WILDATTACK_RANGE,
            SKILL_ADD_HP,
            SKILL_RESIST_PENETRATE};

    auto character::skill_next_read_time(uint32_t vnum) const -> time_t {
        if (vnum >= SKILL_MAX_NUM) {
            metin2::log::error("Skill vnum {:d} out of range", vnum);
            return 0;
        }

        return skill_levels_ ? skill_levels_[vnum].t_next_read : 0;
    }

    void character::set_skill_next_read_time(uint32_t vnum, time_t time) {
        if (skill_levels_ && vnum < SKILL_MAX_NUM)
            skill_levels_[vnum].t_next_read = time;
    }

    auto skill_use_info::hit_once(uint32_t vnum) -> bool {
        if (!used)
            return false;

        metin2::log::debug("Hit once: next use {:d}, now {:d}, hit count {:d}, splash count {:d}", next_skill_usable_time, get_dword_time(), hit_count, splash_count);

        if (next_skill_usable_time && next_skill_usable_time < get_dword_time() && vnum != SKILL_MUYEONG && vnum != SKILL_HORSE_WILDATTACK) {
            metin2::log::debug("Hit once: cannot hit");

            return false;
        }

        if (hit_count == -1) {
            metin2::log::debug("Hit once: ok, next use {:d}, now {:d}, hit count {:d}", next_skill_usable_time, get_dword_time(), hit_count);
            return true;
        }

        if (hit_count) {
            metin2::log::debug("Hit once: ok, next use {:d}, now {:d}, hit count {:d}", next_skill_usable_time, get_dword_time(), hit_count);
            hit_count--;
            return true;
        }
        return false;
    }

    auto skill_use_info::use_skill(bool is_grand_master_value, uint32_t vid_value, uint32_t cooltime, int splashcount, int hitcount, int range_value) -> bool {
        this->is_grand_master = is_grand_master_value;
        uint32_t cur = get_dword_time();

        if (used && next_skill_usable_time > cur) {
            metin2::log::debug("Skill used during cooldown ({:d} ms left)", next_skill_usable_time - cur);
            hit_count = 0;
            return false;
        }

        used = true;

        if (cooltime)
            next_skill_usable_time = cur + cooltime;
        else
            next_skill_usable_time = 0;

        range = range_value;
        max_hit_count = hit_count = hitcount;

        if (metin2::config::test_server())
            metin2::log::debug("Use skill: next use {:d}, now {:d}, cooldown {:d}, hit count {:d}/{:d}", next_skill_usable_time, cur, cooltime, hit_count, max_hit_count);

        vid = vid_value;
        splash_count = splashcount;
        return true;
    }

    auto character::chain_lightning_max_count() const -> int {
        return chain_lightning_count_by_skill_level[std::min(SKILL_MAX_LEVEL, skill_level(SKILL_CHAIN))];
    }

    void character::set_affected_eunhyung() {
        affected_eunhyung_level_ = skill_power(SKILL_EUNHYUNG);
    }

    void character::set_skill_group(uint8_t skill_group_value) {
        if (skill_group_value > 2)
            return;

        if (level() < 5)
            return;

        points_.skill_group = skill_group_value;

        packet_gc_change_skill_group p;
        p.header = HEADER_GC_SKILL_GROUP;
        p.skill_group = points_.skill_group;

        get_desc()->packet(&p, sizeof(packet_gc_change_skill_group));
    }

    auto character::compute_cooltime(int time) -> int {
        return calculate_duration(point(POINT_CASTING_SPEED), time);
    }

    void character::skill_level_packet() {
        if (!get_desc())
            return;

        packet_gc_skill_level pack;

        pack.header = HEADER_GC_SKILL_LEVEL;
        std::memcpy(&pack.skills, skill_levels_.get(), sizeof(player_skill) * SKILL_MAX_NUM);
        get_desc()->packet(&pack, sizeof(packet_gc_skill_level));
    }

    void character::set_skill_level(uint32_t vnum, uint8_t lev) {
        if (nullptr == skill_levels_)
            return;

        if (vnum >= SKILL_MAX_NUM) {
            metin2::log::error("Skill vnum {:d} out of range", vnum);
            return;
        }

        skill_levels_[vnum].level = std::min<int>(40, lev);

        if (lev >= 40)
            skill_levels_[vnum].master_type = SKILL_PERFECT_MASTER;
        else if (lev >= 30)
            skill_levels_[vnum].master_type = SKILL_GRAND_MASTER;
        else if (lev >= 20)
            skill_levels_[vnum].master_type = SKILL_MASTER;
        else
            skill_levels_[vnum].master_type = SKILL_NORMAL;
    }

    auto character::is_learnable_skill(uint32_t skill_vnum) const -> bool {
        const skill_proto *skill = get_skill_manager().get(skill_vnum);

        if (!skill)
            return false;

        if (skill_level(skill_vnum) >= SKILL_MAX_LEVEL)
            return false;

        if (skill->type == 0) {
            if (skill_level(skill_vnum) >= skill->max_level)
                return false;

            return true;
        }

        if (skill->type == 5) {
            if (skill_vnum == SKILL_HORSE_WILDATTACK_RANGE && job() != JOB_ASSASSIN)
                return false;

            return true;
        }

        if (skill_group() == 0)
            return false;

        if (skill->type - 1 == job())
            return true;

        if (6 == skill->type) {
            if (SKILL_7_A_ANTI_TANHWAN <= skill_vnum && skill_vnum <= SKILL_7_D_ANTI_YONGBI) {
                for (int i = 0; i < 4; i++) {
                    if (unsigned(SKILL_7_A_ANTI_TANHWAN + i) != skill_vnum) {
                        if (0 != skill_level(SKILL_7_A_ANTI_TANHWAN + i)) {
                            return false;
                        }
                    }
                }

                return true;
            }

            if (SKILL_8_A_ANTI_GIGONGCHAM <= skill_vnum && skill_vnum <= SKILL_8_D_ANTI_BYEURAK) {
                for (int i = 0; i < 4; i++) {
                    if (unsigned(SKILL_8_A_ANTI_GIGONGCHAM + i) != skill_vnum) {
                        if (0 != skill_level(SKILL_8_A_ANTI_GIGONGCHAM + i))
                            return false;
                    }
                }

                return true;
            }
        }

        return false;
    }

    auto character::learn_grand_master_skill(uint32_t skill_vnum) -> bool {
        skill_proto *sk = get_skill_manager().get(skill_vnum);

        if (!sk)
            return false;

        if (!is_learnable_skill(skill_vnum)) {
            chat_packet(CHAT_TYPE_INFO, lc_text("수련할 수 없는 스킬입니다."));
            return false;
        }

        metin2::log::debug("Grand master training of skill {:d}: now {:d}, next read {:d}", skill_vnum, get_global_time(), skill_next_read_time(skill_vnum));

        if (sk->type == 0) {
            chat_packet(CHAT_TYPE_INFO, lc_text("그랜드 마스터 수련을 할 수 없는 스킬입니다."));
            return false;
        }

        if (skill_master_type(skill_vnum) != SKILL_GRAND_MASTER) {
            if (skill_master_type(skill_vnum) > SKILL_GRAND_MASTER)
                chat_packet(CHAT_TYPE_INFO, lc_text("퍼펙트 마스터된 스킬입니다. 더 이상 수련 할 수 없습니다."));
            else
                chat_packet(CHAT_TYPE_INFO, lc_text("이 스킬은 아직 그랜드 마스터 수련을 할 경지에 이르지 않았습니다."));
            return false;
        }

        std::string train_skill;
        {
            std::ostringstream os;
            os << "training_grandmaster_skill.skill" << skill_vnum;
            train_skill = os.str();
        }

        uint8_t last_level = skill_level(skill_vnum);

        int idx = std::min(9, skill_level(skill_vnum) - 30);

        metin2::log::debug("{}: grand master book table index {:d}, books needed {:d}", name(), idx, grand_master_skill_book_count_for_level_up[idx]);

        int total_read_count = quest_flag(train_skill) + 1;
        set_quest_flag(train_skill, total_read_count);

        int min_read_count = grand_master_skill_book_min_count[idx];
        int max_read_count = grand_master_skill_book_max_count[idx];

        int book_count = grand_master_skill_book_count_for_level_up[idx];

        if (find_affect(AFFECT_SKILL_BOOK_BONUS)) {
            if (book_count & 1)
                book_count = book_count / 2 + 1;
            else
                book_count = book_count / 2;

            remove_affect(AFFECT_SKILL_BOOK_BONUS);
        }

        int n = number(1, book_count);
        metin2::log::debug("Grand master book roll {:d}", n);

        uint32_t next_time = get_global_time() + number(28800, 43200);

        metin2::log::debug("Grand master books: min {:d}, read {:d}, max {:d} (next read {:d})", min_read_count, total_read_count, max_read_count, next_time);

        bool success = n == 2;

        if (total_read_count < min_read_count)
            success = false;
        if (total_read_count > max_read_count)
            success = true;

        if (success) {
            skill_level_up(skill_vnum, SKILL_UP_BY_QUEST);
        }

        set_skill_next_read_time(skill_vnum, next_time);

        if (last_level == skill_level(skill_vnum)) {
            chat_packet(CHAT_TYPE_TALKING, lc_text("크윽, 기가 역류하고 있어! 이거 설마 주화입마인가!? 젠장!"));
            chat_packet(CHAT_TYPE_INFO, lc_text("수련이 실패로 끝났습니다. 다시 도전해주시기 바랍니다."));
            get_log_manager().char_log(this, skill_vnum, "GM_READ_FAIL", "");
            return false;
        }

        chat_packet(CHAT_TYPE_TALKING, lc_text("몸에서 뭔가 힘이 터져 나오는 기분이야!"));
        chat_packet(CHAT_TYPE_TALKING, lc_text("뜨거운 무엇이 계속 용솟음치고 있어! 이건, 이것은!"));
        chat_packet(CHAT_TYPE_INFO, lc_text("더 높은 경지의 수련을 성공적으로 끝내셨습니다."));
        get_log_manager().char_log(this, skill_vnum, "GM_READ_SUCCESS", "");
        return true;
    }

    static auto fn_should_check_exp(character *ch) -> bool {

        return true;

        return false;
    }

    auto character::learn_skill_by_book(uint32_t skill_vnum, uint8_t prob) -> bool {
        const skill_proto *sk = get_skill_manager().get(skill_vnum);

        if (!sk)
            return false;

        if (!is_learnable_skill(skill_vnum)) {
            chat_packet(CHAT_TYPE_INFO, lc_text("수련할 수 없는 스킬입니다."));
            return false;
        }

        uint32_t need_exp = 0;

        if (fn_should_check_exp(this)) {
            need_exp = 20000;

            if (get_exp() < need_exp) {
                chat_packet(CHAT_TYPE_INFO, lc_text("경험치가 부족하여 책을 읽을 수 없습니다."));
                return false;
            }
        }

        if (sk->type != 0) {
            if (skill_master_type(skill_vnum) != SKILL_MASTER) {
                if (skill_master_type(skill_vnum) > SKILL_MASTER)
                    chat_packet(CHAT_TYPE_INFO, lc_text("이 스킬은 책으로 더이상 수련할 수 없습니다."));
                else
                    chat_packet(CHAT_TYPE_INFO, lc_text("이 스킬은 아직 책으로 수련할 경지에 이르지 않았습니다."));
                return false;
            }
        }

        if (get_global_time() < skill_next_read_time(skill_vnum)) {
            if (!(metin2::config::test_server() && quest::get_quest_manager().event_flag("no_read_delay"))) {
                if (find_affect(AFFECT_SKILL_NO_BOOK_DELAY)) {
                    remove_affect(AFFECT_SKILL_NO_BOOK_DELAY);
                    chat_packet(CHAT_TYPE_INFO, lc_text("주안술서를 통해 주화입마에서 빠져나왔습니다."));
                } else {
                    skill_learn_wait_more_time_message(skill_next_read_time(skill_vnum) - get_global_time());
                    return false;
                }
            }
        }

        uint8_t last_level = skill_level(skill_vnum);

        if (prob != 0) {
            if (find_affect(AFFECT_SKILL_BOOK_BONUS)) {
                prob += prob / 2;
                remove_affect(AFFECT_SKILL_BOOK_BONUS);
            }

            metin2::log::debug("Skill book for skill {:d}: chance {:d}%", skill_vnum, prob);

            if (core::prob(prob)) {
                if (metin2::config::test_server())
                    metin2::log::debug("Skill book for skill {:d} succeeded", skill_vnum);

                skill_level_up(skill_vnum, SKILL_UP_BY_BOOK);
            } else {
                if (metin2::config::test_server())
                    metin2::log::debug("Skill book for skill {:d} failed", skill_vnum);
            }
        } else {
            int idx = std::min(9, skill_level(skill_vnum) - 20);

            metin2::log::debug("{}: skill book table index {:d}, books needed {:d}", name(), idx, skill_book_count_for_level_up[idx]);

            int need_bookcount = skill_level(skill_vnum) - 20;

            point_change(POINT_EXP, -need_exp);

            quest::quest_manager &q = quest::get_quest_manager();
            quest::pc *pc = q.get_pc(player_id());

            if (pc) {
                const std::string flag = std::format("traning_master_skill.{}.read_count", skill_vnum);

                int read_count = pc->flag(flag);
                int percent = 65;

                if (find_affect(AFFECT_SKILL_BOOK_BONUS)) {
                    percent = 0;
                    remove_affect(AFFECT_SKILL_BOOK_BONUS);
                }

                if (number(1, 100) > percent) {
                    if (read_count >= need_bookcount) {
                        skill_level_up(skill_vnum, SKILL_UP_BY_BOOK);
                        pc->set_flag(flag, 0);

                        chat_packet(CHAT_TYPE_INFO, lc_text("책으로 더 높은 경지의 수련을 성공적으로 끝내셨습니다."));
                        get_log_manager().char_log(this, skill_vnum, "READ_SUCCESS", "");
                        return true;
                    } else {
                        pc->set_flag(flag, read_count + 1);

                        switch (number(1, 3)) {
                        case 1:
                            chat_packet(CHAT_TYPE_TALKING, lc_text("어느정도 이 기술에 대해 이해가 되었지만 조금 부족한듯 한데.."));
                            break;

                        case 2:
                            chat_packet(CHAT_TYPE_TALKING, lc_text("드디어 끝이 보이는 건가...  이 기술은 이해하기가 너무 힘들어.."));
                            break;

                        case 3:
                        default:
                            chat_packet(CHAT_TYPE_TALKING, lc_text("열심히 하는 배움을 가지는 것만이 기술을 배울수 있는 유일한 길이다.."));
                            break;
                        }

                        chat_packet(CHAT_TYPE_INFO, lc_text("%d 권을 더 읽어야 수련을 완료 할 수 있습니다."), need_bookcount - read_count);
                        return true;
                    }
                }
            } else {
            }
        }

        if (last_level != skill_level(skill_vnum)) {
            chat_packet(CHAT_TYPE_TALKING, lc_text("몸에서 뭔가 힘이 터져 나오는 기분이야!"));
            chat_packet(CHAT_TYPE_TALKING, lc_text("뜨거운 무엇이 계속 용솟음치고 있어! 이건, 이것은!"));
            chat_packet(CHAT_TYPE_INFO, lc_text("책으로 더 높은 경지의 수련을 성공적으로 끝내셨습니다."));
            get_log_manager().char_log(this, skill_vnum, "READ_SUCCESS", "");
        } else {
            chat_packet(CHAT_TYPE_TALKING, lc_text("크윽, 기가 역류하고 있어! 이거 설마 주화입마인가!? 젠장!"));
            chat_packet(CHAT_TYPE_INFO, lc_text("수련이 실패로 끝났습니다. 다시 도전해주시기 바랍니다."));
            get_log_manager().char_log(this, skill_vnum, "READ_FAIL", "");
        }

        return true;
    }

    auto character::skill_level_down(uint32_t vnum_value) -> bool {
        if (nullptr == skill_levels_)
            return false;

        if (metin2::config::game().skill_disable)
            return false;

        if (is_polymorphed())
            return false;

        skill_proto *sk = get_skill_manager().get(vnum_value);

        if (!sk) {
            metin2::log::error("No skill with vnum {:d}", vnum_value);
            return false;
        }

        if (!is_learnable_skill(vnum_value))
            return false;

        if (skill_master_type(sk->vnum) != SKILL_NORMAL)
            return false;

        if (!skill_group())
            return false;

        if (sk->vnum >= SKILL_MAX_NUM)
            return false;

        if (skill_levels_[sk->vnum].level == 0)
            return false;

        int idx = POINT_SKILL;
        switch (sk->type) {
        case 0:
            idx = POINT_SUB_SKILL;
            break;
        case 1:
        case 2:
        case 3:
        case 4:
        case 6:
            idx = POINT_SKILL;
            break;
        case 5:
            idx = POINT_HORSE_SKILL;
            break;
        default:
            metin2::log::error("Wrong skill type {:d} for skill {:d}", sk->type, sk->vnum);
            return false;
        }

        point_change(idx, +1);
        set_skill_level(sk->vnum, skill_levels_[sk->vnum].level - 1);

        metin2::log::info("{}: skill {:d} lowered (master type {:d}, level {:d}, type {:d})", name(), sk->vnum, skill_levels_[sk->vnum].master_type, skill_levels_[sk->vnum].level, sk->type);
        save();

        compute_points();
        skill_level_packet();
        return true;
    }

    void character::skill_level_up(uint32_t vnum_value, uint8_t method) {
        if (nullptr == skill_levels_)
            return;

        if (metin2::config::game().skill_disable)
            return;

        if (is_polymorphed()) {
            chat_packet(CHAT_TYPE_INFO, lc_text("둔갑 중에는 능력을 올릴 수 없습니다."));
            return;
        }

        if (SKILL_7_A_ANTI_TANHWAN <= vnum_value && vnum_value <= SKILL_8_D_ANTI_BYEURAK) {
            if (0 == skill_level(vnum_value))
                return;
        }

        const skill_proto *sk = get_skill_manager().get(vnum_value);

        if (!sk) {
            metin2::log::error("No skill with vnum {:d}", vnum_value);
            return;
        }

        if (sk->vnum >= SKILL_MAX_NUM) {
            metin2::log::error("Skill vnum {:d} out of range", vnum_value);
            return;
        }

        if (!is_learnable_skill(vnum_value))
            return;

        if (sk->type != 0) {
            switch (skill_master_type(sk->vnum)) {
            case SKILL_GRAND_MASTER:
                if (method != SKILL_UP_BY_QUEST)
                    return;
                break;

            case SKILL_PERFECT_MASTER:
                return;
            }
        }

        if (method == SKILL_UP_BY_POINT) {
            if (skill_master_type(sk->vnum) != SKILL_NORMAL)
                return;

            if (is_set(sk->flag, SKILL_FLAG_DISABLE_BY_POINT_UP))
                return;
        } else if (method == SKILL_UP_BY_BOOK) {
            if (sk->type != 0)
                if (skill_master_type(sk->vnum) != SKILL_MASTER)
                    return;
        }

        if (level() < sk->level_limit)
            return;

        if (sk->pre_skill_vnum)
            if (skill_master_type(sk->pre_skill_vnum) == SKILL_NORMAL &&
                skill_level(sk->pre_skill_vnum) < sk->pre_skill_level)
                return;

        if (!skill_group())
            return;

        if (method == SKILL_UP_BY_POINT) {
            int idx;

            switch (sk->type) {
            case 0:
                idx = POINT_SUB_SKILL;
                break;

            case 1:
            case 2:
            case 3:
            case 4:
            case 6:
                idx = POINT_SKILL;
                break;

            case 5:
                idx = POINT_HORSE_SKILL;
                break;

            default:
                metin2::log::error("Wrong skill type {:d} for skill {:d}", sk->type, sk->vnum);
                return;
            }

            if (point(idx) < 1)
                return;

            point_change(idx, -1);
        }

        int skill_point_before = skill_level(sk->vnum);
        set_skill_level(sk->vnum, skill_levels_[sk->vnum].level + 1);

        if (sk->type != 0) {
            switch (skill_master_type(sk->vnum)) {
            case SKILL_NORMAL:
                if (skill_level(sk->vnum) >= 17) {
                    if (quest_flag("reset_scroll.force_to_master_skill") > 0) {
                        set_skill_level(sk->vnum, 20);
                        set_quest_flag("reset_scroll.force_to_master_skill", 0);
                    } else {
                        if (number(1, 21 - std::min(20, skill_level(sk->vnum))) == 1)
                            set_skill_level(sk->vnum, 20);
                    }
                }
                break;

            case SKILL_MASTER:
                if (skill_level(sk->vnum) >= 30) {
                    if (number(1, 31 - std::min(30, skill_level(sk->vnum))) == 1)
                        set_skill_level(sk->vnum, 30);
                }
                break;

            case SKILL_GRAND_MASTER:
                if (skill_level(sk->vnum) >= 40) {
                    set_skill_level(sk->vnum, 40);
                }
                break;
            }
        }

        const std::string skill_up = std::format("SkillUp: {} {} {} {}[Before:{}] type {}",
            name(), sk->vnum, skill_levels_[sk->vnum].master_type, skill_levels_[sk->vnum].level, skill_point_before, sk->type);

        metin2::log::info("{}", skill_up);

        get_log_manager().char_log(this, sk->vnum, "SKILLUP", skill_up.c_str());
        save();

        compute_points();
        skill_level_packet();
    }

    void character::compute_skill_points() {
        if (metin2::config::game().skill_disable)
            return;
    }

    void character::reset_skill() {
        if (nullptr == skill_levels_)
            return;

        std::vector<std::pair<uint32_t, player_skill>> vec;
        size_t count = sizeof(sub_skill_vnums) / sizeof(sub_skill_vnums[0]);

        for (size_t i = 0; i < count; ++i) {
            if (sub_skill_vnums[i] >= SKILL_MAX_NUM)
                continue;

            vec.push_back(std::make_pair(sub_skill_vnums[i], skill_levels_[sub_skill_vnums[i]]));
        }

        memset(skill_levels_.get(), 0, sizeof(player_skill) * SKILL_MAX_NUM);

        auto iter = vec.begin();

        while (iter != vec.end()) {
            const std::pair<uint32_t, player_skill> &pair = *(iter++);
            skill_levels_[pair.first] = pair.second;
        }

        compute_points();
        skill_level_packet();
    }

    auto find_near_victim(character *center, character *attacker, const character_set &excepts_set = {}) -> character * {
        character *next_target = nullptr;
        int count = 0;

        center->get_sectree()->for_each_around([&](entity *ent) {
            if (!ent->is_type(ENTITY_CHARACTER))
                return;

            auto *chr = (character *)ent;

            if (!excepts_set.empty()) {
                if (excepts_set.find(chr) != excepts_set.end())
                    return;
            }

            if (center == chr)
                return;

            if (!battle_is_attackable(attacker, chr)) {
                return;
            }

            if (abs(center->x() - chr->x()) > 1000 || abs(center->y() - chr->y()) > 1000)
                return;

            float dist = distance_approx(center->x() - chr->x(), center->y() - chr->y());

            if (dist < 1000) {
                ++count;

                if ((count == 1) || number(1, count) == 1)
                    next_target = chr;
            }
        });

        return next_target;
    }

    EVENTINFO(chain_lightning_event_info) {
        uint32_t victim;
        uint32_t chr;

        chain_lightning_event_info()
            : victim(0), chr(0) {
        }
    };

    EVENTFUNC(chain_lightning_event) {
        auto *info = dynamic_cast<chain_lightning_event_info *>(event->info.get());

        if (info == nullptr) {
            metin2::log::error("Chain lightning event without event info");
            return 0;
        }

        character *chr_victim = get_character_manager().find(info->victim);
        character *chr = get_character_manager().find(info->chr);
        character *target = nullptr;

        if (!chr || !chr_victim) {
            metin2::log::debug("Chain lightning: caster not found");
            return 0;
        }

        metin2::log::debug("Chain lightning from {}", chr->name());

        if (chr_victim->get_party()) {
            target = chr_victim->get_party()->next_ownership(nullptr, chr_victim->x(), chr_victim->y());
            if (target == chr_victim || !number(0, 2) || chr->chain_lighting_except().find(target) != chr->chain_lighting_except().end())
                target = nullptr;
        }

        if (!target) {
            if (chr_victim->get_sectree())
                target = find_near_victim(chr_victim, chr, chr->chain_lighting_except());
        }

        if (target) {
            chr_victim->create_fly(FLY_CHAIN_LIGHTNING, target);
            chr->compute_skill(SKILL_CHAIN, target);
            chr->add_chain_lightning_except(target);
        } else {
            metin2::log::debug("{}: chain lightning found no victim near {}", chr->name(), chr_victim->name());
        }

        return 0;
    }

    void set_poly_var_for_attack(character *ch, skill_proto *sk, item *weapon) {
        if (ch->is_pc()) {
            if (weapon && weapon->type() == ITEM_WEAPON) {
                int wep_value = number(weapon->value(3), weapon->value(4));
                wep_value += weapon->value(5);

                int mtk_value = number(weapon->value(1), weapon->value(2));
                mtk_value += weapon->value(5);

                sk->set_point_var("wep", wep_value);
                sk->set_point_var("mtk", mtk_value);
                sk->set_point_var("mwep", mtk_value);
            } else {
                sk->set_point_var("wep", 0);
                sk->set_point_var("mtk", 0);
                sk->set_point_var("mwep", 0);
            }
        } else {
            int wep_value = number(ch->mob_damage_min(), ch->mob_damage_max());
            sk->set_point_var("wep", wep_value);
            sk->set_point_var("mwep", wep_value);
            sk->set_point_var("mtk", wep_value);
        }
    }

    void splash_damage(entity *ent, int x, int y, skill_proto *sk, character *chr, item *weapon, bool disable_cooltime, skill_use_info *info, uint8_t use_skill_power) {
        if (!ent->is_type(ENTITY_CHARACTER)) {
            return;
        }

        auto *chr_victim = (character *)ent;

        if (distance_approx(x - chr_victim->x(), y - chr_victim->y()) > sk->splash_range) {
            if (metin2::config::test_server())
                metin2::log::debug("Splash skill of {}: target too far", chr->name());
            return;
        }

        if (!battle_is_attackable(chr, chr_victim)) {
            if (metin2::config::test_server())
                metin2::log::debug("Splash skill of {}: target not attackable", chr->name());
            return;
        }

        if (chr->is_pc())
            if (!(sk->vnum >= GUILD_SKILL_START && sk->vnum <= GUILD_SKILL_END))
                if (!disable_cooltime && info && !info->hit_once(sk->vnum) && sk->vnum != SKILL_MUYEONG) {
                    if (metin2::config::test_server())
                        metin2::log::debug("Splash skill of {}: cannot hit again yet", chr->name());
                    return;
                }

        int dam;

        sk->set_point_var("k", 1.0 * use_skill_power * sk->max_level / 100);
        sk->set_point_var("lv", chr->level());
        sk->set_point_var("iq", chr->point(POINT_IQ));
        sk->set_point_var("str", chr->point(POINT_ST));
        sk->set_point_var("dex", chr->point(POINT_DX));
        sk->set_point_var("con", chr->point(POINT_HT));
        sk->set_point_var("def", chr->point(POINT_DEF_GRADE));
        sk->set_point_var("odef", chr->point(POINT_DEF_GRADE) - chr->point(POINT_DEF_GRADE_BONUS));
        sk->set_point_var("horse_level", chr->horse_level());

        bool ignore_defense = false;

        if (is_set(sk->flag, SKILL_FLAG_PENETRATE)) {
            int penetrate_pct = (int)sk->point_poly2.eval();

            if (prob(penetrate_pct))
                ignore_defense = true;
        }

        bool ignore_target_rating = false;

        if (is_set(sk->flag, SKILL_FLAG_IGNORE_TARGET_RATING)) {
            int pct = (int)sk->point_poly2.eval();

            if (prob(pct))
                ignore_target_rating = true;
        }

        sk->set_point_var("ar", calc_attack_rating(chr, chr_victim, ignore_target_rating));

        if (is_set(sk->flag, SKILL_FLAG_USE_MELEE_DAMAGE))
            sk->set_point_var("atk", calc_melee_damage(chr, chr_victim, true, ignore_target_rating));
        else if (is_set(sk->flag, SKILL_FLAG_USE_ARROW_DAMAGE)) {
            item *bow;
            item *arrow;

            if (1 == chr->arrow_and_bow(&bow, &arrow, 1))
                sk->set_point_var("atk", calc_arrow_damage(chr, chr_victim, bow, arrow, true));
            else
                sk->set_point_var("atk", 0);
        }

        if (sk->point_on == POINT_MOV_SPEED)
            sk->point_poly.set_var("maxv", chr_victim->limit_point(POINT_MOV_SPEED));

        sk->set_point_var("maxhp", chr_victim->max_hp());
        sk->set_point_var("maxsp", chr_victim->max_sp());

        sk->set_point_var("chain", chr->chain_lightning_index());
        chr->inc_chain_lightning_index();

        bool under_eunhyung = chr->affected_eunhyung() > 0;

        sk->set_point_var("ek", chr->affected_eunhyung() * 1. / 100);
        set_poly_var_for_attack(chr, sk, weapon);

        int amount = 0;

        if (chr->used_skill_master_type(sk->vnum) >= SKILL_GRAND_MASTER) {
            amount = (int)sk->master_bonus_poly.eval();
        } else {
            amount = (int)sk->point_poly.eval();
        }

        if (metin2::config::test_server() && amount == 0 && sk->point_on != POINT_NONE) {
            chr->chat_packet(CHAT_TYPE_INFO, "효과가 없습니다. 스킬 공식을 확인하세요");
        }
        amount = -amount;

        if (sk->vnum == SKILL_AMSEOP) {
            float delta = get_degree_delta(chr->rotation(), chr_victim->rotation());
            float adjust;

            if (delta < 35.0f) {
                adjust = 1.5f;

                if (under_eunhyung)
                    adjust += 0.5f;

                if (chr->wear(WEAR_WEAPON) && chr->wear(WEAR_WEAPON)->sub_type() == WEAPON_DAGGER) {
                    adjust += 0.5f;
                }
            } else {
                adjust = 1.0f;

                if (under_eunhyung)
                    adjust += 0.5f;

                if (chr->wear(WEAR_WEAPON) && chr->wear(WEAR_WEAPON)->sub_type() == WEAPON_DAGGER)
                    adjust += 0.5f;
            }

            amount = (int)(amount * adjust);
        } else if (sk->vnum == SKILL_GUNGSIN) {
            float adjust = 1.0;

            if (chr->wear(WEAR_WEAPON) && chr->wear(WEAR_WEAPON)->sub_type() == WEAPON_DAGGER) {
                adjust = 1.35f;
            }

            amount = (int)(amount * adjust);
        }

        dam = calc_battle_damage(amount, chr->level(), chr_victim->level());

        if (chr->is_pc() && chr->skill_use_info_[sk->vnum].main_target_vid() != (uint32_t)chr_victim->get_vid()) {
            dam = (int)(dam * sk->splash_around_damage_adjust_poly.eval());
        }

        damage_type dt = DAMAGE_TYPE_NONE;

        switch (sk->skill_attr_type) {
        case SKILL_ATTR_TYPE_NORMAL:
            break;

        case SKILL_ATTR_TYPE_MELEE:
            {
                dt = DAMAGE_TYPE_MELEE;

                item *weapon = chr->wear(WEAR_WEAPON);

                if (weapon)
                    switch (weapon->sub_type()) {
                    case WEAPON_SWORD:
                        dam = dam * (100 - chr_victim->point(POINT_RESIST_SWORD)) / 100;
                        break;

                    case WEAPON_TWO_HANDED:
                        dam = dam * (100 - chr_victim->point(POINT_RESIST_TWOHAND)) / 100;

                        break;

                    case WEAPON_DAGGER:
                        dam = dam * (100 - chr_victim->point(POINT_RESIST_DAGGER)) / 100;
                        break;

                    case WEAPON_BELL:
                        dam = dam * (100 - chr_victim->point(POINT_RESIST_BELL)) / 100;
                        break;

                    case WEAPON_FAN:
                        dam = dam * (100 - chr_victim->point(POINT_RESIST_FAN)) / 100;
                        break;
                    }

                if (!ignore_defense)
                    dam -= chr_victim->point(POINT_DEF_GRADE);
            }
            break;

        case SKILL_ATTR_TYPE_RANGE:
            dt = DAMAGE_TYPE_RANGE;
            dam = dam * (100 - chr_victim->point(POINT_RESIST_BOW)) / 100;
            break;

        case SKILL_ATTR_TYPE_MAGIC:
            dt = DAMAGE_TYPE_MAGIC;
            dam = calc_att_bonus(chr, chr_victim, dam);
            dam = dam * (100 - chr_victim->point(POINT_RESIST_MAGIC)) / 100;
            break;

        default:
            metin2::log::error("Unknown skill attr type {:d} (skill {:d})", sk->skill_attr_type, sk->vnum);
            break;
        }

        if (chr_victim->is_npc()) {
            if (is_set(sk->flag, SKILL_FLAG_WIND)) {
                dam = dam * (100 - chr_victim->point(POINT_RESIST_WIND)) / 100;
            }

            if (is_set(sk->flag, SKILL_FLAG_ELEC)) {
                dam = dam * (100 - chr_victim->point(POINT_RESIST_ELEC)) / 100;
            }

            if (is_set(sk->flag, SKILL_FLAG_FIRE)) {
                dam = dam * (100 - chr_victim->point(POINT_RESIST_FIRE)) / 100;
            }
        }

        if (is_set(sk->flag, SKILL_FLAG_COMPUTE_MAGIC_DAMAGE))
            dt = DAMAGE_TYPE_MAGIC;

        if (chr_victim->can_begin_fight())
            chr_victim->begin_fight(chr);

        if (sk->vnum == SKILL_CHAIN)
            metin2::log::debug("{}: chain lightning index {:d}, damage {:d}, damage type {:d}", chr->name(), chr->chain_lightning_index() - 1, dam, dt);

        {
            uint8_t anti_skill_id = 0;

            switch (sk->vnum) {
            case SKILL_TANHWAN:    anti_skill_id = SKILL_7_A_ANTI_TANHWAN; break;
            case SKILL_AMSEOP:     anti_skill_id = SKILL_7_B_ANTI_AMSEOP; break;
            case SKILL_SWAERYUNG:  anti_skill_id = SKILL_7_C_ANTI_SWAERYUNG; break;
            case SKILL_YONGBI:     anti_skill_id = SKILL_7_D_ANTI_YONGBI; break;
            case SKILL_GIGONGCHAM: anti_skill_id = SKILL_8_A_ANTI_GIGONGCHAM; break;
            case SKILL_YEONSA:     anti_skill_id = SKILL_8_B_ANTI_YEONSA; break;
            case SKILL_MAHWAN:     anti_skill_id = SKILL_8_C_ANTI_MAHWAN; break;
            case SKILL_BYEURAK:    anti_skill_id = SKILL_8_D_ANTI_BYEURAK; break;
            }

            if (0 != anti_skill_id) {
                uint8_t anti_skill_level = chr_victim->skill_level(anti_skill_id);

                if (0 != anti_skill_level) {
                    skill_proto *sk_value = get_skill_manager().get(anti_skill_id);
                    if (!sk_value) {
                        metin2::log::error("Anti skill {:d} is not in the skill proto", anti_skill_id);
                    } else {
                        sk_value->set_point_var("k", 1.0f * chr_victim->skill_power(anti_skill_id) * sk_value->max_level / 100);

                        double resist_amount = sk_value->point_poly.eval();

                        metin2::log::debug("Anti skill: resist {:f}%, damage {:d} reduced by {:d}", resist_amount, dam, int(dam * (resist_amount / 100.0)));

                        dam -= dam * (resist_amount / 100.0);
                    }
                }
            }
        }

        if (!chr_victim->damage(chr, dam, dt) && !chr_victim->is_stun()) {
            if (is_set(sk->flag, SKILL_FLAG_REMOVE_GOOD_AFFECT)) {
                int amount2 = (int)sk->point_poly2.eval();
                int dur2 = (int)sk->duration_poly2.eval();
                dur2 += chr->point(POINT_PARTY_BUFFER_BONUS);

                if (prob(amount2)) {
                    chr_victim->remove_good_affect();
                    chr_victim->add_affect(sk->vnum, POINT_NONE, 0, AFF_PABEOP, dur2, 0, true);
                }
            }

            if (is_set(sk->flag, SKILL_FLAG_SLOW | SKILL_FLAG_STUN | SKILL_FLAG_FIRE_CONT | SKILL_FLAG_POISON)) {
                int pct = (int)sk->point_poly2.eval();
                int dur = (int)sk->duration_poly2.eval();

                dur += chr->point(POINT_PARTY_BUFFER_BONUS);

                if (is_set(sk->flag, SKILL_FLAG_STUN)) {
                    skill_attack_affect(chr_victim, pct, IMMUNE_STUN, AFFECT_STUN, POINT_NONE, 0, AFF_STUN, dur, sk->name);
                } else if (is_set(sk->flag, SKILL_FLAG_SLOW)) {
                    skill_attack_affect(chr_victim, pct, IMMUNE_SLOW, AFFECT_SLOW, POINT_MOV_SPEED, -30, AFF_SLOW, dur, sk->name);
                } else if (is_set(sk->flag, SKILL_FLAG_FIRE_CONT)) {
                    sk->set_duration_var("k", 1.0 * use_skill_power * sk->max_level / 100);
                    sk->set_duration_var("iq", chr->point(POINT_IQ));

                    dur = (int)sk->duration_poly2.eval();
                    int bonus = chr->point(POINT_PARTY_BUFFER_BONUS);

                    if (bonus != 0) {
                        dur += bonus / 2;
                    }

                    if (prob(dur)) {
                        chr_victim->attacked_by_fire(chr, pct, 5);
                    }
                } else if (is_set(sk->flag, SKILL_FLAG_POISON)) {
                    if (prob(pct))
                        chr_victim->attacked_by_poison(chr);
                }
            }

            if (is_set(sk->flag, SKILL_FLAG_CRUSH | SKILL_FLAG_CRUSH_LONG) &&
                !is_set(chr_victim->ai_flag(), AIFLAG_NOMOVE)) {
                float crush_sliding_length = 200;

                if (chr->is_npc())
                    crush_sliding_length = 400;

                if (is_set(sk->flag, SKILL_FLAG_CRUSH_LONG))
                    crush_sliding_length *= 2;

                float fx, fy;
                float degree = get_degree_from_position_xy(chr->x(), chr->y(), chr_victim->x(), chr_victim->y());

                if (sk->vnum == SKILL_HORSE_WILDATTACK) {
                    degree -= chr->rotation();
                    degree = fmod(degree, 360.0f) - 180.0f;

                    if (degree > 0)
                        degree = chr->rotation() + 90.0f;
                    else
                        degree = chr->rotation() - 90.0f;
                }

                get_delta_by_degree(degree, crush_sliding_length, &fx, &fy);
                metin2::log::debug("{} pushes {} from {:d} {:d} to {:d} {:d}", chr->name(), chr_victim->name(), chr_victim->x(), chr_victim->y(), (int32_t)(chr_victim->x() + fx), (int32_t)(chr_victim->y() + fy));
                auto tx = (int32_t)(chr_victim->x() + fx);
                auto ty = (int32_t)(chr_victim->y() + fy);

                chr_victim->sync(tx, ty);
                chr_victim->go_to(tx, ty);
                chr_victim->calculate_move_duration();

                if (chr->is_pc() && chr->skill_use_info_[sk->vnum].main_target_vid() == (uint32_t)chr_victim->get_vid()) {
                    skill_attack_affect(chr_victim, 1000, IMMUNE_STUN, sk->vnum, POINT_NONE, 0, AFF_STUN, 4, sk->name);
                } else {
                    chr_victim->sync_packet();
                }
            }
        }

        if (is_set(sk->flag, SKILL_FLAG_HP_ABSORB)) {
            int pct = (int)sk->point_poly2.eval();
            chr->point_change(POINT_HP, dam * pct / 100);
        }

        if (is_set(sk->flag, SKILL_FLAG_SP_ABSORB)) {
            int pct = (int)sk->point_poly2.eval();
            chr->point_change(POINT_SP, dam * pct / 100);
        }

        if (sk->vnum == SKILL_CHAIN && chr->chain_lightning_index() < chr->chain_lightning_max_count()) {
            auto *info_value = alloc_event_info<chain_lightning_event_info>();

            info_value->victim = chr_victim->get_vid();
            info_value->chr = chr->get_vid();

            event_create(chain_lightning_event, info_value, metin2::config::server().passes_per_sec / 5);
        }
        if (metin2::config::test_server())
            metin2::log::debug("Splash damage done for {}", chr->name());
    }

    void splash_affect(sectree *tree, character *ch, int x, int y, int dist, uint32_t vnum, uint8_t point_on, int amount, uint32_t affect_flag, int duration, int sp_cost, bool override, int max_hit) {
        int count = 0;

        tree->for_each_around([&](entity *ent) {
            if (max_hit && max_hit <= count)
                return;

            if (ent->is_type(ENTITY_CHARACTER)) {
                auto *chr = (character *)ent;

                if (metin2::config::test_server())
                    metin2::log::debug("Splash affect candidate {}: skill {:d}, duration {:d}", chr->name(), vnum, duration);
                if (distance_approx(x - chr->x(), y - chr->y()) < dist) {
                    if (metin2::config::test_server())
                        metin2::log::debug("Splash affect in range {}: skill {:d}, duration {:d}", chr->name(), vnum, duration);
                    if (vnum == SKILL_TUSOK)
                        if (chr->can_begin_fight())
                            chr->begin_fight(ch);

                    if (chr->is_pc() && vnum == SKILL_TUSOK)
                        chr->add_affect(vnum, point_on, amount, affect_flag, duration / 3, sp_cost, override);
                    else
                        chr->add_affect(vnum, point_on, amount, affect_flag, duration, sp_cost, override);

                    count++;
                }
            }
        });
    }

    EVENTINFO(skill_gwihwan_info) {
        uint32_t pid;
        uint8_t bsklv;
    };

    namespace {
        struct skill_vars {
            float k;
            item *weapon;
        };

        auto set_skill_vars(character *ch, character *victim, skill_proto *sk, uint8_t skill_level, bool horse_damage) -> skill_vars {
            const float k = 1.0 * ch->skill_power(sk->vnum, skill_level) * sk->max_level / 100;

            sk->set_point_var("k", k);
            sk->splash_around_damage_adjust_poly.set_var("k", k);

            if (horse_damage) {
                item *bow;
                item *arrow;
                if (1 == ch->arrow_and_bow(&bow, &arrow, 1)) {
                    sk->set_point_var("atk", calc_arrow_damage(ch, victim, bow, arrow, true));
                } else {
                    sk->set_point_var("atk", calc_melee_damage(ch, victim, true, false));
                }
            } else if (is_set(sk->flag, SKILL_FLAG_USE_MELEE_DAMAGE)) {
                sk->set_point_var("atk", calc_melee_damage(ch, victim, true, false));
            } else if (is_set(sk->flag, SKILL_FLAG_USE_MAGIC_DAMAGE)) {
                sk->set_point_var("atk", calc_magic_damage(ch, victim));
            } else if (is_set(sk->flag, SKILL_FLAG_USE_ARROW_DAMAGE)) {
                item *bow;
                item *arrow;
                if (1 == ch->arrow_and_bow(&bow, &arrow, 1)) {
                    sk->set_point_var("atk", calc_arrow_damage(ch, victim, bow, arrow, true));
                } else {
                    sk->set_point_var("atk", 0);
                }
            }

            if (sk->point_on == POINT_MOV_SPEED) {
                sk->set_point_var("maxv", victim->limit_point(POINT_MOV_SPEED));
            }

            sk->set_point_var("lv", ch->level());
            sk->set_point_var("iq", ch->point(POINT_IQ));
            sk->set_point_var("str", ch->point(POINT_ST));
            sk->set_point_var("dex", ch->point(POINT_DX));
            sk->set_point_var("con", ch->point(POINT_HT));
            sk->set_point_var("maxhp", victim->max_hp());
            sk->set_point_var("maxsp", victim->max_sp());
            sk->set_point_var("chain", 0);
            sk->set_point_var("ar", calc_attack_rating(ch, victim));
            sk->set_point_var("def", ch->point(POINT_DEF_GRADE));
            sk->set_point_var("odef", ch->point(POINT_DEF_GRADE) - ch->point(POINT_DEF_GRADE_BONUS));
            sk->set_point_var("horse_level", ch->horse_level());

            if (sk->skill_attr_type != SKILL_ATTR_TYPE_NORMAL)
                ch->on_move(true);

            item *weapon = ch->wear(WEAR_WEAPON);

            set_poly_var_for_attack(ch, sk, weapon);

            return {k, weapon};
        }

        struct skill_amounts {
            int first;
            int second;
            int third;
        };

        auto eval_skill_amounts(character *ch, skill_proto *sk, float k, uint8_t skill_level, bool log_amounts) -> skill_amounts {
            skill_amounts amounts{(int)sk->point_poly.eval(), (int)sk->point_poly2.eval(), (int)sk->point_poly3.eval()};

            if (log_amounts)
                metin2::log::debug("Skill amounts {:d} {:d} {:d}: atk {:f}, skill level {:f}, k {:f}, power {:d}, max level {:d}, per {:d}",
                    amounts.first, amounts.second, amounts.third,
                    sk->point_poly.var("atk"),
                    sk->point_poly.var("k"),
                    k,
                    ch->skill_power(sk->vnum, skill_level),
                    sk->max_level,
                    sk->max_level / 100);

            if (ch->used_skill_master_type(sk->vnum) >= SKILL_GRAND_MASTER) {
                amounts.first = (int)sk->master_bonus_poly.eval();
            }

            if (metin2::config::test_server() && amounts.first == 0 && sk->point_on != POINT_NONE) {
                ch->chat_packet(CHAT_TYPE_INFO, "효과가 없습니다. 스킬 공식을 확인하세요");
            }

            return amounts;
        }

        void add_affect_or_splash(character *ch, character *target, int x, int y, skill_proto *sk, uint8_t point_on, int amount, uint32_t affect_flag, int duration, bool override) {
            if (!is_set(sk->flag, SKILL_FLAG_SPLASH))
                target->add_affect(sk->vnum, point_on, amount, affect_flag, duration, 0, override);
            else if (target->get_sectree())
                splash_affect(target->get_sectree(), ch, x, y, sk->splash_range, sk->vnum, point_on, amount, affect_flag, duration, 0, override, sk->max_hit);
        }
    }

    auto character::compute_skill_at_position(uint32_t vnum_value, const pixel_position &pos_target, uint8_t skill_level_value) -> int {
        if (get_mount_vnum())
            return BATTLE_NONE;

        if (is_polymorphed())
            return BATTLE_NONE;

        if (metin2::config::game().skill_disable)
            return BATTLE_NONE;

        skill_proto *sk = get_skill_manager().get(vnum_value);

        if (!sk)
            return BATTLE_NONE;

        if (metin2::config::test_server()) {
            metin2::log::debug("{}: skill {:d} at position {:d} {:d} (level {:d})",
                name(), vnum_value, pos_target.x, pos_target.y, skill_level_value);
        }

        if (!is_set(sk->flag, SKILL_FLAG_SPLASH))
            return BATTLE_NONE;

        if (0 == skill_level_value) {
            if ((skill_level_value = skill_level(sk->vnum)) == 0) {
                return BATTLE_NONE;
            }
        }

        const auto [k, weapon] = set_skill_vars(this, this, sk, skill_level_value, false);

        sk->set_duration_var("k", k);

        const auto [amount, amount2, amount3] = eval_skill_amounts(this, sk, k, skill_level_value, false);

        if (is_set(sk->flag, SKILL_FLAG_REMOVE_BAD_AFFECT)) {
            if (prob(amount2)) {
                remove_bad_affect();
            }
        }

        if (is_set(sk->flag, SKILL_FLAG_ATTACK | SKILL_FLAG_USE_MELEE_DAMAGE | SKILL_FLAG_USE_MAGIC_DAMAGE)) {
            bool added = false;

            if (sk->point_on == POINT_HP && amount < 0) {
                auto f = [&, x = pos_target.x, y = pos_target.y, disable_cooltime = disable_cooltime_, info = is_pc() ? &skill_use_info_[vnum_value] : nullptr, use_skill_power = skill_power(vnum_value, skill_level_value)](entity *ent) {
                    splash_damage(ent, x, y, sk, this, weapon, disable_cooltime, info, use_skill_power);
                };

                if (is_set(sk->flag, SKILL_FLAG_SPLASH)) {
                    if (get_sectree())
                        get_sectree()->for_each_around(f);
                } else {
                    f(this);
                }
            } else {
                int dur = (int)sk->duration_poly.eval();

                if (is_pc())
                    if (!(vnum_value >= GUILD_SKILL_START && vnum_value <= GUILD_SKILL_END))
                        if (!disable_cooltime_ && !skill_use_info_[vnum_value].hit_once(vnum_value) && vnum_value != SKILL_MUYEONG) {
                            return BATTLE_NONE;
                        }

                if (dur > 0) {
                    dur += point(POINT_PARTY_BUFFER_BONUS);

                    add_affect_or_splash(this, this, pos_target.x, pos_target.y, sk, sk->point_on, amount, sk->affect_flag, dur, true);
                    added = true;
                }
            }

            if (sk->point_on2 != POINT_NONE) {
                int dur = (int)sk->duration_poly2.eval();

                metin2::log::debug("Skill {:d}: second effect on point {:d}, duration {:d}", sk->vnum, sk->point_on2, dur);

                if (dur > 0) {
                    dur += point(POINT_PARTY_BUFFER_BONUS);

                    add_affect_or_splash(this, this, pos_target.x, pos_target.y, sk, sk->point_on2, amount2, sk->affect_flag2, dur, !added);
                    added = true;
                } else {
                    point_change(sk->point_on2, amount2);
                }
            }

            if (used_skill_master_type(sk->vnum) >= SKILL_GRAND_MASTER && sk->point_on3 != POINT_NONE) {
                int dur = (int)sk->duration_poly3.eval();

                if (dur > 0) {
                    dur += point(POINT_PARTY_BUFFER_BONUS);

                    add_affect_or_splash(this, this, pos_target.x, pos_target.y, sk, sk->point_on3, amount3, 0, dur, !added);
                } else {
                    point_change(sk->point_on3, amount3);
                }
            }

            return BATTLE_DAMAGE;
        } else {
            bool added = false;
            int dur = (int)sk->duration_poly.eval();

            if (dur > 0) {
                dur += point(POINT_PARTY_BUFFER_BONUS);
                sk->duration_sp_cost_poly.set_var("k", k);

                add_affect(sk->vnum,
                    sk->point_on,
                    amount,
                    sk->affect_flag,
                    dur,
                    (int32_t)sk->duration_sp_cost_poly.eval(),
                    !added);

                added = true;
            } else {
                point_change(sk->point_on, amount);
            }

            if (sk->point_on2 != POINT_NONE) {
                int dur = (int)sk->duration_poly2.eval();

                if (dur > 0) {
                    dur += point(POINT_PARTY_BUFFER_BONUS);
                    add_affect(sk->vnum, sk->point_on2, amount2, sk->affect_flag2, dur, 0, !added);
                    added = true;
                } else {
                    point_change(sk->point_on2, amount2);
                }
            }

            if (used_skill_master_type(sk->vnum) >= SKILL_GRAND_MASTER && sk->point_on3 != POINT_NONE) {
                int dur = (int)sk->duration_poly3.eval();

                if (dur > 0) {
                    dur += point(POINT_PARTY_BUFFER_BONUS);
                    add_affect(sk->vnum, sk->point_on3, amount3, 0, dur, 0, !added);
                } else {
                    point_change(sk->point_on3, amount3);
                }
            }

            return BATTLE_NONE;
        }
    }

    auto character::compute_skill(uint32_t vnum_value, character *victim, uint8_t skill_level_value) -> int {
        const bool can_use_horse_skill_value = can_use_horse_skill();

        if (false == can_use_horse_skill_value && true == is_riding())
            return BATTLE_NONE;

        if (is_polymorphed())
            return BATTLE_NONE;

        if (metin2::config::game().skill_disable)
            return BATTLE_NONE;

        skill_proto *sk = get_skill_manager().get(vnum_value);

        if (!sk)
            return BATTLE_NONE;

        if (can_use_horse_skill_value && sk->type != SKILL_TYPE_HORSE)
            return BATTLE_NONE;

        if (!can_use_horse_skill_value && sk->type == SKILL_TYPE_HORSE)
            return BATTLE_NONE;

        if (is_set(sk->flag, SKILL_FLAG_SELFONLY))
            victim = this;

        if (!victim) {
            if (metin2::config::test_server())
                metin2::log::debug("{}: skill {:d} has no victim", name(), vnum_value);

            return BATTLE_NONE;
        }

        if (sk->target_range && distance_sqrt(x() - victim->x(), y() - victim->y()) >= sk->target_range + 50) {
            if (metin2::config::test_server())
                metin2::log::debug("Skill {:d}: {} to {} out of range (distance {:d}, limit {:d})",
                    vnum_value,
                    name(),
                    victim->name(),
                    (int32_t)distance_sqrt(x() - victim->x(), y() - victim->y()),
                    sk->target_range);

            return BATTLE_NONE;
        }

        if (0 == skill_level_value) {
            if ((skill_level_value = skill_level(sk->vnum)) == 0) {
                if (metin2::config::test_server())
                    metin2::log::debug("{}: skill {:d} not learned (level {:d})", name(), sk->vnum, skill_level_value);
                return BATTLE_NONE;
            }
        }

        if (victim->is_affect_flag(AFF_PABEOP) && victim->is_good_affect(vnum_value)) {
            return BATTLE_NONE;
        }

        const auto [k, weapon] = set_skill_vars(this, victim, sk, skill_level_value, sk->type == SKILL_TYPE_HORSE);

        sk->duration_poly.set_var("k", k);
        sk->duration_poly2.set_var("k", k);

        const auto [amount, amount2, amount3] = eval_skill_amounts(this, sk, k, skill_level_value, metin2::config::test_server() && is_pc());

        if (is_set(sk->flag, SKILL_FLAG_REMOVE_BAD_AFFECT)) {
            if (prob(amount2)) {
                victim->remove_bad_affect();
            }
        }

        if (is_set(sk->flag, SKILL_FLAG_ATTACK | SKILL_FLAG_USE_MELEE_DAMAGE | SKILL_FLAG_USE_MAGIC_DAMAGE) &&
            !(sk->vnum == SKILL_MUYEONG && victim == this) && !(sk->is_charge_skill() && victim == this)) {
            bool added = false;

            if (sk->point_on == POINT_HP && amount < 0) {
                auto f = [&, x = victim->x(), y = victim->y(), disable_cooltime = disable_cooltime_, info = is_pc() ? &skill_use_info_[vnum_value] : nullptr, use_skill_power = skill_power(vnum_value, skill_level_value)](entity *ent) {
                    splash_damage(ent, x, y, sk, this, weapon, disable_cooltime, info, use_skill_power);
                };
                if (is_set(sk->flag, SKILL_FLAG_SPLASH)) {
                    if (victim->get_sectree())
                        victim->get_sectree()->for_each_around(f);
                } else {
                    f(victim);
                }
            } else {
                sk->duration_poly.set_var("k", k);
                int dur = (int)sk->duration_poly.eval();

                if (is_pc())
                    if (!(vnum_value >= GUILD_SKILL_START && vnum_value <= GUILD_SKILL_END))
                        if (!disable_cooltime_ && !skill_use_info_[vnum_value].hit_once(vnum_value) && vnum_value != SKILL_MUYEONG) {
                            return BATTLE_NONE;
                        }

                if (dur > 0) {
                    dur += point(POINT_PARTY_BUFFER_BONUS);

                    add_affect_or_splash(this, victim, victim->x(), victim->y(), sk, sk->point_on, amount, sk->affect_flag, dur, true);
                    added = true;
                }
            }

            if (sk->point_on2 != POINT_NONE && !sk->is_charge_skill()) {
                sk->duration_poly2.set_var("k", k);
                int dur = (int)sk->duration_poly2.eval();

                if (dur > 0) {
                    dur += point(POINT_PARTY_BUFFER_BONUS);

                    add_affect_or_splash(this, victim, victim->x(), victim->y(), sk, sk->point_on2, amount2, sk->affect_flag2, dur, !added);

                    added = true;
                } else {
                    victim->point_change(sk->point_on2, amount2);
                }
            }

            if (sk->point_on3 != POINT_NONE && !sk->is_charge_skill() && used_skill_master_type(sk->vnum) >= SKILL_GRAND_MASTER) {
                sk->duration_poly3.set_var("k", k);
                int dur = (int)sk->duration_poly3.eval();

                if (dur > 0) {
                    dur += point(POINT_PARTY_BUFFER_BONUS);

                    add_affect_or_splash(this, victim, victim->x(), victim->y(), sk, sk->point_on3, amount3, 0, dur, !added);

                    added = true;
                } else {
                    victim->point_change(sk->point_on3, amount3);
                }
            }

            return BATTLE_DAMAGE;
        } else {
            if (vnum_value == SKILL_MUYEONG) {
                sk->duration_poly.set_var("k", k);
                sk->duration_sp_cost_poly.set_var("k", k);

                int dur = (int32_t)sk->duration_poly.eval();
                dur += point(POINT_PARTY_BUFFER_BONUS);

                if (victim == this)
                    add_affect(vnum_value,
                        POINT_NONE, 0,
                        AFF_MUYEONG,
                        dur,
                        (int32_t)sk->duration_sp_cost_poly.eval(),
                        true);

                return BATTLE_NONE;
            }

            bool added = false;
            sk->duration_poly.set_var("k", k);
            int dur = (int)sk->duration_poly.eval();

            if (dur > 0) {
                dur += point(POINT_PARTY_BUFFER_BONUS);
                sk->duration_sp_cost_poly.set_var("k", k);

                if (sk->point_on2 != POINT_NONE) {
                    victim->remove_affect(sk->vnum);

                    int dur2 = (int)sk->duration_poly2.eval();

                    if (dur2 > 0) {
                        if (metin2::config::test_server())
                            metin2::log::debug("{}: skill affect {} for {:d}, point {:d}, amount {:d}",
                                name(),
                                sk->name,
                                dur2,
                                sk->point_on2,
                                amount2);

                        dur2 += point(POINT_PARTY_BUFFER_BONUS);
                        victim->add_affect(sk->vnum, sk->point_on2, amount2, sk->affect_flag2, dur2, 0, false);
                    } else {
                        victim->point_change(sk->point_on2, amount2);
                    }

                    uint32_t affact_flag = sk->affect_flag;

                    if ((sk->vnum == SKILL_CHUNKEON && used_skill_master_type(sk->vnum) < SKILL_GRAND_MASTER))
                        affact_flag = AFF_CHEONGEUN_WITH_FALL;

                    victim->add_affect(sk->vnum,
                        sk->point_on,
                        amount,
                        affact_flag,
                        dur,
                        (int32_t)sk->duration_sp_cost_poly.eval(),
                        false);
                } else {
                    if (metin2::config::test_server())
                        metin2::log::debug("{}: skill affect {} for {:d}, point {:d}, amount {:d}",
                            name(),
                            sk->name,
                            dur,
                            sk->point_on,
                            amount);

                    victim->add_affect(sk->vnum,
                        sk->point_on,
                        amount,
                        sk->affect_flag,
                        dur,
                        (int32_t)sk->duration_sp_cost_poly.eval(),
                        !added);
                }

                added = true;
            } else {
                if (!sk->is_charge_skill())
                    victim->point_change(sk->point_on, amount);

                if (sk->point_on2 != POINT_NONE) {
                    victim->remove_affect(sk->vnum);

                    int dur2 = (int)sk->duration_poly2.eval();

                    if (dur2 > 0) {
                        dur2 += point(POINT_PARTY_BUFFER_BONUS);

                        if (sk->is_charge_skill())
                            victim->add_affect(sk->vnum, sk->point_on2, amount2, AFF_TANHWAN_DASH, dur2, 0, false);
                        else
                            victim->add_affect(sk->vnum, sk->point_on2, amount2, sk->affect_flag2, dur2, 0, false);
                    } else {
                        victim->point_change(sk->point_on2, amount2);
                    }
                }
            }

            if (sk->point_on3 != POINT_NONE && !sk->is_charge_skill() && used_skill_master_type(sk->vnum) >= SKILL_GRAND_MASTER) {

                sk->duration_poly3.set_var("k", k);
                int dur = (int)sk->duration_poly3.eval();

                metin2::log::debug("Skill {:d}: third effect on point {:d}, duration {:d}, amount {:d}", sk->vnum, sk->point_on3, dur, amount3);

                if (dur > 0) {
                    dur += point(POINT_PARTY_BUFFER_BONUS);

                    add_affect_or_splash(this, victim, victim->x(), victim->y(), sk, sk->point_on3, amount3, 0, dur, !added);

                    added = true;
                } else {
                    victim->point_change(sk->point_on3, amount3);
                }
            }

            return BATTLE_NONE;
        }
    }

    auto character::use_skill(uint32_t vnum_value, character *victim, bool use_grand_master) -> bool {
        if (false == can_use_skill(vnum_value))
            return false;

        if (metin2::config::test_server()) {
            if (quest::get_quest_manager().event_flag("no_grand_master")) {
                use_grand_master = false;
            }
        }

        if (metin2::config::game().skill_disable)
            return false;

        if (is_observer_mode())
            return false;

        if (!can_move())
            return false;

        if (is_polymorphed())
            return false;

        const bool can_use_horse_skill_value = can_use_horse_skill();

        if (vnum_value == SKILL_HORSE_SUMMON) {
            if (skill_level(vnum_value) == 0)
                return false;

            if (horse_level() <= 0)
                chat_packet(CHAT_TYPE_INFO, lc_text("말이 없습니다. 마굿간 경비병을 찾아가세요."));
            else
                chat_packet(CHAT_TYPE_INFO, lc_text("말 소환 아이템을 사용하세요."));

            return true;
        }

        if (false == can_use_horse_skill_value && true == is_riding())
            return false;

        skill_proto *sk = get_skill_manager().get(vnum_value);
        metin2::log::debug("{} uses skill {:d} on {}", name(), vnum_value, static_cast<const void *>(victim));

        if (!sk)
            return false;

        if (can_use_horse_skill_value && sk->type != SKILL_TYPE_HORSE)
            return BATTLE_NONE;

        if (!can_use_horse_skill_value && sk->type == SKILL_TYPE_HORSE)
            return BATTLE_NONE;

        if (skill_level(vnum_value) == 0)
            return false;

        if (skill_master_type(vnum_value) < SKILL_GRAND_MASTER)
            use_grand_master = false;

        if (wear(WEAR_WEAPON) && (wear(WEAR_WEAPON)->type() == ITEM_ROD || wear(WEAR_WEAPON)->type() == ITEM_PICK))
            return false;

        skill_use_info_[vnum_value].target_vid_map.clear();

        if (sk->is_charge_skill()) {
            if (is_affect_flag(AFF_TANHWAN_DASH) || (victim && victim != this)) {
                if (!victim)
                    return false;

                if (!is_affect_flag(AFF_TANHWAN_DASH)) {
                    if (!use_skill(vnum_value, this))
                        return false;
                }

                skill_use_info_[vnum_value].set_main_target_vid(victim->get_vid());
                compute_skill(vnum_value, victim);
                remove_affect(vnum_value);
                return true;
            }
        }

        if (vnum_value == SKILL_COMBO) {
            if (combo_index_)
                combo_index_ = 0;
            else
                combo_index_ = skill_level(SKILL_COMBO);

            chat_packet(CHAT_TYPE_COMMAND, "combo %d", combo_index_);
            return true;
        }

        if ((0 != sk->affect_flag || sk->vnum == SKILL_MUYEONG) && (sk->flag & SKILL_FLAG_TOGGLE) && remove_affect(sk->vnum)) {
            return true;
        }

        if (is_affect_flag(AFF_REVIVE_INVISIBLE))
            remove_affect(AFFECT_REVIVE_INVISIBLE);

        const float k = 1.0 * skill_power(sk->vnum) * sk->max_level / 100;

        sk->set_point_var("k", k);
        sk->splash_around_damage_adjust_poly.set_var("k", k);

        sk->cooldown_poly.set_var("k", k);
        int cooltime_value = (int)sk->cooldown_poly.eval();
        int max_hit = sk->max_hit ? sk->max_hit : -1;

        sk->set_sp_cost_var("k", k);

        uint32_t cur = get_dword_time();

        if (vnum_value == SKILL_TERROR && skill_use_info_[vnum_value].used && skill_use_info_[vnum_value].next_skill_usable_time > cur) {
            metin2::log::debug("Terror skill used during cooldown ({:d} ms left)", skill_use_info_[vnum_value].next_skill_usable_time - cur);
            return false;
        }

        int needed_sp = 0;

        if (is_set(sk->flag, SKILL_FLAG_USE_HP_AS_COST)) {
            sk->set_sp_cost_var("maxhp", max_hp());
            sk->set_sp_cost_var("v", hp());
            needed_sp = (int)sk->sp_cost_poly.eval();

            if (skill_master_type(vnum_value) >= SKILL_GRAND_MASTER && use_grand_master) {
                needed_sp = (int)sk->grand_master_add_sp_cost_poly.eval();
            }

            if (hp() < needed_sp)
                return false;

            point_change(POINT_HP, -needed_sp);
        } else {
            sk->set_sp_cost_var("maxhp", max_hp());
            sk->set_sp_cost_var("maxv", max_sp());
            sk->set_sp_cost_var("v", sp());

            needed_sp = (int)sk->sp_cost_poly.eval();

            if (skill_master_type(vnum_value) >= SKILL_GRAND_MASTER && use_grand_master) {
                needed_sp = (int)sk->grand_master_add_sp_cost_poly.eval();
            }

            if (sp() < needed_sp)
                return false;

            if (metin2::config::test_server())
                chat_packet(CHAT_TYPE_INFO, lc_text("%s SP소모: %d"), sk->name, needed_sp);

            point_change(POINT_SP, -needed_sp);
        }

        if (is_set(sk->flag, SKILL_FLAG_SELFONLY))
            victim = this;

        if (sk->vnum == SKILL_MUYEONG || (sk->is_charge_skill() && !is_affect_flag(AFF_TANHWAN_DASH) && !victim)) {
            victim = this;
        }

        int splash_count = 1;

        if (false == disable_cooltime_) {
            if (false ==
                skill_use_info_[vnum_value].use_skill(
                    use_grand_master,
                    (nullptr != victim && SKILL_HORSE_WILDATTACK != vnum_value) ? victim->get_vid() : 0,
                    compute_cooltime(cooltime_value * 1000),
                    splash_count,
                    max_hit)) {
                if (metin2::config::test_server())
                    chat_packet(CHAT_TYPE_NOTICE, "cooltime not finished %s %d", sk->name, cooltime_value);

                return false;
            }
        }

        if (vnum_value == SKILL_CHAIN) {
            reset_chain_lightning_index();
            add_chain_lightning_except(victim);
        }

        if (is_set(sk->flag, SKILL_FLAG_SELFONLY))
            compute_skill(vnum_value, this);
        else if (!is_set(sk->flag, SKILL_FLAG_ATTACK))
            compute_skill(vnum_value, victim);
        else if (vnum_value == SKILL_BYEURAK)
            compute_skill(vnum_value, victim);
        else if (vnum_value == SKILL_MUYEONG || sk->is_charge_skill())
            compute_skill(vnum_value, victim);

        last_skill_time_ = get_dword_time();

        return true;
    }

    auto character::used_skill_master_type(uint32_t vnum) -> int {
        const skill_use_info &info = skill_use_info_[vnum];

        if (skill_master_type(vnum) < SKILL_GRAND_MASTER)
            return skill_master_type(vnum);

        if (info.is_grand_master)
            return skill_master_type(vnum);

        return std::min<int>(skill_master_type(vnum), SKILL_MASTER);
    }

    auto character::skill_master_type(uint32_t vnum) const -> int {
        if (!is_pc())
            return 0;

        if (vnum >= SKILL_MAX_NUM) {
            metin2::log::error("{}: skill vnum {:d} out of range", name(), vnum);
            return 0;
        }

        return skill_levels_ ? skill_levels_[vnum].master_type : static_cast<uint8_t>(SKILL_NORMAL);
    }

    auto character::skill_power(uint32_t vnum, uint8_t level) const -> int {
        if (vnum >= SKILL_LANGUAGE1 && vnum <= SKILL_LANGUAGE3 && is_equip_unique_group(UNIQUE_GROUP_RING_OF_LANGUAGE)) {
            return 100;
        }

        if (vnum >= GUILD_SKILL_START && vnum <= GUILD_SKILL_END) {
            if (get_guild())
                return 100 * get_guild()->skill_level(vnum) / 7 / 7;
            else
                return 0;
        }

        if (level) {
            return skill_power_by_level(level, true);
        }

        if (vnum >= SKILL_MAX_NUM) {
            metin2::log::error("{}: skill vnum {:d} out of range", name(), vnum);
            return 0;
        }

        return skill_power_by_level(skill_level(vnum));
    }

    auto character::skill_level(uint32_t vnum) const -> int {
        if (vnum >= SKILL_MAX_NUM) {
            metin2::log::error("{}: skill vnum {:d} out of range", name(), vnum);
            return 0;
        }

        return std::min(SKILL_MAX_LEVEL, skill_levels_ ? skill_levels_[vnum].level : 0);
    }

    EVENTFUNC(skill_muyoung_event) {
        auto *info = dynamic_cast<char_event_info *>(event->info.get());

        if (info == nullptr) {
            metin2::log::error("Muyoung skill event fired without event info");
            return 0;
        }

        character *ch = info->ch;

        if (ch == nullptr) {
            return 0;
        }

        if (!ch->is_affect_flag(AFF_MUYEONG)) {
            ch->stop_muyeong_event();
            return 0;
        }

        if (ch->get_sectree()) {
            if (character *victim = find_near_victim(ch, ch)) {
                ch->create_fly(FLY_SKILL_MUYEONG, victim);
                ch->compute_skill(SKILL_MUYEONG, victim);
            }
        }

        return passes_per_sec(3);
    }

    void character::start_muyeong_event() {
        if (muyeong_event_)
            return;

        auto *info = alloc_event_info<char_event_info>();

        info->ch = this;
        muyeong_event_ = event_create(skill_muyoung_event, info, passes_per_sec(1));
    }

    void character::stop_muyeong_event() {
        event_cancel(&muyeong_event_);
    }

    void character::skill_learn_wait_more_time_message(uint32_t ms) {
        if (ms < 3 * 60)
            chat_packet(CHAT_TYPE_TALKING, "%s", lc_text("몸 속이 뜨겁군. 하지만 아주 편안해. 이대로 기를 안정시키자."));
        else if (ms < 5 * 60)
            chat_packet(CHAT_TYPE_TALKING, "%s", lc_text("그래, 천천히. 좀더 천천히, 그러나 막힘 없이 빠르게!"));
        else if (ms < 10 * 60)
            chat_packet(CHAT_TYPE_TALKING, "%s", lc_text("그래, 이 느낌이야. 체내에 기가 아주 충만해."));
        else if (ms < 30 * 60) {
            chat_packet(CHAT_TYPE_TALKING, "%s", lc_text("다 읽었다! 이제 비급에 적혀있는 대로 전신에 기를 돌리기만 하면,"));
            chat_packet(CHAT_TYPE_TALKING, "%s", lc_text("그것으로 수련은 끝난 거야!"));
        } else if (ms < 1 * 3600)
            chat_packet(CHAT_TYPE_TALKING, "%s", lc_text("이제 책의 마지막 장이야! 수련의 끝이 눈에 보이고 있어!"));
        else if (ms < 2 * 3600)
            chat_packet(CHAT_TYPE_TALKING, "%s", lc_text("얼마 안 남았어! 조금만 더!"));
        else if (ms < 3 * 3600)
            chat_packet(CHAT_TYPE_TALKING, "%s", lc_text("좋았어! 조금만 더 읽으면 끝이다!"));
        else if (ms < 6 * 3600) {
            chat_packet(CHAT_TYPE_TALKING, "%s", lc_text("책장도 이제 얼마 남지 않았군."));
            chat_packet(CHAT_TYPE_TALKING, "%s", lc_text("뭔가 몸 안에 힘이 생기는 기분인 걸."));
        } else if (ms < 12 * 3600) {
            chat_packet(CHAT_TYPE_TALKING, "%s", lc_text("이제 좀 슬슬 가닥이 잡히는 것 같은데."));
            chat_packet(CHAT_TYPE_TALKING, "%s", lc_text("좋아, 이 기세로 계속 나간다!"));
        } else if (ms < 18 * 3600) {
            chat_packet(CHAT_TYPE_TALKING, "%s", lc_text("아니 어떻게 된 게 종일 읽어도 머리에 안 들어오냐."));
            chat_packet(CHAT_TYPE_TALKING, "%s", lc_text("공부하기 싫어지네."));
        } else {
            chat_packet(CHAT_TYPE_TALKING, "%s", lc_text("생각만큼 읽기가 쉽지가 않군. 이해도 어렵고 내용도 난해해."));
            chat_packet(CHAT_TYPE_TALKING, "%s", lc_text("이래서야 공부가 안된다구."));
        }
    }

    void character::disable_cooltime() {
        disable_cooltime_ = true;
    }

    auto character::has_mob_skill() const -> bool {
        return count_mob_skill() > 0;
    }

    auto character::count_mob_skill() const -> size_t {
        if (!mob_data)
            return 0;

        size_t c = 0;

        for (size_t i = 0; i < MOB_SKILL_MAX_NUM; ++i)
            if (mob_data->table.skills[i].vnum)
                ++c;

        return c;
    }

    auto character::mob_skill(unsigned int idx) const -> const mob_skill_info * {
        if (idx >= MOB_SKILL_MAX_NUM)
            return nullptr;

        if (!mob_data)
            return nullptr;

        if (0 == mob_data->table.skills[idx].vnum)
            return nullptr;

        return &mob_data->mob_skill_info[idx];
    }

    auto character::can_use_mob_skill(unsigned int idx) const -> bool {
        const mob_skill_info *info = mob_skill(idx);

        if (!info)
            return false;

        if (mob_skill_cooltime_[idx] > get_dword_time())
            return false;

        if (number(0, 1))
            return false;

        return true;
    }

    EVENTINFO(mob_skill_event_info) {
        dynamic_character_ptr ch;
        pixel_position pos;
        uint32_t vnum;
        int index;
        uint8_t level;

        mob_skill_event_info()
            : ch(), pos(), vnum(0), index(0), level(0) {
        }
    };

    EVENTFUNC(mob_skill_hit_event) {
        auto *info = dynamic_cast<mob_skill_event_info *>(event->info.get());

        if (info == nullptr) {
            metin2::log::error("Mob skill event fired without event info");
            return 0;
        }

        character *ch = info->ch;
        if (ch == nullptr) {
            return 0;
        }

        ch->compute_skill_at_position(info->vnum, info->pos, info->level);
        ch->mob_skill_event.erase(info->index);

        return 0;
    }

    auto character::use_mob_skill(unsigned int idx) -> bool {
        if (is_pc())
            return false;

        const mob_skill_info *info_value = mob_skill(idx);

        if (!info_value)
            return false;

        uint32_t vnum_value = info_value->skill_vnum;
        skill_proto *sk = get_skill_manager().get(vnum_value);

        if (!sk)
            return false;

        const float k = 1.0 * skill_power(sk->vnum, info_value->skill_level) * sk->max_level / 100;

        sk->cooldown_poly.set_var("k", k);
        int cooltime = (int)(sk->cooldown_poly.eval() * 1000);

        mob_skill_cooltime_[idx] = get_dword_time() + cooltime;

        metin2::log::debug("Mob {} uses skill index {:d} (vnum {:d}, cooldown {:d})", name(), idx, vnum_value, cooltime);

        if (mob_data->mob_skill_info[idx].splash_attack.empty()) {
            metin2::log::error("Mob {} has no hit data for skill index {:d}", name(), idx);
            return false;
        }

        for (size_t i = 0; i < mob_data->mob_skill_info[idx].splash_attack.size(); i++) {
            pixel_position pos = xyz();
            const mob_splash_attack_info &r_info = mob_data->mob_skill_info[idx].splash_attack[i];

            if (r_info.hit_distance) {
                float fx, fy;
                get_delta_by_degree(rotation(), r_info.hit_distance, &fx, &fy);
                pos.x += (int32_t)fx;
                pos.y += (int32_t)fy;
            }

            if (r_info.timing) {
                if (metin2::config::test_server())
                    metin2::log::debug("Mob skill hit timing {:d} ms", r_info.timing);

                auto *info = alloc_event_info<mob_skill_event_info>();

                info->ch = this;
                info->pos = pos;
                info->level = info_value->skill_level;
                info->vnum = vnum_value;
                info->index = i;

                auto it = mob_skill_event.find(i);
                if (it != mob_skill_event.end()) {
                    event_ptr existing = it->second;
                    event_cancel(&existing);
                    mob_skill_event.erase(it);
                }

                mob_skill_event.insert(std::make_pair(i, event_create(mob_skill_hit_event, info, passes_per_sec(r_info.timing) / 1000)));
            } else {
                compute_skill_at_position(vnum_value, pos, info_value->skill_level);
            }
        }

        return true;
    }

    void character::reset_mob_skill_cooltime() {
        memset(mob_skill_cooltime_, 0, sizeof(mob_skill_cooltime_));
    }

    auto character::is_usable_skill_motion(uint32_t motion_index) const -> bool {
        uint32_t self_job_group = (job() + 1) * 10 + skill_group();

        const uint32_t skill_num = 158;
        static uint32_t an_skill2_job_group[skill_num] = {
            0,
            11,
            11,
            11,
            11,
            11,
            11,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            12,
            12,
            12,
            12,
            12,
            12,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            21,
            21,
            21,
            21,
            21,
            21,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            22,
            22,
            22,
            22,
            22,
            22,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            31,
            31,
            31,
            31,
            31,
            31,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            32,
            32,
            32,
            32,
            32,
            32,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            41,
            41,
            41,
            41,
            41,
            41,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            42,
            42,
            42,
            42,
            42,
            42,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
        };

        const uint32_t motion_max_num = 124;
        const uint32_t skill_list_max_count = 5;

        static uint32_t an_motion2_skill_vnum_list[motion_max_num][skill_list_max_count] =
            {
                {0, 0,   0,   0,  0  },

                {4, 1,   31,  61, 91 },
                {4, 2,   32,  62, 92 },
                {4, 3,   33,  63, 93 },
                {4, 4,   34,  64, 94 },
                {4, 5,   35,  65, 95 },
                {4, 6,   36,  66, 96 },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },

                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },

                {4, 16,  46,  76, 106},
                {4, 17,  47,  77, 107},
                {4, 18,  48,  78, 108},
                {4, 19,  49,  79, 109},
                {4, 20,  50,  80, 110},
                {4, 21,  51,  81, 111},
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },

                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },

                {4, 1,   31,  61, 91 },
                {4, 2,   32,  62, 92 },
                {4, 3,   33,  63, 93 },
                {4, 4,   34,  64, 94 },
                {4, 5,   35,  65, 95 },
                {4, 6,   36,  66, 96 },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },

                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },

                {4, 16,  46,  76, 106},
                {4, 17,  47,  77, 107},
                {4, 18,  48,  78, 108},
                {4, 19,  49,  79, 109},
                {4, 20,  50,  80, 110},
                {4, 21,  51,  81, 111},
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },

                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },

                {4, 1,   31,  61, 91 },
                {4, 2,   32,  62, 92 },
                {4, 3,   33,  63, 93 },
                {4, 4,   34,  64, 94 },
                {4, 5,   35,  65, 95 },
                {4, 6,   36,  66, 96 },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },

                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },

                {4, 16,  46,  76, 106},
                {4, 17,  47,  77, 107},
                {4, 18,  48,  78, 108},
                {4, 19,  49,  79, 109},
                {4, 20,  50,  80, 110},
                {4, 21,  51,  81, 111},
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },

                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },

                {4, 1,   31,  61, 91 },
                {4, 2,   32,  62, 92 },
                {4, 3,   33,  63, 93 },
                {4, 4,   34,  64, 94 },
                {4, 5,   35,  65, 95 },
                {4, 6,   36,  66, 96 },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },

                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },

                {4, 16,  46,  76, 106},
                {4, 17,  47,  77, 107},
                {4, 18,  48,  78, 108},
                {4, 19,  49,  79, 109},
                {4, 20,  50,  80, 110},
                {4, 21,  51,  81, 111},
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },

                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },

                {1, 152, 0,   0,  0  },
                {1, 153, 0,   0,  0  },
                {1, 154, 0,   0,  0  },
                {1, 155, 0,   0,  0  },
                {1, 156, 0,   0,  0  },
                {1, 157, 0,   0,  0  },

                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },
                {0, 0,   0,   0,  0  },

                {2, 137, 140, 0,  0  },
                {1, 138, 0,   0,  0  },
                {1, 139, 0,   0,  0  },
        };

        if (motion_index >= motion_max_num) {
            metin2::log::error("{}: motion {:d} out of range (max {:d})", name(), motion_index, motion_max_num);
            return false;
        }

        uint32_t *skill_v_nums = an_motion2_skill_vnum_list[motion_index];

        uint32_t skill_count = *skill_v_nums++;
        if (skill_count >= skill_list_max_count) {
            metin2::log::error("{}: skill count {:d} out of range (max {:d})", name(), skill_count, skill_list_max_count);
            return false;
        }

        for (uint32_t skill_index = 0; skill_index != skill_count; ++skill_index) {
            if (skill_index >= SKILL_MAX_NUM) {
                metin2::log::error("{}: skill {:d} out of range (max {:d})", name(), skill_index, SKILL_MAX_NUM);
                return false;
            }

            uint32_t each_skill_v_num = skill_v_nums[skill_index];
            if (each_skill_v_num != 0) {
                uint32_t each_job_group = an_skill2_job_group[each_skill_v_num];

                if (0 == each_job_group || each_job_group == self_job_group) {
                    uint32_t each_skill_level = 0;

                    if (each_skill_v_num >= GUILD_SKILL_START && each_skill_v_num <= GUILD_SKILL_END) {
                        if (get_guild())
                            each_skill_level = get_guild()->skill_level(each_skill_v_num);
                        else
                            each_skill_level = 0;
                    } else {
                        each_skill_level = skill_level(each_skill_v_num);
                    }

                    if (each_skill_level > 0) {
                        return true;
                    }
                }
            }
        }

        return false;
    }

    void character::clear_skill() {
        point_change(POINT_SKILL, 4 + (level() - 5) - point(POINT_SKILL));

        reset_skill();
    }

    void character::clear_sub_skill() {
        point_change(POINT_SUB_SKILL, level() < 10 ? 0 : (level() - 9) - point(POINT_SUB_SKILL));

        if (skill_levels_ == nullptr) {
            metin2::log::error("Skill levels not loaded for {}", name());
            return;
        }

        player_skill clean_skill;
        memset(&clean_skill, 0, sizeof(player_skill));

        size_t count = sizeof(sub_skill_vnums) / sizeof(sub_skill_vnums[0]);

        for (size_t i = 0; i < count; ++i) {
            if (sub_skill_vnums[i] >= SKILL_MAX_NUM)
                continue;

            skill_levels_[sub_skill_vnums[i]] = clean_skill;
        }

        compute_points();
        skill_level_packet();
    }

    auto character::reset_one_skill(uint32_t vnum) -> bool {
        if (nullptr == skill_levels_) {
            metin2::log::error("Skill levels not loaded for {} (skill {:d})", name(), vnum);
            return false;
        }

        if (vnum >= SKILL_MAX_NUM) {
            metin2::log::error("{}: skill vnum {:d} out of range", name(), vnum);
            return false;
        }

        uint8_t level = skill_levels_[vnum].level;

        skill_levels_[vnum].level = 0;
        skill_levels_[vnum].master_type = 0;
        skill_levels_[vnum].t_next_read = 0;

        if (level > 17)
            level = 17;

        point_change(POINT_SKILL, level);

        get_log_manager().char_log(this, vnum, "ONE_SKILL_RESET_BY_SCROLL", "");

        compute_points();
        skill_level_packet();

        return true;
    }

    auto character::can_use_skill(uint32_t skill_vnum) const -> bool {
        if (0 == skill_vnum) return false;

        if (0 < skill_group()) {
            const int skill_count = 6;
            static const uint32_t skill_list[JOB_MAX_NUM][SKILL_GROUP_MAX_NUM][skill_count] =
                {
                    {{1, 2, 3, 4, 5, 6},       {16, 17, 18, 19, 20, 21}      },
                    {{31, 32, 33, 34, 35, 36}, {46, 47, 48, 49, 50, 51}      },
                    {{61, 62, 63, 64, 65, 66}, {76, 77, 78, 79, 80, 81}      },
                    {{91, 92, 93, 94, 95, 96}, {106, 107, 108, 109, 110, 111}},
            };

            const uint32_t *skill = skill_list[job()][skill_group() - 1];

            for (int i = 0; i < skill_count; ++i) {
                if (skill[i] == skill_vnum) return true;
            }
        }

        if (true == is_riding()) {
            if (get_mount_vnum()) {
                if (get_mount_vnum() < 20209 && get_mount_vnum() > 20212)
                    if (get_mount_vnum() != 20215 || get_mount_vnum() != 20218 || get_mount_vnum() != 20220)
                        return false;
            }

            switch (skill_vnum) {
            case SKILL_HORSE_WILDATTACK:
            case SKILL_HORSE_CHARGE:
            case SKILL_HORSE_ESCAPE:
            case SKILL_HORSE_WILDATTACK_RANGE:
                return true;
            }
        }

        switch (skill_vnum) {
        case 121:
        case 122:
        case 124:
        case 126:
        case 127:
        case 128:
        case 129:
        case 130:
        case 131:
        case 151:
        case 152:
        case 153:
        case 154:
        case 155:
        case 156:
        case 157:
        case 158:
        case 159:
            return true;
        }

        return false;
    }

    auto character::check_skill_hit_count(const uint8_t skill_id, const vid target_vid) -> bool {
        auto iter = skill_use_info_.find(skill_id);

        if (iter == skill_use_info_.end()) {
            metin2::log::warn("Possible skill hack by {}: skill {:d} was never used", name(), skill_id);
            return false;
        }

        skill_use_info &skill_use_info = iter->second;

        if (false == skill_use_info.used) {
            metin2::log::warn("Possible skill hack by {}: skill {:d} is not in use", name(), skill_id);
            return false;
        }

        switch (skill_id) {
        case SKILL_YONGKWON:
        case SKILL_HWAYEOMPOK:
        case SKILL_DAEJINGAK:
        case SKILL_PAERYONG:
            metin2::log::warn("Possible skill hack by {}: attack packet for skill {:d}", name(), skill_id);
            return false;
        }

        auto iter_target_map = skill_use_info.target_vid_map.find(target_vid);

        if (skill_use_info.target_vid_map.end() != iter_target_map) {
            size_t max_attack_count_per_target = 1;

            switch (skill_id) {
            case SKILL_SAMYEON:
            case SKILL_CHARYUN:
                max_attack_count_per_target = 3;
                break;

            case SKILL_HORSE_WILDATTACK_RANGE:
                max_attack_count_per_target = 5;
                break;

            case SKILL_YEONSA:
                max_attack_count_per_target = 7;
                break;

            case SKILL_HORSE_ESCAPE:
                max_attack_count_per_target = 10;
                break;
            }

            if (iter_target_map->second >= max_attack_count_per_target) {
                metin2::log::warn("Possible skill hack by {}: too many hits with skill {:d} ({:d})", name(), skill_id, iter_target_map->second);
                return false;
            }

            iter_target_map->second++;
        } else {
            skill_use_info.target_vid_map.insert(std::make_pair(target_vid, 1));
        }

        return true;
    }
}
