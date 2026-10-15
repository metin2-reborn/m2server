#include <metin2/game/characters/character.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/items/over9refine.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/app/utils.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>

#include <cstdint>

namespace metin2::game {


namespace quest
{

	int item_get_cell(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();

		if (q.current_item())
		{
			lua_pushnumber(lua, q.current_item()->cell());
		}
		else
			lua_pushnumber(lua, 0);
		return 1;
	}

	int item_select_cell(lua_State* lua)
	{
		lua_pushboolean(lua, 0);
		if (!lua_isnumber(lua, 1))
		{
			return 1;
		}
		uint32_t cell = (uint32_t) lua_tonumber(lua, 1);

		character * ch = quest_manager::instance().current_character_ptr();
		item * item = ch ? ch->inventory_item(cell) : nullptr;

		if (!item)
		{
			return 1;
		}

		quest_manager::instance().set_current_item(item);
		lua_pushboolean(lua, 1);

		return 1;
	}

	int item_select(lua_State* lua)
	{
		lua_pushboolean(lua, 0);
		if (!lua_isnumber(lua, 1))
		{
			return 1;
		}
		uint32_t id = (uint32_t) lua_tonumber(lua, 1);
		item * item = item_manager::instance().find(id);

		if (!item)
		{
			return 1;
		}

		quest_manager::instance().set_current_item(item);
		lua_pushboolean(lua, 1);

		return 1;
	}

	int item_get_id(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();

		if (q.current_item())
		{
			lua_pushnumber(lua, q.current_item()->id());
		}
		else
			lua_pushnumber(lua, 0);
		return 1;
	}

	int item_remove(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();
		item * item = q.current_item();
		if (item != nullptr) {
			if (q.current_character_ptr() == item->owner()) {
				item_manager::instance().remove_item(item);
			} else {
				quest::error("Tried to remove invalid item {}", static_cast<const void *>(item));
			}
			q.clear_current_item();
		}

		return 0;
	}

	int item_get_socket(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();
		if (q.current_item() && lua_isnumber(lua, 1))
		{
			int idx = (int) lua_tonumber(lua, 1);
			if (idx < 0 || idx >= ITEM_SOCKET_MAX_NUM)
				lua_pushnumber(lua,0);
			else
				lua_pushnumber(lua, q.current_item()->get_socket(idx));
		}
		else
		{
			lua_pushnumber(lua,0);
		}
		return 1;
	}

	int item_set_socket(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();
		if (q.current_item() && lua_isnumber(lua,1) && lua_isnumber(lua,2))
		{
			int idx = (int) lua_tonumber(lua, 1);
			int value = (int) lua_tonumber(lua, 2);
			if (idx >=0 && idx < ITEM_SOCKET_MAX_NUM)
				q.current_item()->set_socket(idx, value);
		}
		return 0;
	}

	int item_get_vnum(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();
		item * item = q.current_item();

		if (item) 
			lua_pushnumber(lua, item->vnum());
		else
			lua_pushnumber(lua, 0);

		return 1;
	}

	int item_has_flag(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();
		item * item = q.current_item();

		if (!lua_isnumber(lua, 1))
		{
			quest::error("flag is not a number.");
			lua_pushboolean(lua, 0);
			return 1;
		}

		if (!item)
		{
			lua_pushboolean(lua, 0);
			return 1;
		}

		int32_t lCheckFlag = (int32_t) lua_tonumber(lua, 1);
		lua_pushboolean(lua, is_set(item->flag(), lCheckFlag));

		return 1;
	}

	int item_get_value(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();
		item * item = q.current_item();

		if (!item)
		{
			lua_pushnumber(lua, 0);
			return 1;
		}

		if (!lua_isnumber(lua, 1))
		{
			quest::error("index is not a number");
			lua_pushnumber(lua, 0);
			return 1;
		}

		int index = (int) lua_tonumber(lua, 1);

		if (index < 0 || index >= ITEM_VALUES_MAX_NUM)
		{
			quest::error("index({:d}) is out of range (0..{:d})", index, ITEM_VALUES_MAX_NUM);
			lua_pushnumber(lua, 0);
		}
		else
			lua_pushnumber(lua, item->value(index));

		return 1;
	}

	int item_set_value(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();
		item * item = q.current_item();

		if (!item)
		{
			lua_pushnumber(lua, 0);
			return 1;
		}

		if (false == (lua_isnumber(lua, 1) && lua_isnumber(lua, 2) && lua_isnumber(lua, 3)))
		{
			quest::error("index is not a number");
			lua_pushnumber(lua, 0);
			return 1;
		}

		item->set_force_attribute(
			lua_tonumber(lua, 1),
			lua_tonumber(lua, 2),
			lua_tonumber(lua, 3)
		);

		return 0;
	}

	int item_get_name(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();
		item * item = q.current_item();

		if (item)
			lua_pushstring(lua, item->name());
		else
			lua_pushstring(lua, "");

		return 1;
	}

	int item_get_size(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();
		item * item = q.current_item();

		if (item)
			lua_pushnumber(lua, item->size());
		else
			lua_pushnumber(lua, 0);

		return 1;
	}

	int item_get_count(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();
		item * item = q.current_item();

		if (item)
			lua_pushnumber(lua, item->count());
		else
			lua_pushnumber(lua, 0);

		return 1;
	}

	int item_get_type(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();
		item * item = q.current_item();

		if (item)
			lua_pushnumber(lua, item->type());
		else
			lua_pushnumber(lua, 0);

		return 1;
	}

