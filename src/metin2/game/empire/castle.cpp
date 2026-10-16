

#include <metin2/game/empire/castle.h>

#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/admin/cmd.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/event.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/persistence/log.h>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/empire/start_position.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>

#include <metin2/common/utils.h>
#include <metin2/log/log.h>
#include <metin2/core/utils.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace metin2::game::inline empire {

constexpr int EMPIRE_NONE = 0;
constexpr int EMPIRE_RED = 1;
constexpr int EMPIRE_YELLOW = 2;
constexpr int EMPIRE_BLUE = 3;


#define SIEGE_EVENT_PULSE	passes_per_sec(60*5)


#define GET_CASTLE(empire)								(castle+(empire))
#define GET_GUARD(empire, region_index, guard_index)	(GET_CASTLE(empire)->guard[region_index][guard_index])
#define GET_GUARD_REGION(empire, region_index)			(s_guard_region[empire][region_index])
#define GET_GUARD_GROUP(empire, region_index, guard_index)	(GET_CASTLE(empire)->guard_group[region_index][guard_index])
#define GET_FROG(empire, index)							(GET_CASTLE(empire)->frog[index])
#define GET_FROG_POS(empire, index)						(frog_pos[empire][index])
#define GET_TOWER(empire, index)							(GET_CASTLE(empire)->tower[index])

#define DO_ALL_EMPIRE(empire)	for (int empire = 1; empire < 4; ++empire)
#define DO_ALL_TOWER(i)			for (int i = 0; i < MAX_CASTLE_TOWER; ++i)
#define DO_ALL_FROG(i)			for (int i = 0; i < MAX_CASTLE_FROG; ++i)


#define GET_SIEGE_STATE()			siege_state
#define GET_SIEGE_EMPIRE()			sige_empire
#define GET_SIEGE_EVENT(empire)		(GET_CASTLE(empire)->siege_event)
#define GET_STONE_EVENT(empire)		(GET_CASTLE(empire)->stone_event)

#define GET_TOWER_REGION(empire)	tower_region[empire]


static castle_data	*castle		= nullptr;
static castle_state	siege_state	= CASTLE_SIEGE_NONE;
static int			sige_empire	= EMPIRE_NONE;


struct position
{
	int	x, y;
};

static position	frog_pos[4][MAX_CASTLE_FROG] = {
	{
		{ 0, 0 },
		{ 0, 0 },
		{ 0, 0 },
		{ 0, 0 },
		{ 0, 0 },

		{ 0, 0 },
		{ 0, 0 },
		{ 0, 0 },
		{ 0, 0 },
		{ 0, 0 },

		{ 0, 0 },
		{ 0, 0 },
		{ 0, 0 },
		{ 0, 0 },
		{ 0, 0 },

		{ 0, 0 },
		{ 0, 0 },
		{ 0, 0 },
		{ 0, 0 },
		{ 0, 0 }
	},
	{
		{ 225, 45 },
		{ 231, 45 },
		{ 237, 45 },
		{ 243, 45 },
		{ 249, 45 },

		{ 225, 50 },
		{ 231, 50 },
		{ 237, 50 },
		{ 243, 50 },
		{ 249, 50 },

		{ 261, 45 },
		{ 267, 45 },
		{ 273, 45 },
		{ 279, 45 },
		{ 285, 45 },

		{ 261, 50 },
		{ 267, 50 },
		{ 273, 50 },
		{ 279, 50 },
		{ 285, 50 }
	},
	{
		{ 221, 36 },
		{ 227, 36 },
		{ 233, 36 },
		{ 239, 36 },
		{ 245, 36 },

		{ 269, 36 },
		{ 275, 36 },
		{ 281, 36 },
		{ 287, 36 },
		{ 293, 36 },

		{ 221, 41 },
		{ 227, 41 },
		{ 233, 41 },
		{ 239, 41 },
		{ 245, 41 },

		{ 269, 41 },
		{ 275, 41 },
		{ 281, 41 },
		{ 287, 41 },
		{ 293, 41 }
	},
	{
		{ 225, 45 },
		{ 231, 45 },
		{ 237, 45 },
		{ 243, 45 },
		{ 249, 45 },

		{ 225, 50 },
		{ 231, 50 },
		{ 237, 50 },
		{ 243, 50 },
		{ 249, 50 },

		{ 261, 45 },
		{ 267, 45 },
		{ 273, 45 },
		{ 279, 45 },
		{ 285, 45 },

		{ 261, 50 },
		{ 267, 50 },
		{ 273, 50 },
		{ 279, 50 },
		{ 285, 50 }
	}
};


