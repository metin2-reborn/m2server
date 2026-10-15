#include <metin2/db/tables/proto_reader.h>

#include <metin2/db/tables/csv_reader.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/common/utils.h>
#include <metin2/log/log.h>
#include <metin2/core/utils.h>

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <map>
#include <memory>
#include <sstream>
#include <string>

namespace metin2::db::inline tables {

inline std::string trim_left(const std::string& str)
{
    std::string::size_type n = str.find_first_not_of(" \t\v\n\r");
    return n == std::string::npos ? str : str.substr(n, str.length());
}

inline std::string trim_right(const std::string& str)
{
    std::string::size_type n = str.find_last_not_of(" \t\v\n\r");
    return n == std::string::npos ? str : str.substr(0, n + 1);
}

std::string trim(const std::string& str){return trim_left(trim_right(str));}

static std::unique_ptr<std::string[]> string_split(std::string origin, std::string tok)
{
    size_t  cutAt;
    int     index     = 0;
    auto strResult = std::make_unique<std::string[]>(30);

    while ((cutAt = origin.find_first_of(tok)) != origin.npos)
    {
       if (cutAt > 0 && index < 30)
       {
            strResult[index++] = origin.substr(0, cutAt);
       }
       origin = origin.substr(cutAt+1);
    }

    if(origin.length() > 0 && index < 30)
    {
        strResult[index++] = origin.substr(0, cutAt);
    }

	for( int i=0;i<index;i++)
	{
		strResult[i] = trim(strResult[i]);
	}

    return strResult;
}



int get_item_type_value(std::string input_string)
{
	std::string arType[] = {"ITEM_NONE", "ITEM_WEAPON",
		"ITEM_ARMOR", "ITEM_USE", 
		"ITEM_AUTOUSE", "ITEM_MATERIAL",
		"ITEM_SPECIAL", "ITEM_TOOL", 
		"ITEM_LOTTERY", "ITEM_ELK",

		"ITEM_METIN", "ITEM_CONTAINER", 
		"ITEM_FISH", "ITEM_ROD", 
		"ITEM_RESOURCE", "ITEM_CAMPFIRE",
		"ITEM_UNIQUE", "ITEM_SKILLBOOK", 
		"ITEM_QUEST", "ITEM_POLYMORPH",

		"ITEM_TREASURE_BOX", "ITEM_TREASURE_KEY",
		"ITEM_SKILLFORGET", "ITEM_GIFTBOX", 
		"ITEM_PICK", "ITEM_HAIR", 
		"ITEM_TOTEM", "ITEM_BLEND", 
		"ITEM_COSTUME", "ITEM_DS",
	
		"ITEM_SPECIAL_DS",	"ITEM_EXTRACT",
		"ITEM_SECONDARY_COIN",

		"ITEM_RING",
		"ITEM_BELT",
	};

	
	int retInt = -1;
	for (size_t j = 0; j < sizeof(arType)/sizeof(arType[0]);j++) {
		std::string tempString = arType[j];
		if	(input_string.find(tempString)!=std::string::npos && tempString.find(input_string)!=std::string::npos) {
			retInt =  j;
			break;
		}
	}

	return retInt;

}

int get_item_sub_type_value(int type_value, std::string input_string) 
{
	static std::string arSub1[] = { "WEAPON_SWORD", "WEAPON_DAGGER", "WEAPON_BOW", "WEAPON_TWO_HANDED",
				"WEAPON_BELL", "WEAPON_FAN", "WEAPON_ARROW", "WEAPON_MOUNT_SPEAR"};
	static std::string arSub2[] = { "ARMOR_BODY", "ARMOR_HEAD", "ARMOR_SHIELD", "ARMOR_WRIST", "ARMOR_FOOTS",
				"ARMOR_NECK", "ARMOR_EAR", "ARMOR_NUM_TYPES"};
	static std::string arSub3[] = { "USE_POTION", "USE_TALISMAN", "USE_TUNING", "USE_MOVE", "USE_TREASURE_BOX", "USE_MONEYBAG", "USE_BAIT",
				"USE_ABILITY_UP", "USE_AFFECT", "USE_CREATE_STONE", "USE_SPECIAL", "USE_POTION_NODELAY", "USE_CLEAR",
				"USE_INVISIBILITY", "USE_DETACHMENT", "USE_BUCKET", "USE_POTION_CONTINUE", "USE_CLEAN_SOCKET",
				"USE_CHANGE_ATTRIBUTE", "USE_ADD_ATTRIBUTE", "USE_ADD_ACCESSORY_SOCKET", "USE_PUT_INTO_ACCESSORY_SOCKET",
				"USE_ADD_ATTRIBUTE2", "USE_RECIPE", "USE_CHANGE_ATTRIBUTE2", "USE_BIND", "USE_UNBIND", "USE_TIME_CHARGE_PER", "USE_TIME_CHARGE_FIX", "USE_PUT_INTO_BELT_SOCKET", "USE_PUT_INTO_RING_SOCKET"};
	static std::string arSub4[] = { "AUTOUSE_POTION", "AUTOUSE_ABILITY_UP", "AUTOUSE_BOMB", "AUTOUSE_GOLD", "AUTOUSE_MONEYBAG", "AUTOUSE_TREASURE_BOX"};
	static std::string arSub5[] = { "MATERIAL_LEATHER", "MATERIAL_BLOOD", "MATERIAL_ROOT", "MATERIAL_NEEDLE", "MATERIAL_JEWEL", 
		"MATERIAL_DS_REFINE_NORMAL", "MATERIAL_DS_REFINE_BLESSED", "MATERIAL_DS_REFINE_HOLLY"};
	static std::string arSub6[] = { "SPECIAL_MAP", "SPECIAL_KEY", "SPECIAL_DOC", "SPECIAL_SPIRIT"};
	static std::string arSub7[] = { "TOOL_FISHING_ROD" };
	static std::string arSub8[] = { "LOTTERY_TICKET", "LOTTERY_INSTANT" };
	static std::string arSub10[] = { "METIN_NORMAL", "METIN_GOLD" };
	static std::string arSub12[] = { "FISH_ALIVE", "FISH_DEAD"};
	static std::string arSub14[] = { "RESOURCE_FISHBONE", "RESOURCE_WATERSTONEPIECE", "RESOURCE_WATERSTONE", "RESOURCE_BLOOD_PEARL",
						"RESOURCE_BLUE_PEARL", "RESOURCE_WHITE_PEARL", "RESOURCE_BUCKET", "RESOURCE_CRYSTAL", "RESOURCE_GEM",
						"RESOURCE_STONE", "RESOURCE_METIN", "RESOURCE_ORE" };
	static std::string arSub16[] = { "UNIQUE_NONE", "UNIQUE_BOOK", "UNIQUE_SPECIAL_RIDE", "UNIQUE_3", "UNIQUE_4", "UNIQUE_5",
					"UNIQUE_6", "UNIQUE_7", "UNIQUE_8", "UNIQUE_9", "USE_SPECIAL"};
	static std::string arSub28[] = { "COSTUME_BODY", "COSTUME_HAIR" };
	static std::string arSub29[] = { "DS_SLOT1", "DS_SLOT2", "DS_SLOT3", "DS_SLOT4", "DS_SLOT5", "DS_SLOT6" };
	static std::string arSub31[] = { "EXTRACT_DRAGON_SOUL", "EXTRACT_DRAGON_HEART" };
	
	static std::string* arSubType[] = {nullptr,
		arSub1,
		arSub2,
		arSub3,
		arSub4,
		arSub5,
		arSub6,
		arSub7,
		arSub8,
		nullptr,
		arSub10,
		nullptr,
		arSub12,
		nullptr,
		arSub14,
		nullptr,
		arSub16,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		arSub28,
		arSub29,
		arSub29,
		arSub31,
		nullptr,
		nullptr,
		nullptr,
	};
	static int arNumberOfSubtype[std::size(arSubType)] = {
		0,
		sizeof(arSub1)/sizeof(arSub1[0]),
		sizeof(arSub2)/sizeof(arSub2[0]),
		sizeof(arSub3)/sizeof(arSub3[0]),
		sizeof(arSub4)/sizeof(arSub4[0]),
		sizeof(arSub5)/sizeof(arSub5[0]),
		sizeof(arSub6)/sizeof(arSub6[0]),
		sizeof(arSub7)/sizeof(arSub7[0]),
		sizeof(arSub8)/sizeof(arSub8[0]),
		0,
		sizeof(arSub10)/sizeof(arSub10[0]),
		0,
		sizeof(arSub12)/sizeof(arSub12[0]),
		0,
		sizeof(arSub14)/sizeof(arSub14[0]),
		0,
		sizeof(arSub16)/sizeof(arSub16[0]),
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
		sizeof(arSub28)/sizeof(arSub28[0]),
		sizeof(arSub29)/sizeof(arSub29[0]),
		sizeof(arSub29)/sizeof(arSub29[0]),
		sizeof(arSub31)/sizeof(arSub31[0]),
		0,
		0,
		0,
	};
	

	assert(std::size(arSubType) > static_cast<size_t>(type_value) && "Subtype rule: Out of range!!");

	if (static_cast<int>(std::size(arSubType)) <= type_value)
	{
		metin2::log::error("SubType : Out of range!! (type_value: {:d}, count of registered subtype: {:d}", type_value, std::size(arSubType));
		return -1;
	}

	if (arSubType[type_value]==nullptr) {
		return 0;
	}

	int retInt = -1;
	for (int j=0;j<arNumberOfSubtype[type_value];j++) {
		std::string tempString = arSubType[type_value][j];
		std::string tempInputString = trim(input_string);
		if	(tempInputString.compare(tempString)==0)
		{
			retInt =  j;
			break;
		}
	}

	return retInt;
}





int get_item_anti_flag_value(std::string input_string) 
{

	std::string arAntiFlag[] = {"ANTI_FEMALE", "ANTI_MALE", "ANTI_MUSA", "ANTI_ASSASSIN", "ANTI_SURA", "ANTI_MUDANG",
							"ANTI_GET", "ANTI_DROP", "ANTI_SELL", "ANTI_EMPIRE_A", "ANTI_EMPIRE_B", "ANTI_EMPIRE_C",
							"ANTI_SAVE", "ANTI_GIVE", "ANTI_PKDROP", "ANTI_STACK", "ANTI_MYSHOP", "ANTI_SAFEBOX"};


	int retValue = 0;
	std::unique_ptr<std::string[]> arInputString = string_split(input_string, "|");
	for (size_t i = 0; i < sizeof(arAntiFlag)/sizeof(arAntiFlag[0]);i++) {
		std::string tempString = arAntiFlag[i];
		for (int j=0; j<30 ; j++)
		{
			std::string tempString2 = arInputString[j];
			if (tempString2.compare(tempString)==0) {
				retValue = retValue + pow((float)2,(float)i);
			}
			
			if(tempString2.compare("") == 0)
				break;
		}
	}

	return retValue;
}

int get_item_flag_value(std::string input_string) 
{

	std::string arFlag[] = {"ITEM_TUNABLE", "ITEM_SAVE", "ITEM_STACKABLE", "COUNT_PER_1GOLD", "ITEM_SLOW_QUERY", "ITEM_UNIQUE",
			"ITEM_MAKECOUNT", "ITEM_IRREMOVABLE", "CONFIRM_WHEN_USE", "QUEST_USE", "QUEST_USE_MULTIPLE",
			"QUEST_GIVE", "ITEM_QUEST", "LOG", "STACKABLE", "SLOW_QUERY", "REFINEABLE", "IRREMOVABLE", "ITEM_APPLICABLE"};


	int retValue = 0;
	std::unique_ptr<std::string[]> arInputString = string_split(input_string, "|");
	for (size_t i = 0; i < sizeof(arFlag)/sizeof(arFlag[0]);i++) {
		std::string tempString = arFlag[i];
		for (int j=0; j<30 ; j++)
		{
			std::string tempString2 = arInputString[j];
			if (tempString2.compare(tempString)==0) {
				retValue = retValue + pow((float)2,(float)i);
			}
			
			if(tempString2.compare("") == 0)
				break;
		}
	}

	return retValue;
}

int get_item_wear_flag_value(std::string input_string) 
{

	std::string arWearrFlag[] = {"WEAR_BODY", "WEAR_HEAD", "WEAR_FOOTS", "WEAR_WRIST", "WEAR_WEAPON", "WEAR_NECK", "WEAR_EAR", "WEAR_SHIELD", "WEAR_UNIQUE",
					"WEAR_ARROW", "WEAR_HAIR", "WEAR_ABILITY"};


	int retValue = 0;
	std::unique_ptr<std::string[]> arInputString = string_split(input_string, "|");
	for (size_t i = 0; i < sizeof(arWearrFlag)/sizeof(arWearrFlag[0]);i++) {
		std::string tempString = arWearrFlag[i];
		for (int j=0; j<30 ; j++)
		{
			std::string tempString2 = arInputString[j];
			if (tempString2.compare(tempString)==0) {
				retValue = retValue + pow((float)2,(float)i);
			}
			
			if(tempString2.compare("") == 0)
				break;
		}
	}

	return retValue;
}

int get_item_immune_value(std::string input_string) 
{

	std::string arImmune[] = {"PARA","CURSE","STUN","SLEEP","SLOW","POISON","TERROR"};

	int retValue = 0;
	std::unique_ptr<std::string[]> arInputString = string_split(input_string, "|");
	for (size_t i = 0; i < sizeof(arImmune)/sizeof(arImmune[0]);i++) {
		std::string tempString = arImmune[i];
		for (int j=0; j<30 ; j++)
		{
			std::string tempString2 = arInputString[j];
			if (tempString2.compare(tempString)==0) {
				retValue = retValue + pow((float)2,(float)i);
			}
			
			if(tempString2.compare("") == 0)
				break;
		}
	}

	return retValue;
}




int get_item_limit_type_value(std::string input_string)
{
	std::string arLimitType[] = {"LIMIT_NONE", "LEVEL", "STR", "DEX", "INT", "CON", "PC_BANG", "REAL_TIME", "REAL_TIME_FIRST_USE", "TIMER_BASED_ON_WEAR"};
	
	int retInt = -1;
	for (size_t j = 0; j < sizeof(arLimitType)/sizeof(arLimitType[0]);j++) {
		std::string tempString = arLimitType[j];
		std::string tempInputString = trim(input_string);
		if	(tempInputString.compare(tempString)==0)
		{
			retInt =  j;
			break;
		}
	}

	return retInt;
}


int get_item_apply_type_value(std::string input_string)
{
	std::string arApplyType[] = {"APPLY_NONE", "APPLY_MAX_HP", "APPLY_MAX_SP", "APPLY_CON", "APPLY_INT", "APPLY_STR", "APPLY_DEX", "APPLY_ATT_SPEED",
			"APPLY_MOV_SPEED", "APPLY_CAST_SPEED", "APPLY_HP_REGEN", "APPLY_SP_REGEN", "APPLY_POISON_PCT", "APPLY_STUN_PCT",
			"APPLY_SLOW_PCT", "APPLY_CRITICAL_PCT", "APPLY_PENETRATE_PCT", "APPLY_ATTBONUS_HUMAN", "APPLY_ATTBONUS_ANIMAL",
			"APPLY_ATTBONUS_ORC", "APPLY_ATTBONUS_MILGYO", "APPLY_ATTBONUS_UNDEAD", "APPLY_ATTBONUS_DEVIL", "APPLY_STEAL_HP",
			"APPLY_STEAL_SP", "APPLY_MANA_BURN_PCT", "APPLY_DAMAGE_SP_RECOVER", "APPLY_BLOCK", "APPLY_DODGE", "APPLY_RESIST_SWORD",
			"APPLY_RESIST_TWOHAND", "APPLY_RESIST_DAGGER", "APPLY_RESIST_BELL", "APPLY_RESIST_FAN", "APPLY_RESIST_BOW", "APPLY_RESIST_FIRE",
			"APPLY_RESIST_ELEC", "APPLY_RESIST_MAGIC", "APPLY_RESIST_WIND", "APPLY_REFLECT_MELEE", "APPLY_REFLECT_CURSE", "APPLY_POISON_REDUCE",
			"APPLY_KILL_SP_RECOVER", "APPLY_EXP_DOUBLE_BONUS", "APPLY_GOLD_DOUBLE_BONUS", "APPLY_ITEM_DROP_BONUS", "APPLY_POTION_BONUS",
			"APPLY_KILL_HP_RECOVER", "APPLY_IMMUNE_STUN", "APPLY_IMMUNE_SLOW", "APPLY_IMMUNE_FALL", "APPLY_SKILL", "APPLY_BOW_DISTANCE",
			"APPLY_ATT_GRADE_BONUS", "APPLY_DEF_GRADE_BONUS", "APPLY_MAGIC_ATT_GRADE", "APPLY_MAGIC_DEF_GRADE", "APPLY_CURSE_PCT",
			"APPLY_MAX_STAMINA", "APPLY_ATTBONUS_WARRIOR", "APPLY_ATTBONUS_ASSASSIN", "APPLY_ATTBONUS_SURA", "APPLY_ATTBONUS_SHAMAN",
			"APPLY_ATTBONUS_MONSTER", "APPLY_MALL_ATTBONUS", "APPLY_MALL_DEFBONUS", "APPLY_MALL_EXPBONUS", "APPLY_MALL_ITEMBONUS",
			"APPLY_MALL_GOLDBONUS", "APPLY_MAX_HP_PCT", "APPLY_MAX_SP_PCT", "APPLY_SKILL_DAMAGE_BONUS", "APPLY_NORMAL_HIT_DAMAGE_BONUS",
			"APPLY_SKILL_DEFEND_BONUS", "APPLY_NORMAL_HIT_DEFEND_BONUS", "APPLY_PC_BANG_EXP_BONUS", "APPLY_PC_BANG_DROP_BONUS",
			"APPLY_EXTRACT_HP_PCT", "APPLY_RESIST_WARRIOR", "APPLY_RESIST_ASSASSIN", "APPLY_RESIST_SURA", "APPLY_RESIST_SHAMAN",
			"APPLY_ENERGY",	"APPLY_DEF_GRADE", "APPLY_COSTUME_ATTR_BONUS", "APPLY_MAGIC_ATTBONUS_PER", "APPLY_MELEE_MAGIC_ATTBONUS_PER",
			"APPLY_RESIST_ICE", "APPLY_RESIST_EARTH", "APPLY_RESIST_DARK", "APPLY_ANTI_CRITICAL_PCT", "APPLY_ANTI_PENETRATE_PCT",
	};

	int retInt = -1;
	for (size_t j = 0; j < sizeof(arApplyType)/sizeof(arApplyType[0]);j++) {
		std::string tempString = arApplyType[j];
		std::string tempInputString = trim(input_string);
		if	(tempInputString.compare(tempString)==0)
		{ 
			retInt =  j;
			break;
		}
	}

	return retInt;

}




int get_mob_rank_value(std::string input_string) 
{
	std::string arRank[] = {"PAWN", "S_PAWN", "KNIGHT", "S_KNIGHT", "BOSS", "KING"};

	int retInt = -1;
	for (size_t j = 0; j < sizeof(arRank)/sizeof(arRank[0]);j++) {
		std::string tempString = arRank[j];
		std::string tempInputString = trim(input_string);
		if	(tempInputString.compare(tempString)==0) 
		{
			retInt =  j;
			break;
		}
	}

	return retInt;
}


int get_mob_type_value(std::string input_string)
{
	std::string arType[] = { "MONSTER", "NPC", "STONE", "WARP", "DOOR", "BUILDING", "PC", "POLYMORPH_PC", "HORSE", "GOTO"};

	int retInt = -1;
	for (size_t j = 0; j < sizeof(arType)/sizeof(arType[0]);j++) {
		std::string tempString = arType[j];
		std::string tempInputString = trim(input_string);
		if	(tempInputString.compare(tempString)==0) 
		{
			retInt =  j;
			break;
		}
	}

	return retInt;
}

int get_mob_battle_type_value(std::string input_string) 
{
	std::string arBattleType[] = { "MELEE", "RANGE", "MAGIC", "SPECIAL", "POWER", "TANKER", "SUPER_POWER", "SUPER_TANKER"};

	int retInt = -1;
	for (size_t j = 0; j < sizeof(arBattleType)/sizeof(arBattleType[0]);j++) {
		std::string tempString = arBattleType[j];
		std::string tempInputString = trim(input_string);
		if	(tempInputString.compare(tempString)==0) 
		{ 
			retInt =  j;
			break;
		}
	}

	return retInt;
}

int get_mob_size_value(std::string input_string)
{
	std::string arSize[] = { "SMALL", "MEDIUM", "BIG"};

	int retInt = 0;
	for (size_t j = 0; j < sizeof(arSize)/sizeof(arSize[0]);j++) {
		std::string tempString = arSize[j];
		std::string tempInputString = trim(input_string);
		if	(tempInputString.compare(tempString)==0) 
		{
			retInt =  j + 1;
			break;
		}
	}

	return retInt;
}

int get_mob_ai_flag_value(std::string input_string)
{
	std::string arAIFlag[] = {"AGGR","NOMOVE","COWARD","NOATTSHINSU","NOATTCHUNJO","NOATTJINNO","ATTMOB","BERSERK","STONESKIN","GODSPEED","DEATHBLOW","REVIVE"};


	int retValue = 0;
	std::unique_ptr<std::string[]> arInputString = string_split(input_string, ",");
	for (size_t i = 0; i < sizeof(arAIFlag)/sizeof(arAIFlag[0]);i++) {
		std::string tempString = arAIFlag[i];
		for (int j=0; j<30 ; j++)
		{
			std::string tempString2 = arInputString[j];
			if (tempString2.compare(tempString)==0) {
				retValue = retValue + pow((float)2,(float)i);
			}
			
			if(tempString2.compare("") == 0)
				break;
		}
	}

	return retValue;
}
int get_mob_race_flag_value(std::string input_string)
{
	std::string arRaceFlag[] = {"ANIMAL","UNDEAD","DEVIL","HUMAN","ORC","MILGYO","INSECT","FIRE","ICE","DESERT","TREE",
		"ATT_ELEC","ATT_FIRE","ATT_ICE","ATT_WIND","ATT_EARTH","ATT_DARK"};

	int retValue = 0;
	std::unique_ptr<std::string[]> arInputString = string_split(input_string, ",");
	for (size_t i = 0; i < sizeof(arRaceFlag)/sizeof(arRaceFlag[0]);i++) {
		std::string tempString = arRaceFlag[i];
		for (int j=0; j<30 ; j++)
		{
			std::string tempString2 = arInputString[j];
			if (tempString2.compare(tempString)==0) {
				retValue = retValue + pow((float)2,(float)i);
			}
			
			if(tempString2.compare("") == 0)
				break;
		}
	}

	return retValue;
}
int get_mob_immune_flag_value(std::string input_string)
{
	std::string arImmuneFlag[] = {"STUN","SLOW","FALL","CURSE","POISON","TERROR", "REFLECT"};

	int retValue = 0;
	std::unique_ptr<std::string[]> arInputString = string_split(input_string, ",");
	for (size_t i = 0; i < sizeof(arImmuneFlag)/sizeof(arImmuneFlag[0]);i++) {
		std::string tempString = arImmuneFlag[i];
		for (int j=0; j<30 ; j++)
		{
			std::string tempString2 = arInputString[j];
			if (tempString2.compare(tempString)==0) {
				retValue = retValue + pow((float)2,(float)i);
			}
			
			if(tempString2.compare("") == 0)
				break;
		}
	}


	return retValue;
}


#ifndef __DUMP_PROTO__

bool set_proto_mob_table(mob_table *mob_table_value, c_csv_table &csv_table,std::map<int,const char*> &name_map)
{
	int col = 0;
	str_to_number(mob_table_value->vnum, csv_table.as_string_by_index(col++));
	utf8_strlcpy(mob_table_value->name, csv_table.as_string_by_index(col++), sizeof(mob_table_value->name));

	std::map<int,const char*>::iterator it;
	it = name_map.find(mob_table_value->vnum);
	if (it != name_map.end()) {
		const char * localeName = it->second;
		utf8_strlcpy(mob_table_value->locale_name, localeName, sizeof (mob_table_value->locale_name));
	} else {
		utf8_strlcpy(mob_table_value->locale_name, mob_table_value->name, sizeof (mob_table_value->locale_name));
	}

	int rankValue = get_mob_rank_value(csv_table.as_string_by_index(col++));
	mob_table_value->rank = rankValue;
	int typeValue = get_mob_type_value(csv_table.as_string_by_index(col++));
	mob_table_value->type = typeValue;
	int battleTypeValue = get_mob_battle_type_value(csv_table.as_string_by_index(col++));
	mob_table_value->battle_type = battleTypeValue;

	str_to_number(mob_table_value->level, csv_table.as_string_by_index(col++));
	int sizeValue = get_mob_size_value(csv_table.as_string_by_index(col++));
	mob_table_value->size = sizeValue;
	int aiFlagValue = get_mob_ai_flag_value(csv_table.as_string_by_index(col++));
	mob_table_value->ai_flag = aiFlagValue;
	col++;
	int raceFlagValue = get_mob_race_flag_value(csv_table.as_string_by_index(col++));
	mob_table_value->race_flag = raceFlagValue;
	int immuneFlagValue = get_mob_immune_flag_value(csv_table.as_string_by_index(col++));
	mob_table_value->immune_flag = immuneFlagValue;

	str_to_number(mob_table_value->empire, csv_table.as_string_by_index(col++));

	strlcpy(mob_table_value->folder, csv_table.as_string_by_index(col++), sizeof(mob_table_value->folder));

	str_to_number(mob_table_value->on_click_type, csv_table.as_string_by_index(col++));	

	str_to_number(mob_table_value->str, csv_table.as_string_by_index(col++));
	str_to_number(mob_table_value->dex, csv_table.as_string_by_index(col++));
	str_to_number(mob_table_value->con, csv_table.as_string_by_index(col++));
	str_to_number(mob_table_value->iq, csv_table.as_string_by_index(col++));
	str_to_number(mob_table_value->damage_range[0], csv_table.as_string_by_index(col++));
	str_to_number(mob_table_value->damage_range[1], csv_table.as_string_by_index(col++));
	str_to_number(mob_table_value->max_hp, csv_table.as_string_by_index(col++));
	str_to_number(mob_table_value->regen_cycle, csv_table.as_string_by_index(col++));
	str_to_number(mob_table_value->regen_percent,	csv_table.as_string_by_index(col++));
	str_to_number(mob_table_value->gold_min, csv_table.as_string_by_index(col++));
	str_to_number(mob_table_value->gold_max, csv_table.as_string_by_index(col++));
	str_to_number(mob_table_value->exp,	csv_table.as_string_by_index(col++));
	str_to_number(mob_table_value->def, csv_table.as_string_by_index(col++));
	str_to_number(mob_table_value->s_attack_speed, csv_table.as_string_by_index(col++));
	str_to_number(mob_table_value->s_moving_speed, csv_table.as_string_by_index(col++));
	str_to_number(mob_table_value->aggresive_hp_pct, csv_table.as_string_by_index(col++));
	str_to_number(mob_table_value->aggressive_sight, csv_table.as_string_by_index(col++));
	str_to_number(mob_table_value->attack_range, csv_table.as_string_by_index(col++));

	str_to_number(mob_table_value->drop_item_vnum, csv_table.as_string_by_index(col++));
	str_to_number(mob_table_value->resurrection_vnum, csv_table.as_string_by_index(col++));
	for (int i = 0; i < MOB_ENCHANTS_MAX_NUM; ++i)
		str_to_number(mob_table_value->c_enchants[i], csv_table.as_string_by_index(col++));

	for (int i = 0; i < MOB_RESISTS_MAX_NUM; ++i)
		str_to_number(mob_table_value->c_resists[i], csv_table.as_string_by_index(col++));

	str_to_number(mob_table_value->dam_multiply, csv_table.as_string_by_index(col++));
	str_to_number(mob_table_value->summon_vnum, csv_table.as_string_by_index(col++));
	str_to_number(mob_table_value->drain_sp, csv_table.as_string_by_index(col++));

	str_to_number(mob_table_value->mob_color, csv_table.as_string_by_index(col++));

	str_to_number(mob_table_value->polymorph_item_vnum, csv_table.as_string_by_index(col++));

	str_to_number(mob_table_value->skills[0].level, csv_table.as_string_by_index(col++));
	str_to_number(mob_table_value->skills[0].vnum, csv_table.as_string_by_index(col++));
	str_to_number(mob_table_value->skills[1].level, csv_table.as_string_by_index(col++));
	str_to_number(mob_table_value->skills[1].vnum, csv_table.as_string_by_index(col++));
	str_to_number(mob_table_value->skills[2].level, csv_table.as_string_by_index(col++));
	str_to_number(mob_table_value->skills[2].vnum, csv_table.as_string_by_index(col++));
	str_to_number(mob_table_value->skills[3].level, csv_table.as_string_by_index(col++));
	str_to_number(mob_table_value->skills[3].vnum, csv_table.as_string_by_index(col++));
	str_to_number(mob_table_value->skills[4].level, csv_table.as_string_by_index(col++));
	str_to_number(mob_table_value->skills[4].vnum, csv_table.as_string_by_index(col++));

	str_to_number(mob_table_value->berserk_point, csv_table.as_string_by_index(col++));
	str_to_number(mob_table_value->stone_skin_point, csv_table.as_string_by_index(col++));
	str_to_number(mob_table_value->god_speed_point, csv_table.as_string_by_index(col++));
	str_to_number(mob_table_value->death_blow_point, csv_table.as_string_by_index(col++));
	str_to_number(mob_table_value->revive_point, csv_table.as_string_by_index(col++));

	metin2::log::info( "MOB #{:<5d} {:<24} level: {:<3d} rank: {:d} empire: {:d}", mob_table_value->vnum, mob_table_value->locale_name, mob_table_value->level, mob_table_value->rank, mob_table_value->empire);

	return true;
}

bool set_proto_item_table(item_table *item_table_value, c_csv_table &csv_table,std::map<int,const char*> &name_map)
{
	int col = 0;

	int dataArray[33];
	for (size_t i = 0; i < sizeof(dataArray)/sizeof(dataArray[0]);i++) {
		int validCheck = 0;
		if (i==2) {
			dataArray[i] = get_item_type_value(csv_table.as_string_by_index(col));
			validCheck = dataArray[i];
		} else if (i==3) {
			dataArray[i] = get_item_sub_type_value(dataArray[i-1], csv_table.as_string_by_index(col));
			validCheck = dataArray[i];
		} else if (i==5) {
			dataArray[i] = get_item_anti_flag_value(csv_table.as_string_by_index(col));
			validCheck = dataArray[i];
		} else if (i==6) {
			dataArray[i] = get_item_flag_value(csv_table.as_string_by_index(col));
			validCheck = dataArray[i];
		} else if (i==7) {
			dataArray[i] = get_item_wear_flag_value(csv_table.as_string_by_index(col));
			validCheck = dataArray[i];
		} else if (i==8) {
			dataArray[i] = get_item_immune_value(csv_table.as_string_by_index(col));
			validCheck = dataArray[i];
		} else if (i==14) {
			dataArray[i] = get_item_limit_type_value(csv_table.as_string_by_index(col));
			validCheck = dataArray[i];
		} else if (i==16) {
			dataArray[i] = get_item_limit_type_value(csv_table.as_string_by_index(col));
			validCheck = dataArray[i];
		} else if (i==18) {
			dataArray[i] = get_item_apply_type_value(csv_table.as_string_by_index(col));
			validCheck = dataArray[i];
		} else if (i==20) {
			dataArray[i] = get_item_apply_type_value(csv_table.as_string_by_index(col));
			validCheck = dataArray[i];
		} else if (i==22) {
			dataArray[i] = get_item_apply_type_value(csv_table.as_string_by_index(col));
			validCheck = dataArray[i];
		} else {
			str_to_number(dataArray[i], csv_table.as_string_by_index(col));
		}

		if (validCheck == -1)
		{
			std::ostringstream dataStream;

			for (size_t j = 0; j < i; ++j)
				dataStream << dataArray[j] << ",";

			metin2::log::error("ItemProto Reading Failed : Invalid value. (index: {:d}, col: {:d}, value: {})", i, col, csv_table.as_string_by_index(col));
			metin2::log::error("\t0 ~ {:d} Values: {}", i, dataStream.str().c_str());

			exit(0);
		}
		
		col = col + 1;
	}

	{
		std::string s(csv_table.as_string_by_index(0));
		size_t pos = s.find("~");
		if (std::string::npos == pos)
		{
			item_table_value->vnum = dataArray[0];
			item_table_value->vnum_range = 0;
		}
		else
		{
			std::string s_start_vnum (s.substr(0, pos));
			std::string s_end_vnum (s.substr(pos +1 ));

			int start_vnum = atoi(s_start_vnum.c_str());
			int end_vnum = atoi(s_end_vnum.c_str());
			if (0 == start_vnum || (0 != end_vnum && end_vnum < start_vnum))
			{
				metin2::log::error("INVALID VNUM {}", s.c_str());
				return false;
			}
			item_table_value->vnum = start_vnum;
			item_table_value->vnum_range = end_vnum - start_vnum;
		}
	}

	utf8_strlcpy(item_table_value->name, csv_table.as_string_by_index(1), sizeof(item_table_value->name));
	std::map<int,const char*>::iterator it;
	it = name_map.find(item_table_value->vnum);
	if (it != name_map.end()) {
		const char * localeName = it->second;
		utf8_strlcpy(item_table_value->locale_name, localeName, sizeof (item_table_value->locale_name));
	} else {
		utf8_strlcpy(item_table_value->locale_name, item_table_value->name, sizeof (item_table_value->locale_name));
	}
	item_table_value->type = dataArray[2];
	item_table_value->sub_type = dataArray[3];
	item_table_value->size = dataArray[4];
	item_table_value->anti_flags = dataArray[5];
	item_table_value->flags = dataArray[6];
	item_table_value->wear_flags = dataArray[7];
	item_table_value->immune_flag = dataArray[8];
	item_table_value->gold = dataArray[9];
	item_table_value->shop_buy_price = dataArray[10];
	item_table_value->refined_vnum = dataArray[11];
	item_table_value->refine_set = dataArray[12];
	item_table_value->alter_to_magic_item_pct = dataArray[13];
	item_table_value->c_limit_real_time_first_use_index = -1;
	item_table_value->c_limit_timer_based_on_wear_index = -1;

	int i;

	for (i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
	{
		item_table_value->limits[i].type = dataArray[14+i*2];
		item_table_value->limits[i].value = dataArray[15+i*2];

		if (LIMIT_REAL_TIME_START_FIRST_USE == item_table_value->limits[i].type)
			item_table_value->c_limit_real_time_first_use_index = (char)i;

		if (LIMIT_TIMER_BASED_ON_WEAR == item_table_value->limits[i].type)
			item_table_value->c_limit_timer_based_on_wear_index = (char)i;

	}

	for (i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
	{
		item_table_value->applies[i].type = dataArray[18+i*2];
		item_table_value->applies[i].value = dataArray[19+i*2];
	}

	for (i = 0; i < ITEM_VALUES_MAX_NUM; ++i)
		item_table_value->values[i] = dataArray[24+i];

	item_table_value->specular = dataArray[30];

	item_table_value->gain_socket_pct = dataArray[31];
	item_table_value->s_addon_type = dataArray[32];

	str_to_number(item_table_value->weight, "0");
			
	return true;
}

#endif
}