	int item_get_sub_type(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();
		item * item = q.current_item();

		if (item)
			lua_pushnumber(lua, item->sub_type());
		else
			lua_pushnumber(lua, 0);

		return 1;
	}

	int item_get_refine_vnum(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();
		item * item = q.current_item();

		if (item)
			lua_pushnumber(lua, item->refined_vnum());
		else
			lua_pushnumber(lua, 0);

		return 1;
	}

	int item_next_refine_vnum(lua_State* lua)
	{
		uint32_t vnum = 0;
		if (lua_isnumber(lua, 1))
			vnum = (uint32_t) lua_tonumber(lua, 1);

		item_table* pTable = item_manager::instance().table(vnum);
		if (pTable)
		{
			lua_pushnumber(lua, pTable->refined_vnum);
		}
		else
		{
			quest::error("Cannot find item table of vnum {:d}", vnum);
			lua_pushnumber(lua, 0);
		}
		return 1;
	}

	int item_get_level(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();
		item * item = q.current_item();

		if (item)
			lua_pushnumber(lua, item->refine_level());
		else
			lua_pushnumber(lua, 0);

		return 1;
	}

	int item_can_over9refine(lua_State* lua)
	{
		item * item = quest_manager::instance().current_item();

		if ( item == nullptr ) return 0;

		lua_pushnumber(lua, over9_refine_manager::instance().can_over9_refine(item->vnum()));

		return 1;
	}

	int item_change_to_over9(lua_State* lua)
	{
		character * ch = quest_manager::instance().current_character_ptr();
		item * item = quest_manager::instance().current_item();

		if ( ch == nullptr || item == nullptr ) return 0;

		lua_pushboolean(lua, over9_refine_manager::instance().change9_to_over9(ch, item));
		
		return 1;
	}
	
	int item_over9refine(lua_State* lua)
	{
		character * ch = quest_manager::instance().current_character_ptr();
		item * item = quest_manager::instance().current_item();

		if ( ch == nullptr || item == nullptr ) return 0;

		lua_pushboolean(lua, over9_refine_manager::instance().over9_refine(ch, item));
		
		return 1;
	}

	int item_get_over9_material_vnum(lua_State* lua)
	{
		if ( lua_isnumber(lua, 1) == true )
		{
			lua_pushnumber(lua, over9_refine_manager::instance().material_vnum((uint32_t)lua_tonumber(lua, 1)));
			return 1;
		}

		return 0;
	}

	int item_get_level_limit (lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();

		if (q.current_item())
		{
			if (q.current_item()->type() != ITEM_WEAPON && q.current_item()->type() != ITEM_ARMOR)
			{
				return 0;
			}
			lua_pushnumber(lua, q.current_item() -> level_limit());
			return 1;
		}
		return 0;
	}

	int item_start_realtime_expire(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();
		item * pItem = q.current_item();

		if (pItem)
		{
			pItem->start_real_time_expire_event();
			return 1;
		}

		return 0;
	}
	
	int item_copy_and_give_before_remove(lua_State* lua)
	{
		lua_pushboolean(lua, 0);
		if (!lua_isnumber(lua, 1))
			return 1;

		uint32_t vnum = (uint32_t)lua_tonumber(lua, 1);

		quest_manager& q = quest_manager::instance();
		item * pItem = q.current_item();
		character * pChar = q.current_character_ptr();

		item * pkNewItem = item_manager::instance().create_item(vnum, 1, 0, false);

		if (pkNewItem)
		{
			item_manager::copy_all_attr_to(pItem, pkNewItem);
			log_manager::instance().item_log(pChar, pkNewItem, "COPY SUCCESS", pkNewItem->name());

			uint8_t bCell = pItem->cell();

			item_manager::instance().remove_item(pItem, "REMOVE (COPY SUCCESS)");

			pkNewItem->add_to_character(pChar, item_pos(INVENTORY, bCell)); 
			item_manager::instance().flush_delayed_save(pkNewItem);
			pkNewItem->attr_log();

			lua_pushboolean(lua, 1);			
		}

		return 1;
	}

	void register_item_function_table()
	{

		luaL_reg item_functions[] =
		{
			{ "get_id",		item_get_id		},
			{ "get_cell",		item_get_cell		},
			{ "select",		item_select		},
			{ "select_cell",	item_select_cell	},
			{ "remove",		item_remove		},
			{ "get_socket",		item_get_socket		},
			{ "set_socket",		item_set_socket		},
			{ "get_vnum",		item_get_vnum		},
			{ "has_flag",		item_has_flag		},
			{ "get_value",		item_get_value		},
			{ "set_value",		item_set_value		},
			{ "get_name",		item_get_name		},
			{ "get_size",		item_get_size		},
			{ "get_count",		item_get_count		},
			{ "get_type",		item_get_type		},
			{ "get_sub_type",	item_get_sub_type	},
			{ "get_refine_vnum",	item_get_refine_vnum	},
			{ "get_level",		item_get_level		},
			{ "next_refine_vnum",	item_next_refine_vnum	},
			{ "can_over9refine",	item_can_over9refine	},
			{ "change_to_over9",		item_change_to_over9	},
			{ "over9refine",		item_over9refine	},
			{ "get_over9_material_vnum",		item_get_over9_material_vnum	},
			{ "get_level_limit", 				item_get_level_limit },
			{ "start_realtime_expire", 			item_start_realtime_expire },
			{ "copy_and_give_before_remove",	item_copy_and_give_before_remove},

			{ nullptr,			nullptr			}
		};
		quest_manager::instance().add_lua_function_table("item", item_functions);
	}
}
}