struct guard_region
{
	int	sx, sy, ex, ey;
};

static guard_region s_guard_region[4][4] = {
	{
		{ 0, 0, 0, 0 },
		{ 0, 0, 0, 0 },
		{ 0, 0, 0, 0 },
		{ 0, 0, 0, 0 }
	},
	{
		{ 74, 170, 96, 180 },
		{ 237, 135, 270, 146 },
		{ 235, 260, 278, 273 },
		{ 421, 167, 435, 205 }
	},
	{
		{ 109, 172, 128, 202 },
		{ 237, 140, 282, 153 },
		{ 232, 261, 279, 276 },
		{ 390, 173, 403, 205 },
	},
	{
		{ 74, 170, 96, 120 },
		{ 237, 135, 270, 146 },
		{ 235, 260, 278, 273 },
		{ 421, 167, 435, 205 }
	}
};

static guard_region tower_region[4] = {
	{ 0, 0, 0, 0 },
	{ 85, 135, 420, 265 },
	{ 120, 130, 405, 276 },
	{ 85, 135, 420, 265 }
};


static auto fn_castle_map_index(int empire) -> int32_t;

EVENTINFO(castle_event_info)
{
	int		empire;
	int		pulse;

	castle_event_info()
	: empire( 0 )
	, pulse( 0 )
	{
	}
};

EVENTFUNC(castle_siege_event)
{
	char	buf[1024] = {0};
	struct castle_event_info	*info = dynamic_cast<castle_event_info*>( event->info );

	if ( info == nullptr )
	{
		metin2::log::error( "Castle siege event has no info" );
		return 0;
	}

	info->pulse += SIEGE_EVENT_PULSE;

	if (info->pulse < passes_per_sec(30*60))
	{
		snprintf(buf, sizeof(buf), lc_text("%s에서 봉화를 둘러싸고 전투가 진행중입니다."),
				empire_name(GET_SIEGE_EMPIRE()));
		broadcast_notice(buf);

		return SIEGE_EVENT_PULSE;
	}

	switch (GET_SIEGE_STATE())
	{
		case CASTLE_SIEGE_NONE:
			break;

		case CASTLE_SIEGE_STRUGGLE:
			{
				snprintf(buf, sizeof(buf), lc_text("%s이 수성에 성공했습니다."), empire_name(GET_SIEGE_EMPIRE()));
				broadcast_notice(buf);

				snprintf(buf, sizeof(buf), lc_text("지금부터 %s은 30분간 봉화를 파괴하여 보상을 획득 할 수 있습니다."), empire_name(GET_SIEGE_EMPIRE()));
				broadcast_notice(buf);

				GET_SIEGE_STATE() = CASTLE_SIEGE_END;

				return passes_per_sec(60*30);
			}
			break;
		case CASTLE_SIEGE_END:
			broadcast_notice(lc_text("30분이 경과했습니다.. 봉화가 사라집니다."));
			castle_end_siege();
			break;
	}
	return 0;
}


static auto fn_random_stone() -> uint32_t
{
	uint32_t	vnum[7] = {
		8012,
		8013,
		8014,
		8024,
		8025,
		8026,
		8027
	};

	int	index = number(0, 6);

	return vnum[index];
}

EVENTINFO(castle_stone_event_info)
{
	int	empire;
	int	spawn_count;

	castle_stone_event_info()
	: empire( 0 )
	, spawn_count( 0 )
	{
	}
};


