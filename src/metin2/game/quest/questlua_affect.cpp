#include <metin2/game/app/constants.h>
#include <metin2/game/characters/affect.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/questmanager.h>

#include <metin2/log/log.h>
#include <metin2/proto/apply_types.h>
#include <metin2/proto/character_types.h>

#include <cstdint>
#include <list>

namespace metin2::game {

    namespace quest {
        auto affect_add(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3)) {
                metin2::log::error("Invalid arguments");
                return 0;
            }

            quest_manager &q = quest_manager::instance();

            auto applyOn = (uint8_t)lua_tonumber(lua, 1);

            character *ch = q.current_character_ptr();

            if (applyOn >= MAX_APPLY_NUM || applyOn < 1) {
                metin2::log::error("Apply type {:d} is out of range", applyOn);
                return 0;
            }

            if (ch->find_affect(AFFECT_QUEST_START_IDX, applyOn))
                return 0;

            auto value = (int32_t)lua_tonumber(lua, 2);
            auto duration = (int32_t)lua_tonumber(lua, 3);

            ch->add_affect(AFFECT_QUEST_START_IDX, a_apply_info[applyOn].point_type, value, 0, duration, 0, false);

            return 0;
        }

        auto affect_remove(lua_State *lua) -> int {
            quest_manager &q = quest_manager::instance();
            int iType;

            if (lua_isnumber(lua, 1)) {
                iType = (int)lua_tonumber(lua, 1);

                if (iType == 0)
                    iType = q.current_pc()->current_quest_index() + AFFECT_QUEST_START_IDX;
            } else
                iType = q.current_pc()->current_quest_index() + AFFECT_QUEST_START_IDX;

            q.current_character_ptr()->remove_affect(iType);

            return 0;
        }

        auto affect_remove_bad(lua_State *lua) -> int {
            character *ch = quest_manager::instance().current_character_ptr();
            ch->remove_bad_affect();
            return 0;
        }

        auto affect_remove_good(lua_State *lua) -> int {
            character *ch = quest_manager::instance().current_character_ptr();
            ch->remove_good_affect();
            return 0;
        }

        auto affect_add_hair(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3)) {
                metin2::log::error("Invalid arguments");
                return 0;
            }

            quest_manager &q = quest_manager::instance();

            auto applyOn = (uint8_t)lua_tonumber(lua, 1);

            character *ch = q.current_character_ptr();

            if (applyOn >= MAX_APPLY_NUM || applyOn < 1) {
                metin2::log::error("Apply type {:d} is out of range", applyOn);
                return 0;
            }

            auto value = (int32_t)lua_tonumber(lua, 2);
            auto duration = (int32_t)lua_tonumber(lua, 3);

            ch->add_affect(AFFECT_HAIR, a_apply_info[applyOn].point_type, value, 0, duration, 0, false);

            return 0;
        }

        auto affect_remove_hair(lua_State *lua) -> int {
            character *ch = quest_manager::instance().current_character_ptr();

            affect *pkAff = ch->find_affect(AFFECT_HAIR);

            if (pkAff != nullptr) {
                lua_pushnumber(lua, pkAff->duration);
                ch->remove_affect(pkAff);
            } else {
                lua_pushnumber(lua, 0);
            }

            return 1;
        }

        auto affect_get_apply_on(lua_State *lua) -> int {
            character *ch = quest_manager::instance().current_character_ptr();

            if (!lua_isnumber(lua, 1)) {
                metin2::log::error("Invalid arguments");
                return 0;
            }

            uint32_t affectType = lua_tonumber(lua, 1);

            affect *pkAff = ch->find_affect(affectType);

            if (pkAff != nullptr)
                lua_pushnumber(lua, pkAff->apply_on);
            else
                lua_pushnil(lua);

            return 1;
        }

        auto affect_add_collect(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3)) {
                metin2::log::error("Invalid arguments");
                return 0;
            }

            quest_manager &q = quest_manager::instance();

            auto applyOn = (uint8_t)lua_tonumber(lua, 1);

            character *ch = q.current_character_ptr();

            if (applyOn >= MAX_APPLY_NUM || applyOn < 1) {
                metin2::log::error("Apply type {:d} is out of range", applyOn);
                return 0;
            }

            auto value = (int32_t)lua_tonumber(lua, 2);
            auto duration = (int32_t)lua_tonumber(lua, 3);

            ch->add_affect(AFFECT_COLLECT, a_apply_info[applyOn].point_type, value, 0, duration, 0, false);

            return 0;
        }

        auto affect_add_collect_point(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3)) {
                metin2::log::error("Invalid arguments");
                return 0;
            }

            quest_manager &q = quest_manager::instance();

            auto point_type = (uint8_t)lua_tonumber(lua, 1);

            character *ch = q.current_character_ptr();

            if (point_type >= POINT_MAX_NUM || point_type < 1) {
                metin2::log::error("Point type {:d} is out of range", point_type);
                return 0;
            }

            auto value = (int32_t)lua_tonumber(lua, 2);
            auto duration = (int32_t)lua_tonumber(lua, 3);

            ch->add_affect(AFFECT_COLLECT, point_type, value, 0, duration, 0, false);

            return 0;
        }

        auto affect_remove_collect(lua_State *lua) -> int {
            character *ch = quest_manager::instance().current_character_ptr();

            if (ch != nullptr) {
                auto bApply = (uint8_t)lua_tonumber(lua, 1);

                if (bApply >= MAX_APPLY_NUM) return 0;

                bApply = a_apply_info[bApply].point_type;
                auto value = (int32_t)lua_tonumber(lua, 2);

                const std::list<affect *> &rList = ch->affect_container();
                const affect *pAffect = nullptr;

                for (auto iter = rList.begin(); iter != rList.end(); ++iter) {
                    pAffect = *iter;

                    if (pAffect->type == AFFECT_COLLECT) {
                        if (pAffect->apply_on == bApply && pAffect->apply_value == value) {
                            break;
                        }
                    }

                    pAffect = nullptr;
                }

                if (pAffect != nullptr) {
                    ch->remove_affect(const_cast<affect *>(pAffect));
                }
            }

            return 0;
        }

        auto affect_remove_all_collect(lua_State *lua) -> int {
            character *ch = quest_manager::instance().current_character_ptr();

            if (ch != nullptr) {
                ch->remove_affect(AFFECT_COLLECT);
            }

            return 0;
        }

        void register_affect_function_table() {
            luaL_reg affect_functions[] =
                {
                    {"add",                affect_add               },
                    {"remove",             affect_remove            },
                    {"remove_bad",         affect_remove_bad        },
                    {"remove_good",        affect_remove_good       },
                    {"add_hair",           affect_add_hair          },
                    {"remove_hair",        affect_remove_hair       },
                    {"add_collect",        affect_add_collect       },
                    {"add_collect_point",  affect_add_collect_point },
                    {"remove_collect",     affect_remove_collect    },
                    {"remove_all_collect", affect_remove_all_collect},
                    {"get_apply_on",       affect_get_apply_on      },

                    {nullptr,              nullptr                  }
            };

            quest_manager::instance().add_lua_function_table("affect", affect_functions);
        }
    };
}
