#include <metin2/game/events/fishing.h>

#include <metin2/game/characters/affect.h>
#include <metin2/game/characters/char.h>
#include <metin2/game/characters/char_manager.h>
#include <metin2/game/admin/cmd.h>
#include <metin2/game/app/config.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/app/event.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/text/locale_service.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/items/unique_item.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/common/utils.h>
#include <metin2/log/log.h>
#include <metin2/core/utils.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <numbers>
#include <string>

namespace metin2::game::inline events {

namespace fishing
{
	enum
	{
		MAX_FISH = 37,
		NUM_USE_RESULT_COUNT = 10, // 1 : DEAD 2 : BONE 3 ~ 12 : rest
		FISH_BONE_VNUM = 27799,
		SHELLFISH_VNUM = 27987,
		EARTHWORM_VNUM = 27801,
		WATER_STONE_VNUM_END = 28043,
		FISH_NAME_MAX_LEN = 64,
		MAX_PROB = 4,
	};

	enum
	{
		USED_NONE,
		USED_SHELLFISH,
		USED_WATER_STONE,
		USED_TREASURE_MAP,
		USED_EARTHWARM,
	};

	enum
	{
		FISHING_TIME_NORMAL,
		FISHING_TIME_SLOW,
		FISHING_TIME_QUICK,
		FISHING_TIME_ALL,
		FISHING_TIME_EASY,

		FISHING_TIME_COUNT,

		MAX_FISHING_TIME_COUNT = 31,
	};

	int fishing_time[FISHING_TIME_COUNT][MAX_FISHING_TIME_COUNT] =
	{
		{   0,   0,   0,   0,   0,   2,   4,   8,  12,  16,  20,  22,  25,  30,  50,  80,  50,  30,  25,  22,  20,  16,  12,   8,   4,   2,   0,   0,   0,   0,   0 },
		{   0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   4,   8,  12,  16,  20,  22,  25,  30,  50,  80,  50,  30,  25,  22,  20 },
		{  20,  22,  25,  30,  50,  80,  50,  30,  25,  22,  20,  16,  12,   8,   4,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0 },
		{ 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100 },
		{  20,  20,  20,  20,  20,  22,  24,  28,  32,  36,  40,  42,  45,  50,  70, 100,  70,  50,  45,  42,  40,  36,  32,  28,  24,  22,  20,  20,  20,  20,  20 },
	};

	struct fish_info_data
	{
		char name[FISH_NAME_MAX_LEN];

		uint32_t vnum;
		uint32_t dead_vnum;
		uint32_t grill_vnum;

		int prob[MAX_PROB];
		int difficulty;

		int time_type;
		int length_range[3]; // MIN MAX EXTRA_MAX : 99% MIN~MAX, 1% MAX~EXTRA_MAX

		int used_table[NUM_USE_RESULT_COUNT];
		// 6000 2000 1000 500 300 100 50 30 10 5 4 1
	};

	int prob_sum[MAX_PROB];
	int prob_accumulate[MAX_PROB][MAX_FISH];