EVENTFUNC(castle_stone_event)
{
	struct castle_stone_event_info	*info = dynamic_cast<castle_stone_event_info*>( event->info );

	if (info == nullptr)
	{
		metin2::log::error( "Castle stone event has no info" );
		return 0;
	}

	int32_t	map_index	= fn_castle_map_index(GET_SIEGE_EMPIRE());

	sectree_map	*sectree_map = sectree_manager::instance().map(map_index);

	if (nullptr == sectree_map)
		return 0;

	const int SPAWN_COUNT = 15;

	if (info->spawn_count < (SPAWN_COUNT * 2))
	{
		for (int i = 0; i < SPAWN_COUNT; ++i)
		{
			uint32_t	sx = sectree_map->setting.base_x + 100 * GET_TOWER_REGION(info->empire).sx;
			uint32_t	sy = sectree_map->setting.base_y + 100 * GET_TOWER_REGION(info->empire).sy;
			uint32_t	ex = sectree_map->setting.base_x + 100 * GET_TOWER_REGION(info->empire).ex;
			uint32_t	ey = sectree_map->setting.base_y + 100 * GET_TOWER_REGION(info->empire).ey;

			character_manager::instance().spawn_mob_range(fn_random_stone(),
														fn_castle_map_index(info->empire),
														sx, sy, ex, ey);
		}

		info->spawn_count += SPAWN_COUNT;

		if (info->spawn_count < (SPAWN_COUNT * 2))
			return passes_per_sec(30 * 60);
		else
			return 0;
	}

	return 0;
}



auto castle_spawn_frog_force(int empire, int empty_index) -> character *;

static auto fn_castle_map_index(int empire) -> int32_t
{
	switch (empire)
	{
		case EMPIRE_RED:	return 181;
		case EMPIRE_YELLOW:	return 183;
		case EMPIRE_BLUE:	return 182;
	}
	return 0;
}

static auto fn_empty_frog_index(int empire) -> int
{
	DO_ALL_FROG(i)
	{
		if (nullptr == GET_FROG(empire, i))
			return i;
	}
	return (-1);
}

static auto fn_empty_frog_pos(int empire) -> position*
{
	int	frog_index = fn_empty_frog_index(empire);

	if (frog_index < 0)
		return nullptr;

	switch (empire)
	{
		case EMPIRE_RED:
		case EMPIRE_YELLOW:
		case EMPIRE_BLUE:
			return &GET_FROG_POS(empire, frog_index);
	}

	return nullptr;
}

static auto fn_empty_guard_pos(int empire, int region_index) -> int
{
	for (int i = 0; i < MAX_CASTLE_GUARD_PER_REGION; ++i)
	{
		if (nullptr == GET_GUARD(empire, region_index, i))
		{
			return i;
		}
	}

	return -1;
}

static auto fn_is_castle_map(int map_index) -> bool
{
	switch (map_index)
	{
		case 181:
		case 182:
		case 183:
			return true;
	}
	return false;
}


auto castle_boot() -> bool
{
	FILE	*fp;
	char	one_line[256];
	const char	*delim			= " \t\r\n";
	char	*v;
	int		empire			= 0;
	int		guard_region	= 0;

	castle = new castle_data[4]();

	const char	*castle_file = "castle_data.txt";

	if ((fp = fopen(castle_file, "r")) == nullptr)
		return false;

	while (fgets(one_line, 256, fp))
	{
		int value = 0;

		if (one_line[0] == '#')
			continue;

		const char* token_string = strtok(one_line, delim);

		if (nullptr == token_string)
			continue;

		TOKEN("section")
		{
			continue;
		}
		else TOKEN("empire")
		{
			v = strtok(nullptr, delim);
			if (v)
			{
				str_to_number(empire, v);
			}
			else
			{
				fclose(fp);
				metin2::log::error("Castle file {} has a line without an empire", castle_file);
				return false;
			}
		}
		else TOKEN("frog")
		{
			int	pos = 0;

			while ((v = strtok(nullptr, delim)))
			{
				str_to_number(value, v);
				if (value)
				{
					castle_spawn_frog_force(empire, pos);
				}
				++pos;
			}
		}
		else TOKEN("guard")
		{
			int	group_vnum		= 0;

			while ((v = strtok(nullptr, delim)))
			{
				str_to_number(group_vnum, v);
				if (group_vnum)
				{
					castle_spawn_guard(empire, group_vnum, guard_region);
				}
			}

			++guard_region;
		}
		else TOKEN("end")
		{
			guard_region = 0;
		}
	}

	fclose(fp);

	return true;
}

