#include <metin2/game/combat/skill.h>

#include <metin2/game/admin/cmd.h>
#include <metin2/game/characters/character.h>

#include <metin2/log/log.h>
#include <metin2/proto/tables.h>

#include <cstdint>
#include <cstdio>
#include <format>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include <strings.h>

namespace metin2::game::inline combat {

    void skill_proto::set_point_var(const std::string &name, double var) {
        point_poly.set_var(name, var);
        point_poly2.set_var(name, var);
        point_poly3.set_var(name, var);
        master_bonus_poly.set_var(name, var);
    }

    void skill_proto::set_duration_var(const std::string &name, double var) {
        duration_poly.set_var(name, var);
        duration_poly2.set_var(name, var);
        duration_poly3.set_var(name, var);
    }

    void skill_proto::set_sp_cost_var(const std::string &name, double var) {
        sp_cost_poly.set_var(name, var);
        grand_master_add_sp_cost_poly.set_var(name, var);
    }

    skill_manager::skill_manager() {
    }

    skill_manager::~skill_manager() {
    }

    struct point_on_type {
        const char *psz_name;
        int point_on;
    } point_on_types[] = {
        {"NONE",                    POINT_NONE                   },
        {"MAX_HP",                  POINT_MAX_HP                 },
        {"MAX_SP",                  POINT_MAX_SP                 },
        {"HP_REGEN",                POINT_HP_REGEN               },
        {"SP_REGEN",                POINT_SP_REGEN               },
        {"BLOCK",                   POINT_BLOCK                  },
        {"HP",                      POINT_HP                     },
        {"SP",                      POINT_SP                     },
        {"ATT_GRADE",               POINT_ATT_GRADE_BONUS        },
        {"DEF_GRADE",               POINT_DEF_GRADE_BONUS        },
        {"MAGIC_ATT_GRADE",         POINT_MAGIC_ATT_GRADE_BONUS  },
        {"MAGIC_DEF_GRADE",         POINT_MAGIC_DEF_GRADE_BONUS  },
        {"BOW_DISTANCE",            POINT_BOW_DISTANCE           },
        {"MOV_SPEED",               POINT_MOV_SPEED              },
        {"ATT_SPEED",               POINT_ATT_SPEED              },
        {"POISON_PCT",              POINT_POISON_PCT             },
        {"RESIST_RANGE",            POINT_RESIST_BOW             },
        {"CASTING_SPEED",           POINT_CASTING_SPEED          },
        {"REFLECT_MELEE",           POINT_REFLECT_MELEE          },
        {"ATT_BONUS",               POINT_ATT_BONUS              },
        {"DEF_BONUS",               POINT_DEF_BONUS              },
        {"RESIST_NORMAL",           POINT_RESIST_NORMAL_DAMAGE   },
        {"DODGE",                   POINT_DODGE                  },
        {"KILL_HP_RECOVER",         POINT_KILL_HP_RECOVERY       },
        {"KILL_SP_RECOVER",         POINT_KILL_SP_RECOVER        },
        {"HIT_HP_RECOVER",          POINT_HIT_HP_RECOVERY        },
        {"HIT_SP_RECOVER",          POINT_HIT_SP_RECOVERY        },
        {"CRITICAL",                POINT_CRITICAL_PCT           },
        {"MANASHIELD",              POINT_MANASHIELD             },
        {"SKILL_DAMAGE_BONUS",      POINT_SKILL_DAMAGE_BONUS     },
        {"NORMAL_HIT_DAMAGE_BONUS", POINT_NORMAL_HIT_DAMAGE_BONUS},
        {"\n",                      POINT_NONE                   },
    };

    auto find_point_type(const char *sz) -> int {
        for (int i = 0; *point_on_types[i].psz_name != '\n'; ++i) {
            if (!strcasecmp(sz, point_on_types[i].psz_name))
                return point_on_types[i].point_on;
        }
        return -1;
    }