	fish_info_data fish_info[MAX_FISH] = {};
	/*
	   {
	   { "꽝",		00000, 00000, 00000, {  750, 1750, 2750 },   10, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_NORMAL, { 0,           }, 
	   {0, } },
	   { "금반지",	50002, 00000, 00000, {   50,   50,    0 },  200, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_NORMAL, { 0,           }, 
	   {0, } },
	   { "피라미",	27802, 00000, 00000, { 2100, 1500,   50 },   10, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_EASY,   {500, 550, 600}, 
	   {0, } },
	   { "붕어",	27803, 27833, 27863, { 2100, 1500,  100 },   13, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_EASY,   {1000,2500,2800},
	   {USED_NONE, USED_SHELLFISH, USED_NONE, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "쏘가리",	27804, 27834, 27864, { 1100, 1300,  150 },   16, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_NORMAL, {2000,3500,3800},
	   {USED_NONE, USED_SHELLFISH, USED_NONE, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "월척붕어",	27805, 27835, 27865, { 1100, 1100,  200 },   20, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_SLOW, {3030,3500,4300},
	   {USED_NONE, USED_SHELLFISH, USED_NONE, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "잉어",	27806, 27836, 27866, { 1100,  500,  300 },   26, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_NORMAL, {4000,6000,10000},
	   {USED_NONE, USED_SHELLFISH, USED_NONE, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "연어",	27807, 27837, 27867, { 1100,  450,  400 },   33, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_NORMAL,{6000,8000,10000},
	   {USED_NONE, USED_SHELLFISH, USED_NONE, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "향어",	27808, 27838, 27868, {  200,  400,  500 },   42, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_NORMAL,{1500,3000,3800},
	   {USED_NONE, USED_SHELLFISH, USED_NONE, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "송어",	27809, 27839, 27869, {  200,  300,  700 },   54, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_NORMAL,{5000,7000,8000},
	   {USED_NONE, USED_SHELLFISH, USED_NONE, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "민물장어",	27810, 27840, 27870, {    0,  270, 1000 },   70, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_NORMAL,{4000,5000,6000},
	   {USED_SHELLFISH, USED_NONE, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_NONE, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "무지개송어",	27811, 27841, 27871, {    0,  200, 1000 },   91, FISHING_LIMIT_APPEAR,	{  0,   0,   0}, FISHING_TIME_NORMAL,{5000,7000,8000},
	   {USED_SHELLFISH, USED_NONE, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_NONE, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "강송어",	27812, 27842, 27872, {    0,  160, 1000 },  118, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_QUICK,{4000,6000,7000},
	   {USED_SHELLFISH, USED_NONE, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_NONE, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "러드",	27813, 27843, 27873, {    0,  130,  700 },  153, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_NORMAL,{4000,6000,10000},
	   {USED_SHELLFISH, USED_NONE, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_NONE, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "퍼치",	27814, 27844, 27874, {    0,  100,  400 },  198, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_NORMAL,{3000,4000,5000},
	   {USED_SHELLFISH, USED_NONE, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_NONE, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "텐치",	27815, 27845, 27875, {    0,   50,  300 },  257, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_NORMAL,{3500,5500,8000},
	   {USED_SHELLFISH, USED_NONE, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_NONE, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "메기",	27816, 27846, 27876, {    0,   30,  100 },  334, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_NORMAL,{3000,5000,10000},
	   {USED_SHELLFISH, USED_NONE, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_NONE, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "미꾸라지",	27817, 27847, 27877, {    0,   10,   64 },  434, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_QUICK,{1800,2200,3000},
	   {USED_SHELLFISH, USED_NONE, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_NONE, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "백련",	27818, 27848, 27878, {    0,    0,   15 },  564, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_NORMAL,{5000,8000,10000},
	   {USED_SHELLFISH, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_NONE, USED_NONE, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "은어",	27819, 27849, 27879, {    0,    0,    9 },  733, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_NORMAL,{1500,3000,3800},
	   {USED_SHELLFISH, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_NONE, USED_NONE, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "빙어",	27820, 27850, 27880, {    0,    0,    6 },  952, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_QUICK,{1500,3000,3800},
	   {USED_SHELLFISH, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_NONE, USED_NONE, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "쉬리",	27821, 27851, 27881, {    0,    0,    3 }, 1237, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_NORMAL,{1000,1500,2000},
	   {USED_SHELLFISH, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_NONE, USED_NONE, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "비늘잉어",	27822, 27852, 27882, {    0,    0,    2 }, 1608, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_SLOW,{4000,6000,10000},
	   {USED_SHELLFISH, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_NONE, USED_NONE, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "황금붕어",	27823, 27853, 27883, {    0,    0,    1 }, 2090, FISHING_LIMIT_NONE,	{  0,   0,   0}, FISHING_TIME_SLOW,{1000,3000,3500},
	   {USED_SHELLFISH, USED_NONE, USED_WATER_STONE, USED_TREASURE_MAP, USED_NONE, USED_NONE, USED_EARTHWARM, USED_NONE,USED_NONE,  USED_NONE } },
	   { "탈색약",     70201, 00000, 00000, { 5,    5,  0 },   60, FISHING_LIMIT_NONE,    {  0,   0,   0}, FISHING_TIME_NORMAL, {0,           },
	   {0,	}},
	   { "염색약(흰색)",  70202, 00000, 00000, { 15,  15,  0 },   60, FISHING_LIMIT_NONE,    {  0,   0,   0}, FISHING_TIME_NORMAL, {0,           },
	   {0,	}},
	   { "염색약(금색)",  70203, 00000, 00000, { 15,  15,  0 },   60, FISHING_LIMIT_NONE,    {  0,   0,   0}, FISHING_TIME_NORMAL, {0,           },
	   {0,	}},
	   { "염색약(빨간색)",70204, 00000, 00000, { 15,  15,  0 },   60, FISHING_LIMIT_NONE,    {  0,   0,   0}, FISHING_TIME_NORMAL, {0,           },
	   {0,	}},
	   { "염색약(갈색)",  70205, 00000, 00000, { 15,  15,  0 },   60, FISHING_LIMIT_NONE,    {  0,   0,   0}, FISHING_TIME_NORMAL, {0,           },
	   {0,	}},
	   { "염색약(검은색)",70206, 00000, 00000, { 15,  15,  0 },   60, FISHING_LIMIT_NONE,    {  0,   0,   0}, FISHING_TIME_NORMAL, {0,           },
	   {0,	}},
	   { "은둔자의 망토", 70048, 00000, 00000, {  8,   8,  0 },   60, FISHING_LIMIT_NONE,    {  0,   0,   0}, FISHING_TIME_NORMAL, {0,           },
	   {0,	}},
	   { "행운의 반지",   70049, 00000, 00000, {  8,   8,  0 },   60, FISHING_LIMIT_NONE,    {  0,   0,   0}, FISHING_TIME_NORMAL, {0,           },
	   {0,	}},
	   { "선왕의 증표",   70050, 00000, 00000, {  8,   8,  0 },   60, FISHING_LIMIT_NONE,    {  0,   0,   0}, FISHING_TIME_NORMAL, {0,           },
	   {0,	}},
	   { "선왕의 장갑",   70051, 00000, 00000, {  8,   8,  0 },   60, FISHING_LIMIT_NONE,    {  0,   0,   0}, FISHING_TIME_NORMAL, {0,           },
	   {0,	}},
	   { "금덩어리",	   80008, 00000, 00000, { 20,  20,  0 },  250, FISHING_LIMIT_NONE,    {  0,   0,   0}, FISHING_TIME_SLOW,    {0,           },
	   {0, } },
	{ "은열쇠",	   50009, 00000, 00000, {300, 300, 0, },   70, FISHING_LIMIT_NONE,    { 0, 0, 0}, FISHING_TIME_NORMAL, {0,	}, {0, } },

	{ "금열쇠",	   50008, 00000, 00000, {110, 110, 0, },  100, FISHING_LIMIT_NONE,    { 0, 0, 0}, FISHING_TIME_NORMAL, {0,	}, {0, } },
};
	*/
void initialize()
{
	fish_info_data fish_info_bak[MAX_FISH];
	std::memcpy(fish_info_bak, fish_info, sizeof(fish_info));

	memset(fish_info, 0, sizeof(fish_info));


	// LOCALE_SERVICE
	const int FILE_NAME_LEN = 256;
	char szFishingFileName[FILE_NAME_LEN+1];
	snprintf(szFishingFileName, sizeof(szFishingFileName),
			"%s/fishing.txt", locale_service_get_base_path().c_str());
	FILE * fp = fopen(szFishingFileName, "r");
	// END_OF_LOCALE_SERVICE

	if (*fish_info_bak[0].name)
		send_log("Reloading fish table.");

	if (!fp)
	{
		send_log("error! cannot open fishing.txt");

		// If the name is in the backup, restore it.
		if (*fish_info_bak[0].name)
		{
			std::memcpy(fish_info, fish_info_bak, sizeof(fish_info));
			send_log("  restoring to backup");
		}
		return;
	}

	memset(fish_info, 0, sizeof(fish_info));

	char buf[512];
	int idx = 0;

	while (fgets(buf, 512, fp))
	{
		if (*buf == '#')
			continue;

		char * p = strrchr(buf, '\n');
		*p = '\0';

		const char * start = buf;
		const char * tab = strchr(start, '\t');

		if (!tab)
		{
			printf("Tab error on line: %s\n", buf);
			send_log("error! parsing fishing.txt");

			if (*fish_info_bak[0].name)
			{
				std::memcpy(fish_info, fish_info_bak, sizeof(fish_info));
				send_log("  restoring to backup");
			}
			break;
		}

		char szCol[256], szCol2[256];
		int iColCount = 0;

		do
		{
			strlcpy(szCol2, start, std::min<int>(sizeof(szCol2), (tab - start) + 1));
			szCol2[tab-start] = '\0';

			trim_and_lower(szCol2, szCol, sizeof(szCol));

			if (!*szCol || *szCol == '\t')
				iColCount++;
			else 
			{
				switch (iColCount++)
				{
					case 0: strlcpy(fish_info[idx].name, szCol, sizeof(fish_info[idx].name)); break;
					case 1: str_to_number(fish_info[idx].vnum, szCol); break;
					case 2: str_to_number(fish_info[idx].dead_vnum, szCol); break;
					case 3: str_to_number(fish_info[idx].grill_vnum, szCol); break;
					case 4: str_to_number(fish_info[idx].prob[0], szCol); break;
					case 5: str_to_number(fish_info[idx].prob[1], szCol); break;
					case 6: str_to_number(fish_info[idx].prob[2], szCol); break;
					case 7: str_to_number(fish_info[idx].prob[3], szCol); break;
					case 8: str_to_number(fish_info[idx].difficulty, szCol); break;
					case 9: str_to_number(fish_info[idx].time_type, szCol); break;
					case 10: str_to_number(fish_info[idx].length_range[0], szCol); break;
					case 11: str_to_number(fish_info[idx].length_range[1], szCol); break;
					case 12: str_to_number(fish_info[idx].length_range[2], szCol); break;
					case 13: // 0
					case 14: // 1
					case 15: // 2
					case 16: // 3
					case 17: // 4
					case 18: // 5
					case 19: // 6
					case 20: // 7
					case 21: // 8
					case 22: // 9
							 str_to_number(fish_info[idx].used_table[iColCount-1-12], szCol);
							 break;
				}
			}

			start = tab + 1;
			tab = strchr(start, '\t');
		} while (tab);

		idx++;

		if (idx == MAX_FISH)
			break;
	}

	fclose(fp);

	for (int i = 0; i < MAX_FISH; ++i)
	{
		metin2::log::info( "FISH: {:<24} vnum {:5d} prob {:4d} {:4d} {:4d} {:4d} len {:d} {:d} {:d}",
				fish_info[i].name,
				fish_info[i].vnum,
				fish_info[i].prob[0],
				fish_info[i].prob[1],
				fish_info[i].prob[2],
				fish_info[i].prob[3],
				fish_info[i].length_range[0],
				fish_info[i].length_range[1],
				fish_info[i].length_range[2]);
	}

	// Compute probability
	for (int j = 0; j < MAX_PROB; ++j)
	{
		prob_accumulate[j][0] = fish_info[0].prob[j];

		for (int i = 1; i < MAX_FISH; ++i)
			prob_accumulate[j][i] = fish_info[i].prob[j] + prob_accumulate[j][i - 1];

		prob_sum[j] = prob_accumulate[j][MAX_FISH - 1];
		metin2::log::info( "FISH: prob table {:d} {:d}", j, prob_sum[j]);
	}
}

int determine_fish_by_prob_index(int prob_idx)
{
	int rv = number(1, prob_sum[prob_idx]);
	int * p = std::lower_bound(prob_accumulate[prob_idx], prob_accumulate[prob_idx]+ MAX_FISH, rv);
	int fish_idx = p - prob_accumulate[prob_idx];
	return fish_idx;
}

int get_prob_index_by_map_index(int index)
{
	if (index > 60)
		return -1;

	switch (index)
	{
		case 1:
		case 21:
		case 41:
			return 0;

		case 3:
		case 23:
		case 43:
			return 1;
	}

	return -1;
}

int determine_fish(character * ch)
{
	int map_idx = ch->map_index();
	int prob_idx = get_prob_index_by_map_index(map_idx);

	if (prob_idx < 0) 
		return 0;

	// ADD_PREMIUM
	if (ch->premium_remain_seconds(PREMIUM_FISH_MIND) > 0 ||
			ch->is_equip_unique_group(UNIQUE_GROUP_FISH_MIND)) // Higher chance of high-grade fish while wearing the monthly fish-mind item
	{
		if (quest::quest_manager::instance().event_flag("manwoo") != 0)
			prob_idx = 3;
		else
			prob_idx = 2;
	}
	// END_OF_ADD_PREMIUM

	int adjust = 0;
	if (quest::quest_manager::instance().event_flag("fish_miss_pct") != 0)
	{
		int fish_pct_value = std::clamp(quest::quest_manager::instance().event_flag("fish_miss_pct"), 0, 200);
		adjust = (100-fish_pct_value) * fish_info[0].prob[prob_idx] / 100;
	}

	int rv = number(adjust + 1, prob_sum[prob_idx]);

	int * p = std::lower_bound(prob_accumulate[prob_idx], prob_accumulate[prob_idx] + MAX_FISH, rv);
	int fish_idx = p - prob_accumulate[prob_idx];


	uint32_t vnum = fish_info[fish_idx].vnum;

	if (vnum == 50008 || vnum == 50009 || vnum == 80008) 
		return 0;

	return (fish_idx);
}

void fishing_react(character * ch)
{
	packet_gc_fishing p;
	p.header = HEADER_GC_FISHING;
	p.subheader = FISHING_SUBHEADER_GC_REACT;
	p.info = ch->get_vid();
	ch->packet_around(&p, sizeof(p));
}

void fishing_success(character * ch)
{
	packet_gc_fishing p;
	p.header = HEADER_GC_FISHING;
	p.subheader = FISHING_SUBHEADER_GC_SUCCESS;
	p.info = ch->get_vid();
	ch->packet_around(&p, sizeof(p));
}

void fishing_fail(character * ch)
{
	packet_gc_fishing p;
	p.header = HEADER_GC_FISHING;
	p.subheader = FISHING_SUBHEADER_GC_FAIL;
	p.info = ch->get_vid();
	ch->packet_around(&p, sizeof(p));
}

void fishing_practice(character * ch)
{
	item * rod = ch->wear(WEAR_WEAPON);
	if (rod && rod->type() == ITEM_ROD)
	{
		// Train the fishing rod if not at max mastery
		if ( rod->refined_vnum()>0 && rod->get_socket(0) < rod->value(2) && number(1,rod->value(1))==1 )
		{
			rod->set_socket(0, rod->get_socket(0) + 1);
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("낚시대의 수련도가 증가하였습니다! (%d/%d)"),rod->get_socket(0), rod->value(2));
			if (rod->get_socket(0) == rod->value(2))
			{
				ch->chat_packet(CHAT_TYPE_INFO, lc_text("낚시대가 최대 수련도에 도달하였습니다."));
				ch->chat_packet(CHAT_TYPE_INFO, lc_text("어부를 통해 다음 레벨의 낚시대로 업그레이드 할 수 있습니다."));
			}
		}
	}
	// Remove the bait
	rod->set_socket(2, 0);
}

bool predict_fish(character * ch)
{
	// ADD_PREMIUM
	// Fish-mind pill
	if (ch->find_affect(AFFECT_FISH_MIND_PILL) || 
			ch->premium_remain_seconds(PREMIUM_FISH_MIND) > 0 ||
			ch->is_equip_unique_group(UNIQUE_GROUP_FISH_MIND))
		return true;
	// END_OF_ADD_PREMIUM

	return false;
}

EVENTFUNC(fishing_event)
{
	fishing_event_info * info = dynamic_cast<fishing_event_info *>( event->info );

	if ( info == nullptr )
	{
		metin2::log::error( "fishing_event> <Factor> Null pointer" );
		return 0;
	}

	character * ch = character_manager::instance().find_by_pid(info->pid);

	if (!ch)
		return 0;

	item * rod = ch->wear(WEAR_WEAPON);

	if (!(rod && rod->type() == ITEM_ROD))
	{
		ch->fishing_event = nullptr;
		return 0;
	}

	switch (info->step)
	{
		case 0:	// Just a nibble, or only the bait flies off
			++info->step;

			info->hang_time = get_dword_time();
			info->fish_id = determine_fish(ch);
			fishing_react(ch);

			if (predict_fish(ch))
			{
				packet_gc_fishing p;
				p.header	= HEADER_GC_FISHING;
				p.subheader	= FISHING_SUBHEADER_GC_FISH;
				p.info	= fish_info[info->fish_id].vnum;
				ch->get_desc()->packet(&p, sizeof(packet_gc_fishing));
			}
			return (passes_per_sec(6));

		default:
			++info->step;

			if (info->step > 5)
				info->step = 5;

			ch->fishing_event = nullptr;
			fishing_fail(ch);
			rod->set_socket(2, 0);
			return 0;
	}
}

event_ptr create_fishing_event(character * ch)
{
	fishing_event_info* info = alloc_event_info<fishing_event_info>();
	info->pid	= ch->player_id();
	info->step	= 0;
	info->hang_time	= 0;

	int time = number(10, 40);

	packet_gc_fishing p;
	p.header	= HEADER_GC_FISHING;
	p.subheader	= FISHING_SUBHEADER_GC_START;
	p.info		= ch->get_vid();
	p.dir		= (uint8_t)(ch->rotation()/5);
	ch->packet_around(&p, sizeof(packet_gc_fishing));

	return event_create(fishing_event, info, passes_per_sec(time));
}

int get_fishing_level(character * ch)
{
	item * rod = ch->wear(WEAR_WEAPON);

	if (!rod || rod->type()!= ITEM_ROD)
		return 0;

	return rod->get_socket(2) + rod->value(0);
}

int compute(uint32_t fish_id, uint32_t ms, uint32_t* item, int level)
{
	if (fish_id == 0)
		return -2;

	if (fish_id >= MAX_FISH)
	{
		metin2::log::error("Wrong FISH ID : {:d}", fish_id);
		return -2; 
	}

	if (ms > 6000)
		return -1;

	int time_step = std::clamp<int>(((ms + 99) / 200), 0, MAX_FISHING_TIME_COUNT - 1);

	if (number(1, 100) <= fishing_time[fish_info[fish_id].time_type][time_step])
	{
		if (number(1, fish_info[fish_id].difficulty) <= level)
		{
			*item = fish_info[fish_id].vnum;
			return 0;
		}
		return -3;
	}

	return -1;
}

int get_fish_length(int fish_id)
{
	if (number(0,99))
	{
		// 99% : normal size
		return (int)(fish_info[fish_id].length_range[0] + 
				(fish_info[fish_id].length_range[1] - fish_info[fish_id].length_range[0])
				* (number(0,2000)+number(0,2000)+number(0,2000)+number(0,2000)+number(0,2000))/10000);
	}
	else
	{
		// 1% : extra LARGE size
		return (int)(fish_info[fish_id].length_range[1] + 
				(fish_info[fish_id].length_range[2] - fish_info[fish_id].length_range[1])
				* 2 * asin(number(0,10000)/10000.) / std::numbers::pi);
	}
}

void take(fishing_event_info* info, character * ch)
{
	if (info->step == 1)	// If a fish is hooked..
	{
		int32_t ms = (int32_t) ((get_dword_time() - info->hang_time));
		uint32_t item_vnum = 0;
		int ret = compute(info->fish_id, ms, &item_vnum, get_fishing_level(ch));

		switch (ret)
		{
			case -2: // Not caught
			case -3: // Failed due to difficulty
			case -1: // Failed due to timing probability
				//ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("고기가 미끼만 빼먹고 잽싸게 도망칩니다."));
				{
					int map_idx = ch->map_index();
					int prob_idx = get_prob_index_by_map_index(map_idx);

					log_manager::instance().fish_log(
							ch->player_id(),
							prob_idx,
							info->fish_id,
							get_fishing_level(ch),
							ms);
				}
				fishing_fail(ch);
				break;

			case 0:
				//ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("고기가 잡혔습니다! (%s)"), fish_info[info->fish_id].name);
				if (item_vnum)
				{
					fishing_success(ch);

					packet_gc_fishing p;
					p.header = HEADER_GC_FISHING;
					p.subheader = FISHING_SUBHEADER_GC_FISH;
					p.info = item_vnum;
					ch->get_desc()->packet(&p, sizeof(packet_gc_fishing));

					item * item = ch->auto_give_item(item_vnum, 1, -1, false);
					if (item)
					{
						item->set_socket(0,get_fish_length(info->fish_id));
						if (metin2::config::test_server())
						{
							ch->chat_packet(CHAT_TYPE_INFO, lc_text("이번에 잡은 물고기의 길이는 %.2fcm"), item->get_socket(0)/100.f);
						}

						if (quest::quest_manager::instance().event_flag("fishevent") > 0 && (info->fish_id == 5 || info->fish_id == 6))
						{
							// Event in progress, so record it.

							packet_gd_highscore p;
							p.pid = ch->player_id();
							p.value = item->get_socket(0);

							if (info->fish_id == 5)
							{
								strlcpy(p.board, lc_text("낚시이벤트월척붕어"), sizeof(p.board));
							}
							else if (info->fish_id == 6)
							{
								strlcpy(p.board, lc_text("낚시이벤트잉어"), sizeof(p.board));
							}

							db_clientdesc->db_packet(HEADER_GD_HIGHSCORE_REGISTER, 0, &p, sizeof(packet_gd_highscore));
						}
					}

					int map_idx = ch->map_index();
					int prob_idx = get_prob_index_by_map_index(map_idx);

					log_manager::instance().fish_log(
							ch->player_id(),
							prob_idx,
							info->fish_id,
							get_fishing_level(ch),
							ms,
							true,
							item ? item->get_socket(0) : 0);

				}
				else
				{
					int map_idx = ch->map_index();
					int prob_idx = get_prob_index_by_map_index(map_idx);

					log_manager::instance().fish_log(
							ch->player_id(),
							prob_idx,
							info->fish_id,
							get_fishing_level(ch),
							ms);
					fishing_fail(ch);
				}
				break;
		}
	}
	else if (info->step > 1)
	{
		int map_idx = ch->map_index();
		int prob_idx = get_prob_index_by_map_index(map_idx);

		log_manager::instance().fish_log(
				ch->player_id(),
				prob_idx,
				info->fish_id,
				get_fishing_level(ch),
				7000);
		//ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("고기가 미끼만 빼먹고 잽싸게 도망칩니다."));
		fishing_fail(ch);
	}
	else
	{
		packet_gc_fishing p;
		p.header = HEADER_GC_FISHING;
		p.subheader = FISHING_SUBHEADER_GC_STOP;
		p.info = ch->get_vid();
		ch->packet_around(&p, sizeof(p));
	}

	if (info->step)
	{
		fishing_practice(ch);
	}
}

void simulation(int level, int count, int prob_idx, character * ch)
{
	std::map<std::string, int> fished;
	int total_count = 0;

	for (int i = 0; i < count; ++i)
	{
		int fish_id = determine_fish_by_prob_index(prob_idx);
		uint32_t item = 0;
		compute(fish_id, (number(2000, 4000) + number(2000,4000)) / 2, &item, level);

		if (item)
		{
			fished[fish_info[fish_id].name]++;
			total_count ++;
		}
	}

	for (std::map<std::string,int>::iterator it = fished.begin(); it != fished.end(); ++it)
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("%s : %d 마리"), it->first.c_str(), it->second);