void castle_save()
{
	if (nullptr == castle)
		return;

	const char	*castle_file = "castle_data.txt";
	FILE		*fp;

	fp = fopen(castle_file, "w");

	if (nullptr == fp)
	{
		metin2::log::error("Cannot open castle file {} for writing", castle_file);
		return;
	}

	DO_ALL_EMPIRE(empire)
	{
		fprintf(fp, "section\n");

		fprintf(fp, "\tempire %d\n", empire);

		fprintf(fp, "\tfrog ");
		for (int i = 0; i < MAX_CASTLE_FROG; ++i)
		{
			fprintf(fp, " %d", GET_FROG(empire, i) ? 1 : 0);
		}
		fprintf(fp, "\n");

		for (int region_index = 0; region_index < MAX_CASTLE_GUARD_REGION; ++region_index)
		{
			fprintf(fp, "\tguard ");
			for (int guard_index = 0; guard_index < MAX_CASTLE_GUARD_PER_REGION; ++guard_index)
			{
				fprintf(fp, " %u", GET_GUARD_GROUP(empire, region_index, guard_index));
			}
			fprintf(fp, "\n");
		}
		fprintf(fp, "end\n");
	}

	fclose(fp);
}

auto castle_siege(int empire, int tower_count) -> int
{
	{
		if (nullptr == sectree_manager::instance().map(181)) return 0;
		if (nullptr == sectree_manager::instance().map(182)) return 0;
		if (nullptr == sectree_manager::instance().map(183)) return 0;
	}

	switch (GET_SIEGE_STATE())
	{
		case CASTLE_SIEGE_NONE:
			castle_start_siege(empire, tower_count);
			return 1;
			break;

		case CASTLE_SIEGE_STRUGGLE:
		case CASTLE_SIEGE_END:
			castle_end_siege();
			return 2;
			break;
	}

	return 0;
}

void castle_start_siege(int empire, int tower_count)
{
	if (CASTLE_SIEGE_NONE != GET_SIEGE_STATE())
		return;

	GET_SIEGE_STATE()	= CASTLE_SIEGE_STRUGGLE;
	GET_SIEGE_EMPIRE()	= empire;

	castle_spawn_tower(empire, tower_count);

	{
		castle_event_info* info = alloc_event_info<castle_event_info>();

		info->empire = empire;
		info->pulse	= 0;

		GET_SIEGE_EVENT(empire) = event_create(castle_siege_event, info, SIEGE_EVENT_PULSE);
	}

	{
		castle_stone_event_info* info = alloc_event_info<castle_stone_event_info>();

		info->spawn_count = 0;
		info->empire = empire;

		GET_STONE_EVENT(empire) = event_create(castle_stone_event, info, passes_per_sec(1));
	}
}

void castle_end_siege()
{
	GET_SIEGE_EMPIRE()	= EMPIRE_NONE;
	GET_SIEGE_STATE()	= CASTLE_SIEGE_NONE;

	DO_ALL_EMPIRE(empire)
	{
		if (GET_SIEGE_EVENT(empire))
		{
			event_cancel(&GET_SIEGE_EVENT(empire));
		}

		DO_ALL_TOWER(i)
		{
			if (GET_TOWER(empire, i))
			{
				character *	npc = GET_TOWER(empire, i);
				character_manager::instance().destroy_character(npc);
				GET_TOWER(empire, i) = nullptr;
			}
		}
	}
}


auto castle_spawn_frog(int empire) -> character *
{
	int		dir = 1;
	int32_t	map_index	= fn_castle_map_index(empire);

	position	*empty_pos = fn_empty_frog_pos(empire);
	if (nullptr == empty_pos)
		return nullptr;

	sectree_map	*sectree_map = sectree_manager::instance().map(map_index);
	if (nullptr == sectree_map)
		return nullptr;
	uint32_t x = sectree_map->setting.base_x + 100*empty_pos->x;
	uint32_t y = sectree_map->setting.base_y + 100*empty_pos->y;

	character * frog = character_manager::instance().spawn_mob(CASTLE_FROG_VNUM, map_index,
															x, y, 0 ,
															false, dir);
	if (frog)
	{
		frog->set_empire(empire);
		int empty_index	= fn_empty_frog_index(empire);
		GET_FROG(empire, empty_index) = frog;
		return frog;
	}
	return nullptr;
}

auto castle_spawn_frog_force(int empire, int empty_index) -> character *
{
	int		dir = 1;
	int32_t	map_index	= fn_castle_map_index(empire);

	position	*empty_pos = &GET_FROG_POS(empire, empty_index);

	sectree_map	*sectree_map = sectree_manager::instance().map(map_index);
	if (nullptr == sectree_map)
	{
		return nullptr;
	}
	uint32_t x = sectree_map->setting.base_x + 100*empty_pos->x;
	uint32_t y = sectree_map->setting.base_y + 100*empty_pos->y;

	character * frog = character_manager::instance().spawn_mob(CASTLE_FROG_VNUM, map_index,
															x, y, 0 ,
															false, dir);
	if (frog)
	{
		frog->set_empire(empire);
		GET_FROG(empire, empty_index) = frog;
		return frog;
	}
	return nullptr;
}