    auto skill_manager::initialize(skill_table *tab, int size) -> bool {
        std::string buf;
        std::map<uint32_t, std::unique_ptr<skill_proto>> map_pkSkillProto;

        skill_table *t = tab;
        bool bError = false;

        for (int i = 0; i < size; ++i, ++t) {
            auto pkProto = std::make_unique<skill_proto>();

            pkProto->vnum = t->vnum;
            strlcpy(pkProto->name, t->name, sizeof(pkProto->name));
            pkProto->type = t->type;
            pkProto->max_level = t->max_level;
            pkProto->flag = t->flag;
            pkProto->affect_flag = t->affect_flag;
            pkProto->affect_flag2 = t->affect_flag2;

            pkProto->level_step = t->level_step;
            pkProto->level_limit = t->level_limit;
            pkProto->splash_range = t->splash_range;
            pkProto->target_range = t->target_range;
            pkProto->pre_skill_vnum = t->pre_skill_vnum;
            pkProto->pre_skill_level = t->pre_skill_level;

            pkProto->max_hit = t->max_hit;

            pkProto->skill_attr_type = t->skill_attr_type;

            int iIdx = find_point_type(t->point_on);

            if (iIdx < 0) {
                buf = std::format("SkillManager::Initialize : cannot find point type on skill: {} szPointOn: {}", t->name, t->point_on);
                metin2::log::error("{}", buf);
                send_log(buf.c_str());
                bError = true;
                continue;
            }

            pkProto->point_on = iIdx;

            int iIdx2 = find_point_type(t->point_on2);

            if (iIdx2 < 0) {
                buf = std::format("SkillManager::Initialize : cannot find point type on skill: {} szPointOn2: {}", t->name, t->point_on2);
                metin2::log::error("{}", buf);
                send_log(buf.c_str());
                bError = true;
                continue;
            }

            pkProto->point_on2 = iIdx2;

            int iIdx3 = find_point_type(t->point_on3);

            if (iIdx3 < 0) {
                if (t->point_on3[0] == 0) {
                    iIdx3 = POINT_NONE;
                } else {
                    buf = std::format("SkillManager::Initialize : cannot find point type on skill: {} szPointOn3: {}", t->name, t->point_on3);
                    metin2::log::error("{}", buf);
                    send_log(buf.c_str());
                    bError = true;
                    continue;
                }
            }

            pkProto->point_on3 = iIdx3;

            if (!pkProto->splash_around_damage_adjust_poly.analyze(t->splash_around_damage_adjust_poly)) {
                buf = std::format("SkillManager::Initialize : syntax error skill: {} szSplashAroundDamageAdjustPoly: {}", t->name, t->splash_around_damage_adjust_poly);
                metin2::log::error("{}", buf);
                send_log(buf.c_str());
                bError = true;
                continue;
            }

            if (!pkProto->point_poly.analyze(t->point_poly)) {
                buf = std::format("SkillManager::Initialize : syntax error skill: {} szPointPoly: {}", t->name, t->point_poly);
                metin2::log::error("{}", buf);
                send_log(buf.c_str());
                bError = true;
                continue;
            }

            if (!pkProto->point_poly2.analyze(t->point_poly2)) {
                buf = std::format("SkillManager::Initialize : syntax error skill: {} szPointPoly2: {}", t->name, t->point_poly2);
                metin2::log::error("{}", buf);
                send_log(buf.c_str());
                bError = true;
                continue;
            }

            if (!pkProto->point_poly3.analyze(t->point_poly3)) {
                buf = std::format("SkillManager::Initialize : syntax error skill: {} szPointPoly3: {}", t->name, t->point_poly3);
                metin2::log::error("{}", buf);
                send_log(buf.c_str());
                bError = true;
                continue;
            }

            if (!pkProto->sp_cost_poly.analyze(t->sp_cost_poly)) {
                buf = std::format("SkillManager::Initialize : syntax error skill: {} szSPCostPoly: {}", t->name, t->sp_cost_poly);
                metin2::log::error("{}", buf);
                send_log(buf.c_str());
                bError = true;
                continue;
            }

            if (!pkProto->grand_master_add_sp_cost_poly.analyze(t->grand_master_add_sp_cost_poly)) {
                buf = std::format("SkillManager::Initialize : syntax error skill: {} szGrandMasterAddSPCostPoly: {}", t->name, t->grand_master_add_sp_cost_poly);
                metin2::log::error("{}", buf);
                send_log(buf.c_str());
                bError = true;
                continue;
            }

            if (!pkProto->duration_poly.analyze(t->duration_poly)) {
                buf = std::format("SkillManager::Initialize : syntax error skill: {} szDurationPoly: {}", t->name, t->duration_poly);
                metin2::log::error("{}", buf);
                send_log(buf.c_str());
                bError = true;
                continue;
            }

            if (!pkProto->duration_poly2.analyze(t->duration_poly2)) {
                buf = std::format("SkillManager::Initialize : syntax error skill: {} szDurationPoly2: {}", t->name, t->duration_poly2);
                metin2::log::error("{}", buf);
                send_log(buf.c_str());
                bError = true;
                continue;
            }

            if (!pkProto->duration_poly3.analyze(t->duration_poly3)) {
                buf = std::format("SkillManager::Initialize : syntax error skill: {} szDurationPoly3: {}", t->name, t->duration_poly3);
                metin2::log::error("{}", buf);
                send_log(buf.c_str());
                bError = true;
                continue;
            }

            if (!pkProto->duration_sp_cost_poly.analyze(t->duration_sp_cost_poly)) {
                buf = std::format("SkillManager::Initialize : syntax error skill: {} szDurationSPCostPoly: {}", t->name, t->duration_sp_cost_poly);
                metin2::log::error("{}", buf);
                send_log(buf.c_str());
                bError = true;
                continue;
            }

            if (!pkProto->cooldown_poly.analyze(t->cooldown_poly)) {
                buf = std::format("SkillManager::Initialize : syntax error skill: {} szCooldownPoly: {}", t->name, t->cooldown_poly);
                metin2::log::error("{}", buf);
                send_log(buf.c_str());
                bError = true;
                continue;
            }

            metin2::log::debug("Skill {}: master bonus poly {}", t->name, t->master_bonus_poly);
            if (!pkProto->master_bonus_poly.analyze(t->master_bonus_poly)) {
                buf = std::format("SkillManager::Initialize : syntax error skill: {} szMasterBonusPoly: {}", t->name, t->master_bonus_poly);
                metin2::log::error("{}", buf);
                send_log(buf.c_str());
                bError = true;
                continue;
            }

            metin2::log::debug("Skill {:d} {}: type {:d} flag {:d} affect {:d} point poly {}",
                pkProto->vnum, pkProto->name, pkProto->type, pkProto->flag, pkProto->affect_flag, t->point_poly);

            map_pkSkillProto.emplace(pkProto->vnum, std::move(pkProto));
        }

        if (!bError) {
            map_skill_proto_.clear();

            for (auto &[dwVnum, pkProto] : map_pkSkillProto)
                map_skill_proto_.emplace(dwVnum, std::move(pkProto));

            send_log("Skill Prototype reloaded!");
        } else
            send_log("There were erros when loading skill table");

        return !bError;
    }

    auto skill_manager::get(uint32_t vnum) -> skill_proto * {
        auto it = map_skill_proto_.find(vnum);

        if (it == map_skill_proto_.end())
            return nullptr;

        return it->second.get();
    }

    auto skill_manager::get(const char *skill_name) -> skill_proto * {
        auto it = map_skill_proto_.begin();

        while (it != map_skill_proto_.end()) {
            if (!strcasecmp(it->second->name, skill_name))
                return it->second.get();

            it++;
        }

        return nullptr;
    }
}