	ch->chat_packet(CHAT_TYPE_INFO, lc_text("%d 종류 %d 마리 낚음"), fished.size(), total_count);
}

void use_fish(character * ch, item * item)
{
	int idx = item->vnum() - fish_info[2].vnum+2;

	// Minnows can't be used; nothing that isn't alive can be used

	if (idx<=1 || idx >= MAX_FISH)
		return;

	int r = number(1, 10000);

	item->set_count(item->count()-1);

	if (r >= 4001)
	{
		// Dead fish
		ch->auto_give_item(fish_info[idx].dead_vnum);
	}
	else if (r >= 2001)
	{
		// Fish bone
		ch->auto_give_item(FISH_BONE_VNUM);
	}
	else
	{
		// 1000 500 300 100 50 30 10 5 4 1
		static int s_acc_prob[NUM_USE_RESULT_COUNT] = { 1000, 1500, 1800, 1900, 1950, 1980, 1990, 1995, 1999, 2000 };
		int u_index = std::lower_bound(s_acc_prob, s_acc_prob + NUM_USE_RESULT_COUNT, r) - s_acc_prob;

		switch (fish_info[idx].used_table[u_index])
		{
			case USED_TREASURE_MAP:	// 3
			case USED_NONE:		// 0
			case USED_WATER_STONE:	// 2
				ch->chat_packet(CHAT_TYPE_INFO, lc_text("고기가 흔적도 없이 사라집니다."));
				break;

			case USED_SHELLFISH:	// 1

				ch->chat_packet(CHAT_TYPE_INFO, lc_text("배 속에서 조개가 나왔습니다."));
				ch->auto_give_item(SHELLFISH_VNUM);
				break;

			case USED_EARTHWARM:	// 4
				ch->chat_packet(CHAT_TYPE_INFO, lc_text("배 속에서 지렁이가 나왔습니다."));
				ch->auto_give_item(EARTHWORM_VNUM);
				break;

			default:
				ch->auto_give_item(fish_info[idx].used_table[u_index]);
				break;
		}
	}
}

