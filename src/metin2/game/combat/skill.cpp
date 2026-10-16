#include <metin2/game/combat/skill.h>

#include <metin2/game/admin/cmd.h>
#include <metin2/game/characters/character.h>

#include <metin2/log/log.h>
#include <metin2/proto/tables.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <format>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

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

    skill_manager::skill_manager() = default;

    skill_manager::~skill_manager() = default;

    namespace {
        auto iequals(std::string_view a, std::string_view b) -> bool {
            return std::ranges::equal(a, b, [](unsigned char x, unsigned char y) -> bool { return std::tolower(x) == std::tolower(y); });
        }

        struct point_on_type {
            std::string_view name;
            int point_on;
        };

        constexpr auto point_on_types = std::to_array<point_on_type>({
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
        });

        auto find_point_type(std::string_view name) -> int {
            const auto it = std::ranges::find_if(point_on_types, [&](const auto &entry) { return iequals(name, entry.name); });
            return it == point_on_types.end() ? -1 : it->point_on;
        }
    }

    auto skill_manager::initialize(skill_table *tab, int size) -> bool {
        std::map<uint32_t, std::unique_ptr<skill_proto>> map_pk_skill_proto;

        skill_table *t = tab;
        bool error_value = false;

        for (int i = 0; i < size; ++i, ++t) {
            auto proto = std::make_unique<skill_proto>();

            proto->vnum = t->vnum;
            strlcpy(proto->name, t->name, sizeof(proto->name));
            proto->type = t->type;
            proto->max_level = t->max_level;
            proto->flag = t->flag;
            proto->affect_flag = t->affect_flag;
            proto->affect_flag2 = t->affect_flag2;

            proto->level_step = t->level_step;
            proto->level_limit = t->level_limit;
            proto->splash_range = t->splash_range;
            proto->target_range = t->target_range;
            proto->pre_skill_vnum = t->pre_skill_vnum;
            proto->pre_skill_level = t->pre_skill_level;

            proto->max_hit = t->max_hit;

            proto->skill_attr_type = t->skill_attr_type;

            const auto report = [&](const std::string &message) {
                metin2::log::error("{}", message);
                send_log(message.c_str());
                error_value = true;
            };

            int idx = find_point_type(t->point_on);

            if (idx < 0) {
                report(std::format("SkillManager::Initialize : cannot find point type on skill: {} szPointOn: {}", t->name, t->point_on));
                continue;
            }

            proto->point_on = idx;

            int idx2 = find_point_type(t->point_on2);

            if (idx2 < 0) {
                report(std::format("SkillManager::Initialize : cannot find point type on skill: {} szPointOn2: {}", t->name, t->point_on2));
                continue;
            }

            proto->point_on2 = idx2;

            int idx3 = find_point_type(t->point_on3);

            if (idx3 < 0) {
                if (t->point_on3[0] == 0) {
                    idx3 = POINT_NONE;
                } else {
                    report(std::format("SkillManager::Initialize : cannot find point type on skill: {} szPointOn3: {}", t->name, t->point_on3));
                    continue;
                }
            }

            proto->point_on3 = idx3;

            struct poly_field {
                expression *poly;
                const char *source;
                const char *name;
            };

            const poly_field polys[] = {
                {&proto->splash_around_damage_adjust_poly, t->splash_around_damage_adjust_poly, "szSplashAroundDamageAdjustPoly"},
                {&proto->point_poly,                       t->point_poly,                       "szPointPoly"                   },
                {&proto->point_poly2,                      t->point_poly2,                      "szPointPoly2"                  },
                {&proto->point_poly3,                      t->point_poly3,                      "szPointPoly3"                  },
                {&proto->sp_cost_poly,                     t->sp_cost_poly,                     "szSPCostPoly"                  },
                {&proto->grand_master_add_sp_cost_poly,    t->grand_master_add_sp_cost_poly,    "szGrandMasterAddSPCostPoly"    },
                {&proto->duration_poly,                    t->duration_poly,                    "szDurationPoly"                },
                {&proto->duration_poly2,                   t->duration_poly2,                   "szDurationPoly2"               },
                {&proto->duration_poly3,                   t->duration_poly3,                   "szDurationPoly3"               },
                {&proto->duration_sp_cost_poly,            t->duration_sp_cost_poly,            "szDurationSPCostPoly"          },
                {&proto->cooldown_poly,                    t->cooldown_poly,                    "szCooldownPoly"                },
                {&proto->master_bonus_poly,                t->master_bonus_poly,                "szMasterBonusPoly"             },
            };

            const poly_field *bad_poly = nullptr;
            for (const poly_field &field : polys) {
                if (field.poly == &proto->master_bonus_poly)
                    metin2::log::debug("Skill {}: master bonus poly {}", t->name, t->master_bonus_poly);

                if (!field.poly->analyze(field.source)) {
                    bad_poly = &field;
                    break;
                }
            }

            if (bad_poly) {
                report(std::format("SkillManager::Initialize : syntax error skill: {} {}: {}", t->name, bad_poly->name, bad_poly->source));
                continue;
            }

            metin2::log::debug("Skill {:d} {}: type {:d} flag {:d} affect {:d} point poly {}",
                proto->vnum, proto->name, proto->type, proto->flag, proto->affect_flag, t->point_poly);

            map_pk_skill_proto.emplace(proto->vnum, std::move(proto));
        }

        if (!error_value) {
            map_skill_proto_.clear();

            for (auto &[vnum, proto] : map_pk_skill_proto)
                map_skill_proto_.emplace(vnum, std::move(proto));

            send_log("Skill Prototype reloaded!");
        } else
            send_log("There were erros when loading skill table");

        return !error_value;
    }

    auto skill_manager::get(uint32_t vnum) -> skill_proto * {
        auto it = map_skill_proto_.find(vnum);

        if (it == map_skill_proto_.end())
            return nullptr;

        return it->second.get();
    }

    auto skill_manager::get(const char *skill_name) -> skill_proto * {
        const auto it = std::ranges::find_if(map_skill_proto_, [&](const auto &entry) { return iequals(entry.second->name, skill_name); });
        return it == map_skill_proto_.end() ? nullptr : it->second.get();
    }
}