auto castle_spawn_guard(int empire, uint32_t group_vnum, int region_index) -> character *
{
	character *	mob;
	int	sx, sy, ex, ey;
	int32_t	map_index	= fn_castle_map_index(empire);

	sectree_map	*sectree_map = sectree_manager::instance().map(map_index);
	if (nullptr == sectree_map)
		return nullptr;

	if (castle_guard_count(empire, region_index) >= MAX_CASTLE_GUARD_PER_REGION)
		return nullptr;

	sx = sectree_map->setting.base_x + 100*GET_GUARD_REGION(empire, region_index).sx;
	sy = sectree_map->setting.base_y + 100*GET_GUARD_REGION(empire, region_index).sy;
	ex = sectree_map->setting.base_x + 100*GET_GUARD_REGION(empire, region_index).ex;
	ey = sectree_map->setting.base_y + 100*GET_GUARD_REGION(empire, region_index).ey;

	mob = character_manager::instance().spawn_group(group_vnum, map_index,
													sx, sy, ex, ey);
	if (mob)
	{
		mob->set_empire(empire);

		int	pos = fn_empty_guard_pos(empire, region_index);
		GET_GUARD(empire, region_index, pos) = mob;
		GET_GUARD_GROUP(empire, region_index, pos) = group_vnum;
	}

	return mob;
}


static auto fn_random_tower() -> uint32_t
{
	uint32_t vnum[5] =
	{
		11506,
		11507,
		11508,
		11509,
		11510
	};

	int index = number(0, 4);
	return vnum[index];
}

static void fn_spawn_tower(int empire, sectree_map * sectree_map)
{
	DO_ALL_TOWER(i)
	{
		if (GET_TOWER(empire, i))
			continue;

		uint32_t	sx = sectree_map->setting.base_x + 100 * GET_TOWER_REGION(empire).sx;
		uint32_t	sy = sectree_map->setting.base_y + 100 * GET_TOWER_REGION(empire).sy;
		uint32_t	ex = sectree_map->setting.base_x + 100 * GET_TOWER_REGION(empire).ex;
		uint32_t	ey = sectree_map->setting.base_y + 100 * GET_TOWER_REGION(empire).ey;

		GET_TOWER(empire, i) =
			character_manager::instance().spawn_mob_range(fn_random_tower(),
														fn_castle_map_index(empire),
														sx, sy, ex, ey);
		GET_TOWER(empire, i)->set_empire(empire);
		return;
	}
}

auto castle_spawn_tower(int empire, int tower_count) -> bool
{
	int	map_index = fn_castle_map_index(empire);
	sectree_map	*sectree_map = sectree_manager::instance().map(map_index);
	if (nullptr == sectree_map)
		return false;

	DO_ALL_TOWER(i)
	{
		if (GET_TOWER(empire, i))
				GET_TOWER(empire, i)->dead(nullptr, true);
		GET_TOWER(empire, i) = nullptr;
	}

	int	spawn_count = std::clamp(tower_count, MIN_CASTLE_TOWER, MAX_CASTLE_TOWER);

	for (int j = 0; j < spawn_count; ++j)
	{
		fn_spawn_tower(empire, sectree_map);
	}

	{
		char buf[1024];
		snprintf(buf, sizeof(buf), lc_text("%s에 전쟁의 시작을 알리는 봉화가 나타났습니다."), empire_name(empire));
		broadcast_notice(buf);
	}
	return true;
}

void castle_guard_die(character * ch, character * killer)
{
	int	empire = ch->empire();

	for (int region_index = 0; region_index < MAX_CASTLE_GUARD_REGION; ++region_index)
	{
		for (int i = 0; i < MAX_CASTLE_GUARD_PER_REGION; ++i)
		{
			if (GET_GUARD(empire, region_index, i) == ch)
			{
				GET_GUARD(empire, region_index, i) = nullptr;
				GET_GUARD_GROUP(empire, region_index, i) = 0;
			}
		}
	}

	castle_save();
}