void grill(character * ch, item * item)
{
	int idx = -1;
	uint32_t vnum = item->vnum();
	if (vnum >= 27803 && vnum <= 27830)
		idx = vnum - 27800;
	if (vnum >= 27833 && vnum <= 27860)
		idx = vnum - 27830;
	if (idx == -1)
		return;

	int count = item->count();

	ch->chat_packet(CHAT_TYPE_INFO, lc_text("%s를 구웠습니다."), item->name());
	item->set_count(0);
	ch->auto_give_item(fish_info[idx].grill_vnum, count);
}

bool refinable_rod(item * rod)
{
	if (rod->type() != ITEM_ROD)
		return false;

	if (rod->is_equipped())
		return false;

	return (rod->get_socket(0) == rod->value(2));
}

int real_refine_rod(character * ch, item * item)
{
	if (!ch || !item)
		return 2;

	// REFINE_ROD_HACK_BUG_FIX
	if (!refinable_rod(item))
	{
		metin2::log::error("REFINE_ROD_HACK pid({:d}) item({}:{:d})", ch->player_id(), item->name(), item->id());

		log_manager::instance().refine_log(ch->player_id(), item->name(), item->id(), -1, 1, "ROD_HACK");

		return 2;
	}
	// END_OF_REFINE_ROD_HACK_BUG_FIX

	game::items::item * rod = item;	

	int iAdv = rod->value(0) / 10;

	if (number(1,100) <= rod->value(3))
	{
		log_manager::instance().refine_log(ch->player_id(), rod->name(), rod->id(), iAdv, 1, "ROD");

		game::items::item * pkNewItem = item_manager::instance().create_item(rod->refined_vnum(), 1);

		if (pkNewItem)
		{
			uint8_t bCell = rod->cell();
			// Fishing rod upgrade succeeded
			item_manager::instance().remove_item(rod, "REMOVE (REFINE FISH_ROD)");
			pkNewItem->add_to_character(ch, item_pos (INVENTORY, bCell)); 
			log_manager::instance().item_log(ch, pkNewItem, "REFINE FISH_ROD SUCCESS", pkNewItem->name());
			return 1;
		}

		// Fishing rod upgrade failed
		return 2;
	}
	else
	{
		log_manager::instance().refine_log(ch->player_id(), rod->name(), rod->id(), iAdv, 0, "ROD");

		game::items::item * pkNewItem = item_manager::instance().create_item(rod->value(4), 1);
		if (pkNewItem)
		{
			uint8_t bCell = rod->cell();
			// Fishing rod upgrade succeeded
			item_manager::instance().remove_item(rod, "REMOVE (REFINE FISH_ROD)");
			pkNewItem->add_to_character(ch, item_pos(INVENTORY, bCell)); 
			log_manager::instance().item_log(ch, pkNewItem, "REFINE FISH_ROD FAIL", pkNewItem->name());
			return 0;
		}

		// Fishing rod upgrade failed
		return 2;
	}
}
}
}
