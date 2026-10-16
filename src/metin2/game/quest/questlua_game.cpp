#include <metin2/game/admin/cmd.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/questlua.h>
#include <metin2/game/quest/questmanager.h>

#include <metin2/core/utils.h>
#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>

#include <cstdint>

namespace metin2::game {

    namespace quest {
        auto game_set_event_flag(lua_State *lua) -> int {
            quest_manager &q = quest_manager::instance();

            if (lua_isstring(lua, 1) && lua_isnumber(lua, 2))
                q.request_set_event_flag(lua_tostring(lua, 1), (int)lua_tonumber(lua, 2));

            return 0;
        }

        auto game_get_event_flag(lua_State *lua) -> int {
            quest_manager &q = quest_manager::instance();

            if (lua_isstring(lua, 1))
                lua_pushnumber(lua, q.event_flag(lua_tostring(lua, 1)));
            else
                lua_pushnumber(lua, 0);

            return 1;
        }

        auto game_request_make_guild(lua_State *lua) -> int {
            quest_manager &q = quest_manager::instance();
            character *ch = q.current_character_ptr();
            desc *d = ch->get_desc();
            if (d) {
                // Never 0, which means "no request".
                uint32_t key = 0;
                while (key == 0)
                    key = secure_random_u32();

                ch->set_guild_creation_key(key);

                packet_gc_request_make_guild p;
                p.header = HEADER_GC_REQUEST_MAKE_GUILD;
                p.key = key;
                d->packet(&p, sizeof(p));
            }
            return 0;
        }

        auto game_get_safebox_level(lua_State *lua) -> int {
            quest_manager &q = quest_manager::instance();
            lua_pushnumber(lua, q.current_character_ptr()->safebox_size() / SAFEBOX_PAGE_SIZE);
            return 1;
        }

        auto game_set_safebox_level(lua_State *lua) -> int {
            quest_manager &q = quest_manager::instance();

            safebox_change_size_packet p;
            p.id = q.current_character_ptr()->get_desc()->get_account_table().id;
            p.size = (int)lua_tonumber(lua, -1);
            db_clientdesc->db_packet(HEADER_GD_SAFEBOX_CHANGE_SIZE, q.current_character_ptr()->get_desc()->handle(), &p, sizeof(p));

            q.current_character_ptr()->set_safebox_size(SAFEBOX_PAGE_SIZE * (int)lua_tonumber(lua, -1));
            return 0;
        }

        auto game_open_safebox(lua_State *) -> int {
            quest_manager &q = quest_manager::instance();
            character *ch = q.current_character_ptr();
            ch->set_safebox_open_position();
            ch->chat_packet(CHAT_TYPE_COMMAND, "ShowMeSafeboxPassword");
            return 0;
        }

        auto game_open_mall(lua_State *) -> int {
            quest_manager &q = quest_manager::instance();
            character *ch = q.current_character_ptr();
            ch->set_safebox_open_position();
            ch->chat_packet(CHAT_TYPE_COMMAND, "ShowMeMallPassword");
            return 0;
        }

        auto game_drop_item(lua_State *lua) -> int {
            character *ch = quest_manager::instance().current_character_ptr();

            uint32_t item_vnum = (uint32_t)lua_tonumber(lua, 1);
            int count = (int)lua_tonumber(lua, 2);
            int32_t x = ch->x();
            int32_t y = ch->y();

            item *item = item_manager::instance().create_item(item_vnum, count);

            if (!item) {
                quest::error("Cannot create item (vnum {:d}, count {:d})", item_vnum, count);
                return 0;
            }

            pixel_position pos;
            pos.x = x + number(-200, 200);
            pos.y = y + number(-200, 200);

            item->add_to_ground(ch->map_index(), pos);
            item->start_destroy_event();

            return 0;
        }

        auto game_drop_item_with_ownership(lua_State *lua) -> int {
            character *ch = quest_manager::instance().current_character_ptr();

            item *item = nullptr;
            switch (lua_gettop(lua)) {
            case 1:
                item = item_manager::instance().create_item((uint32_t)lua_tonumber(lua, 1));
                break;
            case 2:
            case 3:
                item = item_manager::instance().create_item((uint32_t)lua_tonumber(lua, 1), (int)lua_tonumber(lua, 2));
                break;
            default:
                return 0;
            }

            if (item == nullptr) {
                return 0;
            }

            if (lua_isnumber(lua, 3)) {
                int sec = (int)lua_tonumber(lua, 3);
                if (sec <= 0) {
                    item->set_ownership(ch);
                } else {
                    item->set_ownership(ch, sec);
                }
            } else
                item->set_ownership(ch);

            pixel_position pos;
            pos.x = ch->x() + number(-200, 200);
            pos.y = ch->y() + number(-200, 200);

            item->add_to_ground(ch->map_index(), pos);
            item->start_destroy_event();

            return 0;
        }

        auto game_web_mall(lua_State *lua) -> int {
            character *ch = quest_manager::instance().current_character_ptr();

            if (ch != nullptr) {
                do_in_game_mall(ch, const_cast<char *>(""), 0, 0);
            }
            return 0;
        }

        void register_game_function_table() {
            luaL_reg game_functions[] =
                {
                    {"get_safebox_level",        game_get_safebox_level       },
                    {"request_make_guild",       game_request_make_guild      },
                    {"set_safebox_level",        game_set_safebox_level       },
                    {"open_safebox",             game_open_safebox            },
                    {"open_mall",                game_open_mall               },
                    {"get_event_flag",           game_get_event_flag          },
                    {"set_event_flag",           game_set_event_flag          },
                    {"drop_item",                game_drop_item               },
                    {"drop_item_with_ownership", game_drop_item_with_ownership},
                    {"open_web_mall",            game_web_mall                },

                    {nullptr,                    nullptr                      }
            };

            quest_manager::instance().add_lua_function_table("game", game_functions);
        }
    }
}