void castle_frog_die(character * ch, character * killer)
{
	if (nullptr == ch || nullptr == killer)
		return;

	int	empire = ch->empire();

	DO_ALL_FROG(i)
	{
		if (ch == GET_FROG(empire, i))
		{
			GET_FROG(empire, i) = nullptr;

			killer->point_change(POINT_GOLD, 10000000 , true);
			castle_save();
			return;
		}
	}
}

void castle_tower_die(character * ch, character * killer)
{
	char	buf[1024] = {0};

	if (nullptr == ch || nullptr == killer)
		return;

	int	killer_empire = killer->empire();

	switch (GET_SIEGE_STATE())
	{
		case CASTLE_SIEGE_NONE:
			break;

		case CASTLE_SIEGE_STRUGGLE:
		case CASTLE_SIEGE_END:
			{
				int	siege_end = true;
				snprintf(buf, sizeof(buf), lc_text("%s이 봉화를 파괴했습니다."), empire_name(killer_empire));
				broadcast_notice(buf);

				log_manager::instance().char_log(killer, 0, "CASTLE_TORCH_KILL", "");

				DO_ALL_TOWER(i)
				{
					if (ch == GET_TOWER(GET_SIEGE_EMPIRE(), i))
						GET_TOWER(GET_SIEGE_EMPIRE(), i) = nullptr;
				}

				DO_ALL_TOWER(i)
				{
					if (GET_TOWER(GET_SIEGE_EMPIRE(), i))
						siege_end = false;
				}

				if (siege_end)
				{
					if (GET_SIEGE_STATE() == CASTLE_SIEGE_STRUGGLE)
					{
						snprintf(buf, sizeof(buf), lc_text("%s이 수성에 실패하여 전쟁에 패배하였습니다.."), empire_name(GET_SIEGE_EMPIRE()));
						broadcast_notice(buf);
					}
					else
					{
						snprintf(buf, sizeof(buf), lc_text("%s이 모든 봉화를 파괴하였습니다."), empire_name(GET_SIEGE_EMPIRE()));
						broadcast_notice(buf);
					}
					castle_end_siege();
				}
			}
			break;
	}
}


auto castle_guard_count(int empire, int region_index) -> int
{
	int count = 0;

	for (int i = 0; i < MAX_CASTLE_GUARD_PER_REGION; ++i)
	{
		if ( GET_GUARD(empire, region_index, i) )
			++count;
	}
	return count;
}


auto castle_is_guard_vnum(uint32_t vnum) -> bool
{
	switch (vnum)
	{
		case 11112:
		case 11114:
		case 11116:
		case 11106:
		case 11108:
		case 11110:
		case 11100:
		case 11102:
		case 11104:
		case 11113:
		case 11115:
		case 11117:
		case 11107:
		case 11109:
		case 11111:
		case 11101:
		case 11103:
		case 11105:
			return true;
	}

	return false;
}

auto castle_cost_of_hiring_guard(uint32_t group_vnum) -> int
{
	switch (group_vnum)
	{
		case 9501:
		case 9511:
		case 9521:

		case 9502:
		case 9512:
		case 9522:
			return (100*10000);

		case 9503:
		case 9513:
		case 9523:

		case 9504:
		case 9514:
		case 9524:
			return (300*10000);

		case 9505:
		case 9515:
		case 9525:

		case 9506:
		case 9516:
		case 9526:
			return (1000*10000);
	}

	return 0;
}

auto castle_can_attack(character * ch, character * victim) -> bool
{
	if (nullptr == ch || nullptr == victim)
		return false;

	if (false == fn_is_castle_map(ch->map_index()))
		return false;

	if (ch->is_pc() && victim->is_pc())
		return true;

	if (CASTLE_SIEGE_END == GET_SIEGE_STATE())
	{
		if (castle_is_tower_vnum(victim->race_num()))
		{
			if (ch->empire() == victim->empire())
				return true;
			else
				return false;
		}
	}

	if (ch->empire() == victim->empire())
		return false;

	return true;
}

auto castle_is_my_castle(int empire, int map_index) -> bool
{
	switch (empire)
	{
		case EMPIRE_RED:	return (181 == map_index);
		case EMPIRE_YELLOW: return (183 == map_index);
		case EMPIRE_BLUE:	return (182 == map_index);
	}
	return false;
}

auto castle_is_tower_vnum(uint32_t vnum) -> bool
{
	switch (vnum)
	{
		case 11506:
		case 11507:
		case 11508:
		case 11509:
		case 11510:
			return true;
	}
	return false;
}
}
