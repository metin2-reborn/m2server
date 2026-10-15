#include <metin2/game/world/regen.h>

#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/app/config.h>
#include <metin2/game/world/dungeon.h>
#include <metin2/game/app/event.h>
#include <metin2/game/characters/mob_manager.h>
#include <metin2/game/world/sectree_manager.h>

#include <metin2/proto/length.h>
#include <metin2/common/utils.h>
#include <metin2/log/log.h>
#include <metin2/core/utils.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Push item to the front of a doubly linked list.
#define INSERT_TO_TW_LIST(item, head, prev, next)	\
	if (!(head))					\
	{						\
		head = item;				\
		(head)->prev = (head)->next = nullptr;	\
	}						\
	else						\
	{						\
		(head)->prev = item;			\
		(item)->next = head;			\
		(item)->prev = nullptr;			\
		head = item;				\
	}

namespace metin2::game::inline world {

regen *	regen_list = nullptr;
regen_exception * regen_exception_list = nullptr;

enum regen_modes
{
	MODE_TYPE = 0,
	MODE_SX,
	MODE_SY,
	MODE_EX,
	MODE_EY,
	MODE_Z_SECTION,
	MODE_DIRECTION,
	MODE_REGEN_TIME,
	MODE_REGEN_PERCENT,
	MODE_MAX_COUNT,
	MODE_VNUM
};

static bool get_word(FILE *fp, char *buf) // Read word by word.
{
	int i = 0;
	int c;

	int semicolon_mode = 0;

	while ((c = fgetc(fp)) != EOF)
	{
		if (i == 0)
		{
			if (c == '"')
			{
				semicolon_mode = 1;
				continue;
			}

			if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
				continue;
		}

		if (semicolon_mode)
		{
			if (c == '"')
			{
				buf[i] = '\0';
				return true;
			}

			buf[i++] = c;
		}
		else
		{
			if ((c == ' ' || c == '\t' || c == '\n' || c == '\r'))
			{
				buf[i] = '\0';
				return true;
			}

			buf[i++] = c;
		}

		if (i == 2 && buf[0] == '/' && buf[1] == '/')
		{
			buf[i] = '\0';
			return true;
		}
	}

	buf[i] = '\0';
	return (i != 0);
}

static void next_line(FILE *fp)
{
	int c;

	while ((c = fgetc(fp)) != EOF)
		if (c == '\n')
			return;
}

static bool read_line(FILE *fp, regen * regen)
{
	char szTmp[256];

	int mode = MODE_TYPE;
	int tmpTime;
	uint32_t i;

	while (get_word(fp, szTmp))
	{
		if (!strncmp(szTmp, "//", 2))
		{
			next_line(fp);
			continue;
		}

		switch (mode)
		{
			case MODE_TYPE:
				if (szTmp[0] == 'm')
					regen->type = REGEN_TYPE_MOB;
				else if (szTmp[0] == 'g')
				{
					regen->type = REGEN_TYPE_GROUP;

					if (szTmp[1] == 'a')
						regen->is_aggressive = true;
				}
				else if (szTmp[0] == 'e')
					regen->type = REGEN_TYPE_EXCEPTION;
				else if (szTmp[0] == 'r')
					regen->type = REGEN_TYPE_GROUP_GROUP;
				else if (szTmp[0] == 's')
					regen->type = REGEN_TYPE_ANYWHERE;
				else
				{
					metin2::log::error("read_line: unknown regen type {:c}", szTmp[0]);
					exit(1);
				}

				++mode;
				break;

			case MODE_SX:
				str_to_number(regen->sx, szTmp);
				++mode;
				break;

			case MODE_SY:
				str_to_number(regen->sy, szTmp);
				++mode;
				break;

			case MODE_EX:
				{
					int iX = 0;
					str_to_number(iX, szTmp);

					regen->sx -= iX;
					regen->ex = regen->sx + iX * 2;

					regen->sx *= 100;
					regen->ex *= 100;

					++mode;
				}
				break;

			case MODE_EY:
				{
					int iY = 0;
					str_to_number(iY, szTmp);

					regen->sy -= iY;
					regen->ey = regen->sy + iY * 2;

					regen->sy *= 100;
					regen->ey *= 100;

					++mode;
				}
				break;

			case MODE_Z_SECTION:
				str_to_number(regen->z_section, szTmp);

				// Bail out on exception.
				if (regen->type == REGEN_TYPE_EXCEPTION)
					return true;

				++mode;
				break;

			case MODE_DIRECTION:
				str_to_number(regen->direction, szTmp);
				++mode;
				break;

			case MODE_REGEN_TIME:
				regen->time = 0;
				tmpTime = 0;

				for (i = 0; i < strlen(szTmp); ++i)
				{
					switch (szTmp[i])
					{
						case 'h':
							regen->time += tmpTime * 3600;
							tmpTime = 0;
							break;

						case 'm':
							regen->time += tmpTime * 60;
							tmpTime = 0;
							break;

						case 's':
							regen->time += tmpTime;
							tmpTime = 0;
							break;

						default:
							if (szTmp[i] >= '0' && szTmp[i] <= '9')
							{
								tmpTime *= 10;
								tmpTime += (szTmp[i] - '0');
							}
					}
				}

				++mode;
				break;

			case MODE_REGEN_PERCENT:
				++mode;
				break;

			case MODE_MAX_COUNT:
				regen->count = 0;
				str_to_number(regen->max_count, szTmp);
				++mode;
				break;

			case MODE_VNUM:
				str_to_number(regen->vnum, szTmp);
				++mode;
				return true;
		}
	}

	return false;
}

static void regen_spawn_dungeon(regen * regen, dungeon * pDungeon, bool bOnce)
{
	uint32_t	num;
	uint32_t	i;

	num = (regen->max_count - regen->count);

	if (!num)
		return;

	for (i = 0; i < num; ++i)
	{
		character * ch = nullptr;

		if (regen->type == REGEN_TYPE_ANYWHERE)
		{
			ch = character_manager::instance().spawn_mob_random_position(regen->vnum, regen->map_index);

			if (ch)
			{
				++regen->count;
				ch->set_dungeon(pDungeon);
			}
		}
		else if (regen->sx == regen->ex && regen->sy == regen->ey)
		{
			ch = character_manager::instance().spawn_mob(regen->vnum,
					regen->map_index,
					regen->sx,
					regen->sy,
					regen->z_section,
					false,
					regen->direction == 0 ? number(0, 7) * 45 : (regen->direction - 1) * 45);

			if (ch)
			{
				++regen->count;
				ch->set_dungeon(pDungeon);
			}
		}
		else
		{
			if (regen->type == REGEN_TYPE_MOB)
			{
				ch = character_manager::instance().spawn_mob_range(regen->vnum, regen->map_index, regen->sx, regen->sy, regen->ex, regen->ey, true);

				if (ch)
				{
					++regen->count;
					ch->set_dungeon(pDungeon);
				}
			}
			else if (regen->type == REGEN_TYPE_GROUP)
			{
				if (character_manager::instance().spawn_group(regen->vnum, regen->map_index, regen->sx, regen->sy, regen->ex, regen->ey, bOnce ? nullptr : regen, regen->is_aggressive, pDungeon))
					++regen->count;
			}
			else if (regen->type == REGEN_TYPE_GROUP_GROUP)
			{
				if (character_manager::instance().spawn_group_group(regen->vnum, regen->map_index, regen->sx, regen->sy, regen->ex, regen->ey, bOnce ? nullptr : regen, regen->is_aggressive, pDungeon))
					++regen->count;
			}
		}

		if (ch && !bOnce)
			ch->set_regen(regen);
	}
}

static void regen_spawn(regen * regen, bool bOnce)
{
	uint32_t	num;
	uint32_t	i;

	num = (regen->max_count - regen->count);

	if (!num)
		return;

	for (i = 0; i < num; ++i)
	{
		character * ch = nullptr;

		if (regen->type == REGEN_TYPE_ANYWHERE)
		{
			ch = character_manager::instance().spawn_mob_random_position(regen->vnum, regen->map_index);

			if (ch)
				++regen->count;
		}
		else if (regen->sx == regen->ex && regen->sy == regen->ey)
		{
			ch = character_manager::instance().spawn_mob(regen->vnum,
					regen->map_index,
					regen->sx,
					regen->sy,
					regen->z_section,
					false,
					regen->direction == 0 ? number(0, 7) * 45 : (regen->direction - 1) * 45);

			if (ch)
				++regen->count;
		}
		else
		{
			if (regen->type == REGEN_TYPE_MOB)
			{
				ch = character_manager::instance().spawn_mob_range(regen->vnum, regen->map_index, regen->sx, regen->sy, regen->ex, regen->ey, true, regen->is_aggressive, regen->is_aggressive );

				if (ch)
					++regen->count;
			}
			else if (regen->type == REGEN_TYPE_GROUP)
			{
				if (character_manager::instance().spawn_group(regen->vnum, regen->map_index, regen->sx, regen->sy, regen->ex, regen->ey, bOnce ? nullptr : regen, regen->is_aggressive))
					++regen->count;
			}
			else if (regen->type == REGEN_TYPE_GROUP_GROUP)
			{
				if (character_manager::instance().spawn_group_group(regen->vnum, regen->map_index, regen->sx, regen->sy, regen->ex, regen->ey, bOnce ? nullptr : regen, regen->is_aggressive))
					++regen->count;
			}
		}

		if (ch && !bOnce)
			ch->set_regen(regen);
	}
}

EVENTFUNC(dungeon_regen_event)
{
	dungeon_regen_event_info* info = dynamic_cast<dungeon_regen_event_info*>( event->info );

	if ( info == nullptr )
	{
		metin2::log::error( "dungeon_regen_event> <Factor> Null pointer" );
		return 0;
	}

	dungeon * pDungeon = dungeon_manager::instance().find(info->dungeon_id);
	if (pDungeon == nullptr) {
		return 0;
	}

	regen *	regen = info->regen;
	if (regen->time == 0)
	{
		regen->event = nullptr;
	}

	regen_spawn_dungeon(regen, pDungeon, false);
	return passes_per_sec(regen->time);
}

bool regen_do(const char* filename, int32_t lMapIndex, int base_x, int base_y, dungeon * pDungeon, bool bOnce)
{
	if (metin2::config::game().no_regen)
		return true;

	if ( lMapIndex >= 114 && lMapIndex <= 117 )
		return true;

	regen * regen = nullptr;
	FILE* fp = fopen(filename, "rt");

	if (nullptr == fp)
	{
		metin2::log::error("SYSTEM: regen_do: {}: file not found", filename);
		return false;
	}

	while (true)
	{
		game::world::regen tmp;

		if (!read_line(fp, &tmp))
			break;

		if (tmp.type == REGEN_TYPE_MOB ||
			tmp.type == REGEN_TYPE_GROUP ||
			tmp.type == REGEN_TYPE_GROUP_GROUP ||
			tmp.type == REGEN_TYPE_ANYWHERE)
		{
			if (!bOnce)
			{
				regen = new game::world::regen(tmp);
			}
			else
				regen = &tmp;

			if (pDungeon)
				regen->is_aggressive = true;

			regen->map_index = lMapIndex;
			regen->count = 0;

			regen->sx += base_x;
			regen->ex += base_x;

			regen->sy += base_y;
			regen->ey += base_y;

			if (regen->sx > regen->ex)
			{
				regen->sx ^= regen->ex;
				regen->ex ^= regen->sx;
				regen->sx ^= regen->ex;
			}

			if (regen->sy > regen->ey)
			{
				regen->sy ^= regen->ey;
				regen->ey ^= regen->sy;
				regen->sy ^= regen->ey;
			}

			if (regen->type == REGEN_TYPE_MOB)
			{
				const mob * p = mob_manager::instance().get(regen->vnum);

				if (!p)
				{
					metin2::log::error("No mob data by vnum {:d}", regen->vnum);
					if (!bOnce) {
						delete regen;
					}
					continue;
				}
			}

			if (!bOnce && pDungeon != nullptr)
			{
				dungeon_regen_event_info* info = alloc_event_info<dungeon_regen_event_info>();

				info->regen = regen;
				info->dungeon_id = pDungeon->id();

				regen->event = event_create(dungeon_regen_event, info, passes_per_sec(number(0, 16)) + passes_per_sec(regen->time));

				pDungeon->add_regen(regen);
				// regen_id should be determined at this point,
				// before the call to CHARACTER::SetRegen()
			}

			// Always regen the first time.
			regen_spawn_dungeon(regen, pDungeon, bOnce);

		}
	}

	fclose(fp);
	return true;
}

bool regen_load_in_file(const char* filename, int32_t lMapIndex, int base_x, int base_y)
{
	if (metin2::config::game().no_regen)
		return true;

	regen * regen = nullptr;
	FILE * fp = fopen(filename, "rt");

	if (nullptr == fp)
	{
		metin2::log::error("SYSTEM: regen_do: {}: file not found", filename);
		return false;
	}

	while (true)
	{
		game::world::regen tmp;

		if (!read_line(fp, &tmp))
			break;

		if (tmp.type == REGEN_TYPE_MOB ||
			tmp.type == REGEN_TYPE_GROUP ||
			tmp.type == REGEN_TYPE_GROUP_GROUP ||
			tmp.type == REGEN_TYPE_ANYWHERE)
		{
			regen = &tmp;

			regen->is_aggressive = true;

			regen->map_index = lMapIndex;
			regen->count = 0;

			regen->sx += base_x;
			regen->ex += base_x;

			regen->sy += base_y;
			regen->ey += base_y;

			if (regen->sx > regen->ex)
			{
				regen->sx ^= regen->ex;
				regen->ex ^= regen->sx;
				regen->sx ^= regen->ex;
			}

			if (regen->sy > regen->ey)
			{
				regen->sy ^= regen->ey;
				regen->ey ^= regen->sy;
				regen->sy ^= regen->ey;
			}

			if (regen->type == REGEN_TYPE_MOB)
			{
				const mob * p = mob_manager::instance().get(regen->vnum);

				if (!p)
				{
					metin2::log::error("No mob data by vnum {:d}", regen->vnum);
					continue;
				}
			}

			// Always regen the first time.
			regen_spawn(regen, true);
		}
	}

	fclose(fp);
	return true;
}

EVENTFUNC(regen_event)
{
	regen_event_info* info = dynamic_cast<regen_event_info*>( event->info );

	if ( info == nullptr )
	{
		metin2::log::error( "regen_event> <Factor> Null pointer" );
		return 0;
	}

	regen *	regen = info->regen;

	if (regen->time == 0)
		regen->event = nullptr;

	regen_spawn(regen, false);
	return passes_per_sec(regen->time);
}

bool regen_load(const char* filename, int32_t lMapIndex, int base_x, int base_y)
{
	if (metin2::config::game().no_regen)
		return true;

	regen * regen = nullptr;
	FILE* fp = fopen(filename, "rt");

	if (nullptr == fp)
	{
		metin2::log::info( "SYSTEM: regen_load: {}: file not found", filename);
		return false;
	}

	while (true)
	{
		game::world::regen tmp;

		if (!read_line(fp, &tmp))
			break;

		if (tmp.type == REGEN_TYPE_MOB ||
			tmp.type == REGEN_TYPE_GROUP ||
			tmp.type == REGEN_TYPE_GROUP_GROUP ||
			tmp.type == REGEN_TYPE_ANYWHERE)
		{
			if (metin2::config::test_server())
			{
				mob_manager::instance().inc_regen_count(tmp.type, tmp.vnum, tmp.max_count, tmp.time);
			}

			regen = new game::world::regen(tmp);
			INSERT_TO_TW_LIST(regen, regen_list, prev, next);

			regen->map_index = lMapIndex;
			regen->count = 0;

			regen->sx += base_x;
			regen->ex += base_x;

			regen->sy += base_y;
			regen->ey += base_y;

			if (regen->sx > regen->ex)
			{
				regen->sx ^= regen->ex;
				regen->ex ^= regen->sx;
				regen->sx ^= regen->ex;
			}

			if (regen->sy > regen->ey)
			{
				regen->sy ^= regen->ey;
				regen->ey ^= regen->sy;
				regen->sy ^= regen->ey;
			}

			if (regen->type == REGEN_TYPE_MOB)
			{
				const mob * p = mob_manager::instance().get(regen->vnum);

				if (!p)
				{
					metin2::log::error("No mob data by vnum {:d}", regen->vnum);
				}
				else if (p->table.type == CHAR_TYPE_NPC || p->table.type == CHAR_TYPE_WARP || p->table.type == CHAR_TYPE_GOTO)
				{
					sectree_manager::instance().insert_npc_position(lMapIndex,
							p->table.type,
							p->table.locale_name,
							(regen->sx+regen->ex) / 2 - base_x,
							(regen->sy+regen->ey) / 2 - base_y);
				}
			}

			//NO_REGEN
			// Desc: 	When the regen time is set to 0 in regen.txt (and other regen-related text files) 
			// 			no regen is done.
			if (regen->time != 0)
			{
				// Always regen the first time.
				regen_spawn(regen, false);

				regen_event_info* info = alloc_event_info<regen_event_info>();

				info->regen = regen;

				regen->event = event_create(regen_event, info, passes_per_sec(number(0, 16)) + passes_per_sec(regen->time)); 
			}
			//END_NO_REGEN
		}
		else if (tmp.type == REGEN_TYPE_EXCEPTION)
		{
			regen_exception * exc;

			exc = new regen_exception;

			exc->sx = tmp.sx;
			exc->sy = tmp.sy;
			exc->ex = tmp.ex;
			exc->ey = tmp.ey;
			exc->z_section = tmp.z_section;

			INSERT_TO_TW_LIST(exc, regen_exception_list, prev, next);
		}
	}

	fclose(fp);
	return true;
}

void regen_free(void)
{
	regen * r;
	regen * next_regen;
	regen_exception * exc;
	regen_exception * next_exc;

	for (r = regen_list; r; r = next_regen)
	{
		next_regen = r->next;

		event_cancel(&r->event);
		delete r;
	}

	regen_list = nullptr;

	for (exc = regen_exception_list; exc; exc = next_exc)
	{
		next_exc = exc->next;

		delete exc;
	}

	regen_exception_list = nullptr;
}

void regen_reset(int x, int y)
{
	regen * regen;

	for (regen = regen_list; regen; regen = regen->next)
	{
		if (!regen->event)
			continue;

		// If coordinates are given, regen only the regen list within those coordinates.
		if (x != 0 || y != 0)
		{
			if (x >= regen->sx && x <= regen->ex)
				if (y >= regen->sy && y <= regen->ey)
					event_reset_time(regen->event, 1);
		}
		// Otherwise regen everything
		else
			event_reset_time(regen->event, 1);
	}
}
}
