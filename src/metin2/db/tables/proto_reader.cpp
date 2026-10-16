#include <metin2/db/tables/proto_reader.h>

#include <metin2/db/tables/csv_reader.h>

#include <metin2/common/utils.h>
#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <ranges>
#include <span>
#include <sstream>
#include <string>
#include <string_view>

namespace metin2::db::inline tables {

    namespace {
        constexpr std::array<std::string_view, 35> item_types = {
            "ITEM_NONE", "ITEM_WEAPON", "ITEM_ARMOR", "ITEM_USE", "ITEM_AUTOUSE", "ITEM_MATERIAL", "ITEM_SPECIAL",
            "ITEM_TOOL", "ITEM_LOTTERY", "ITEM_ELK", "ITEM_METIN", "ITEM_CONTAINER", "ITEM_FISH", "ITEM_ROD",
            "ITEM_RESOURCE", "ITEM_CAMPFIRE", "ITEM_UNIQUE", "ITEM_SKILLBOOK", "ITEM_QUEST", "ITEM_POLYMORPH",
            "ITEM_TREASURE_BOX", "ITEM_TREASURE_KEY", "ITEM_SKILLFORGET", "ITEM_GIFTBOX", "ITEM_PICK", "ITEM_HAIR",
            "ITEM_TOTEM", "ITEM_BLEND", "ITEM_COSTUME", "ITEM_DS", "ITEM_SPECIAL_DS", "ITEM_EXTRACT",
            "ITEM_SECONDARY_COIN", "ITEM_RING", "ITEM_BELT"};
        constexpr std::array<std::string_view, 8> weapon_sub_types = {
            "WEAPON_SWORD", "WEAPON_DAGGER", "WEAPON_BOW", "WEAPON_TWO_HANDED", "WEAPON_BELL", "WEAPON_FAN",
            "WEAPON_ARROW", "WEAPON_MOUNT_SPEAR"};
        constexpr std::array<std::string_view, 8> armor_sub_types = {
            "ARMOR_BODY", "ARMOR_HEAD", "ARMOR_SHIELD", "ARMOR_WRIST", "ARMOR_FOOTS", "ARMOR_NECK", "ARMOR_EAR",
            "ARMOR_NUM_TYPES"};
        constexpr std::array<std::string_view, 31> use_sub_types = {
            "USE_POTION", "USE_TALISMAN", "USE_TUNING", "USE_MOVE", "USE_TREASURE_BOX", "USE_MONEYBAG", "USE_BAIT",
            "USE_ABILITY_UP", "USE_AFFECT", "USE_CREATE_STONE", "USE_SPECIAL", "USE_POTION_NODELAY", "USE_CLEAR",
            "USE_INVISIBILITY", "USE_DETACHMENT", "USE_BUCKET", "USE_POTION_CONTINUE", "USE_CLEAN_SOCKET",
            "USE_CHANGE_ATTRIBUTE", "USE_ADD_ATTRIBUTE", "USE_ADD_ACCESSORY_SOCKET", "USE_PUT_INTO_ACCESSORY_SOCKET",
            "USE_ADD_ATTRIBUTE2", "USE_RECIPE", "USE_CHANGE_ATTRIBUTE2", "USE_BIND", "USE_UNBIND",
            "USE_TIME_CHARGE_PER", "USE_TIME_CHARGE_FIX", "USE_PUT_INTO_BELT_SOCKET", "USE_PUT_INTO_RING_SOCKET"};
        constexpr std::array<std::string_view, 6> autouse_sub_types = {
            "AUTOUSE_POTION", "AUTOUSE_ABILITY_UP", "AUTOUSE_BOMB", "AUTOUSE_GOLD", "AUTOUSE_MONEYBAG",
            "AUTOUSE_TREASURE_BOX"};
        constexpr std::array<std::string_view, 8> material_sub_types = {
            "MATERIAL_LEATHER", "MATERIAL_BLOOD", "MATERIAL_ROOT", "MATERIAL_NEEDLE", "MATERIAL_JEWEL",
            "MATERIAL_DS_REFINE_NORMAL", "MATERIAL_DS_REFINE_BLESSED", "MATERIAL_DS_REFINE_HOLLY"};
        constexpr std::array<std::string_view, 4> special_sub_types = {
            "SPECIAL_MAP", "SPECIAL_KEY", "SPECIAL_DOC", "SPECIAL_SPIRIT"};
        constexpr std::array<std::string_view, 1> tool_sub_types = {
            "TOOL_FISHING_ROD"};
        constexpr std::array<std::string_view, 2> lottery_sub_types = {
            "LOTTERY_TICKET", "LOTTERY_INSTANT"};
        constexpr std::array<std::string_view, 2> metin_sub_types = {
            "METIN_NORMAL", "METIN_GOLD"};
        constexpr std::array<std::string_view, 2> fish_sub_types = {
            "FISH_ALIVE", "FISH_DEAD"};
        constexpr std::array<std::string_view, 12> resource_sub_types = {
            "RESOURCE_FISHBONE", "RESOURCE_WATERSTONEPIECE", "RESOURCE_WATERSTONE", "RESOURCE_BLOOD_PEARL",
            "RESOURCE_BLUE_PEARL", "RESOURCE_WHITE_PEARL", "RESOURCE_BUCKET", "RESOURCE_CRYSTAL", "RESOURCE_GEM",
            "RESOURCE_STONE", "RESOURCE_METIN", "RESOURCE_ORE"};
        constexpr std::array<std::string_view, 11> unique_sub_types = {
            "UNIQUE_NONE", "UNIQUE_BOOK", "UNIQUE_SPECIAL_RIDE", "UNIQUE_3", "UNIQUE_4", "UNIQUE_5", "UNIQUE_6",
            "UNIQUE_7", "UNIQUE_8", "UNIQUE_9", "USE_SPECIAL"};
        constexpr std::array<std::string_view, 2> costume_sub_types = {
            "COSTUME_BODY", "COSTUME_HAIR"};
        constexpr std::array<std::string_view, 6> ds_sub_types = {
            "DS_SLOT1", "DS_SLOT2", "DS_SLOT3", "DS_SLOT4", "DS_SLOT5", "DS_SLOT6"};
        constexpr std::array<std::string_view, 2> extract_sub_types = {
            "EXTRACT_DRAGON_SOUL", "EXTRACT_DRAGON_HEART"};
        constexpr std::array<std::string_view, 18> item_anti_flags = {
            "ANTI_FEMALE", "ANTI_MALE", "ANTI_MUSA", "ANTI_ASSASSIN", "ANTI_SURA", "ANTI_MUDANG", "ANTI_GET",
            "ANTI_DROP", "ANTI_SELL", "ANTI_EMPIRE_A", "ANTI_EMPIRE_B", "ANTI_EMPIRE_C", "ANTI_SAVE", "ANTI_GIVE",
            "ANTI_PKDROP", "ANTI_STACK", "ANTI_MYSHOP", "ANTI_SAFEBOX"};
        constexpr std::array<std::string_view, 19> item_flags = {
            "ITEM_TUNABLE", "ITEM_SAVE", "ITEM_STACKABLE", "COUNT_PER_1GOLD", "ITEM_SLOW_QUERY", "ITEM_UNIQUE",
            "ITEM_MAKECOUNT", "ITEM_IRREMOVABLE", "CONFIRM_WHEN_USE", "QUEST_USE", "QUEST_USE_MULTIPLE", "QUEST_GIVE",
            "ITEM_QUEST", "LOG", "STACKABLE", "SLOW_QUERY", "REFINEABLE", "IRREMOVABLE", "ITEM_APPLICABLE"};
        constexpr std::array<std::string_view, 12> item_wear_flags = {
            "WEAR_BODY", "WEAR_HEAD", "WEAR_FOOTS", "WEAR_WRIST", "WEAR_WEAPON", "WEAR_NECK", "WEAR_EAR", "WEAR_SHIELD",
            "WEAR_UNIQUE", "WEAR_ARROW", "WEAR_HAIR", "WEAR_ABILITY"};
        constexpr std::array<std::string_view, 7> item_immune_flags = {
            "PARA", "CURSE", "STUN", "SLEEP", "SLOW", "POISON", "TERROR"};
        constexpr std::array<std::string_view, 10> item_limit_types = {
            "LIMIT_NONE", "LEVEL", "STR", "DEX", "INT", "CON", "PC_BANG", "REAL_TIME", "REAL_TIME_FIRST_USE",
            "TIMER_BASED_ON_WEAR"};
        constexpr std::array<std::string_view, 92> item_apply_types = {
            "APPLY_NONE", "APPLY_MAX_HP", "APPLY_MAX_SP", "APPLY_CON", "APPLY_INT", "APPLY_STR", "APPLY_DEX",
            "APPLY_ATT_SPEED", "APPLY_MOV_SPEED", "APPLY_CAST_SPEED", "APPLY_HP_REGEN", "APPLY_SP_REGEN",
            "APPLY_POISON_PCT", "APPLY_STUN_PCT", "APPLY_SLOW_PCT", "APPLY_CRITICAL_PCT", "APPLY_PENETRATE_PCT",
            "APPLY_ATTBONUS_HUMAN", "APPLY_ATTBONUS_ANIMAL", "APPLY_ATTBONUS_ORC", "APPLY_ATTBONUS_MILGYO",
            "APPLY_ATTBONUS_UNDEAD", "APPLY_ATTBONUS_DEVIL", "APPLY_STEAL_HP", "APPLY_STEAL_SP", "APPLY_MANA_BURN_PCT",
            "APPLY_DAMAGE_SP_RECOVER", "APPLY_BLOCK", "APPLY_DODGE", "APPLY_RESIST_SWORD", "APPLY_RESIST_TWOHAND",
            "APPLY_RESIST_DAGGER", "APPLY_RESIST_BELL", "APPLY_RESIST_FAN", "APPLY_RESIST_BOW", "APPLY_RESIST_FIRE",
            "APPLY_RESIST_ELEC", "APPLY_RESIST_MAGIC", "APPLY_RESIST_WIND", "APPLY_REFLECT_MELEE",
            "APPLY_REFLECT_CURSE", "APPLY_POISON_REDUCE", "APPLY_KILL_SP_RECOVER", "APPLY_EXP_DOUBLE_BONUS",
            "APPLY_GOLD_DOUBLE_BONUS", "APPLY_ITEM_DROP_BONUS", "APPLY_POTION_BONUS", "APPLY_KILL_HP_RECOVER",
            "APPLY_IMMUNE_STUN", "APPLY_IMMUNE_SLOW", "APPLY_IMMUNE_FALL", "APPLY_SKILL", "APPLY_BOW_DISTANCE",
            "APPLY_ATT_GRADE_BONUS", "APPLY_DEF_GRADE_BONUS", "APPLY_MAGIC_ATT_GRADE", "APPLY_MAGIC_DEF_GRADE",
            "APPLY_CURSE_PCT", "APPLY_MAX_STAMINA", "APPLY_ATTBONUS_WARRIOR", "APPLY_ATTBONUS_ASSASSIN",
            "APPLY_ATTBONUS_SURA", "APPLY_ATTBONUS_SHAMAN", "APPLY_ATTBONUS_MONSTER", "APPLY_MALL_ATTBONUS",
            "APPLY_MALL_DEFBONUS", "APPLY_MALL_EXPBONUS", "APPLY_MALL_ITEMBONUS", "APPLY_MALL_GOLDBONUS",
            "APPLY_MAX_HP_PCT", "APPLY_MAX_SP_PCT", "APPLY_SKILL_DAMAGE_BONUS", "APPLY_NORMAL_HIT_DAMAGE_BONUS",
            "APPLY_SKILL_DEFEND_BONUS", "APPLY_NORMAL_HIT_DEFEND_BONUS", "APPLY_PC_BANG_EXP_BONUS",
            "APPLY_PC_BANG_DROP_BONUS", "APPLY_EXTRACT_HP_PCT", "APPLY_RESIST_WARRIOR", "APPLY_RESIST_ASSASSIN",
            "APPLY_RESIST_SURA", "APPLY_RESIST_SHAMAN", "APPLY_ENERGY", "APPLY_DEF_GRADE", "APPLY_COSTUME_ATTR_BONUS",
            "APPLY_MAGIC_ATTBONUS_PER", "APPLY_MELEE_MAGIC_ATTBONUS_PER", "APPLY_RESIST_ICE", "APPLY_RESIST_EARTH",
            "APPLY_RESIST_DARK", "APPLY_ANTI_CRITICAL_PCT", "APPLY_ANTI_PENETRATE_PCT"};
        constexpr std::array<std::string_view, 6> mob_ranks = {
            "PAWN", "S_PAWN", "KNIGHT", "S_KNIGHT", "BOSS", "KING"};
        constexpr std::array<std::string_view, 10> mob_types = {
            "MONSTER", "NPC", "STONE", "WARP", "DOOR", "BUILDING", "PC", "POLYMORPH_PC", "HORSE", "GOTO"};
        constexpr std::array<std::string_view, 8> mob_battle_types = {
            "MELEE", "RANGE", "MAGIC", "SPECIAL", "POWER", "TANKER", "SUPER_POWER", "SUPER_TANKER"};
        constexpr std::array<std::string_view, 3> mob_sizes = {
            "SMALL", "MEDIUM", "BIG"};
        constexpr std::array<std::string_view, 12> mob_ai_flags = {
            "AGGR", "NOMOVE", "COWARD", "NOATTSHINSU", "NOATTCHUNJO", "NOATTJINNO", "ATTMOB", "BERSERK", "STONESKIN",
            "GODSPEED", "DEATHBLOW", "REVIVE"};
        constexpr std::array<std::string_view, 17> mob_race_flags = {
            "ANIMAL", "UNDEAD", "DEVIL", "HUMAN", "ORC", "MILGYO", "INSECT", "FIRE", "ICE", "DESERT", "TREE",
            "ATT_ELEC", "ATT_FIRE", "ATT_ICE", "ATT_WIND", "ATT_EARTH", "ATT_DARK"};
        constexpr std::array<std::string_view, 7> mob_immune_flags = {
            "STUN", "SLOW", "FALL", "CURSE", "POISON", "TERROR", "REFLECT"};

        using sub_type_names = std::span<const std::string_view>;

        constexpr std::array<sub_type_names, 35> item_sub_types = {
            {{}, weapon_sub_types, armor_sub_types, use_sub_types, autouse_sub_types, material_sub_types,
             special_sub_types, tool_sub_types, lottery_sub_types, {}, metin_sub_types, {}, fish_sub_types, {},
             resource_sub_types, {}, unique_sub_types, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
             costume_sub_types, ds_sub_types, ds_sub_types, extract_sub_types, {}, {}, {}}
        };

        auto trim(std::string_view text) -> std::string_view {
            constexpr std::string_view whitespace = " \t\v\n\r";
            const auto first = text.find_first_not_of(whitespace);
            if (first == std::string_view::npos)
                return text;
            return text.substr(first, text.find_last_not_of(whitespace) - first + 1);
        }

        auto index_of(std::span<const std::string_view> names, std::string_view name) -> int {
            const auto it = std::ranges::find(names, name);
            return it == names.end() ? -1 : static_cast<int>(it - names.begin());
        }

        // Each matching token adds its bit, so a repeated token adds it twice; at most 30 tokens are read.
        auto parse_flags(std::string_view input, std::span<const std::string_view> names, char separator) -> int {
            unsigned value = 0;
            int tokens = 0;
            for (const auto part : input | std::views::split(separator)) {
                const std::string_view token(part.begin(), part.end());
                if (token.empty())
                    continue;
                if (tokens++ == 30)
                    break;
                if (const int index = index_of(names, trim(token)); index >= 0)
                    value += 1u << index;
            }
            return static_cast<int>(value);
        }

        void copy_locale_name(std::span<char> locale_name, const char *name, uint32_t vnum, const std::map<int, std::string> &name_map) {
            const auto it = name_map.find(static_cast<int>(vnum));
            utf8_strlcpy(locale_name, it != name_map.end() ? std::string_view(it->second) : std::string_view(name));
        }

        auto get_item_type_value(std::string_view input) -> int { return index_of(item_types, input); }

        auto get_item_sub_type_value(int type_value, std::string_view input) -> int {
            assert(item_sub_types.size() > static_cast<size_t>(type_value) && "Subtype rule: Out of range!!");

            if (static_cast<int>(item_sub_types.size()) <= type_value) {
                metin2::log::error("Item type {:d} is out of range for subtype lookup ({:d} types registered)", type_value, item_sub_types.size());
                return -1;
            }

            const auto sub_types = item_sub_types[type_value];
            return sub_types.empty() ? 0 : index_of(sub_types, trim(input));
        }

        auto get_item_anti_flag_value(std::string_view input) -> int { return parse_flags(input, item_anti_flags, '|'); }
        auto get_item_flag_value(std::string_view input) -> int { return parse_flags(input, item_flags, '|'); }
        auto get_item_wear_flag_value(std::string_view input) -> int { return parse_flags(input, item_wear_flags, '|'); }
        auto get_item_immune_value(std::string_view input) -> int { return parse_flags(input, item_immune_flags, '|'); }
        auto get_item_limit_type_value(std::string_view input) -> int { return index_of(item_limit_types, trim(input)); }
        auto get_item_apply_type_value(std::string_view input) -> int { return index_of(item_apply_types, trim(input)); }

        auto get_mob_rank_value(std::string_view input) -> int { return index_of(mob_ranks, trim(input)); }
        auto get_mob_type_value(std::string_view input) -> int { return index_of(mob_types, trim(input)); }
        auto get_mob_battle_type_value(std::string_view input) -> int { return index_of(mob_battle_types, trim(input)); }
        auto get_mob_size_value(std::string_view input) -> int { return index_of(mob_sizes, trim(input)) + 1; }
        auto get_mob_ai_flag_value(std::string_view input) -> int { return parse_flags(input, mob_ai_flags, ','); }
        auto get_mob_race_flag_value(std::string_view input) -> int { return parse_flags(input, mob_race_flags, ','); }
        auto get_mob_immune_flag_value(std::string_view input) -> int { return parse_flags(input, mob_immune_flags, ','); }
    }

    auto set_proto_mob_table(mob_table *mob_table_value, c_csv_table &csv_table, const std::map<int, std::string> &name_map) -> bool {
        int col = 0;
        str_to_number(mob_table_value->vnum, csv_table.as_string_by_index(col++));
        utf8_strlcpy(mob_table_value->name, csv_table.as_string_by_index(col++));

        copy_locale_name(mob_table_value->locale_name, mob_table_value->name, mob_table_value->vnum, name_map);

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
        str_to_number(mob_table_value->regen_percent, csv_table.as_string_by_index(col++));
        str_to_number(mob_table_value->gold_min, csv_table.as_string_by_index(col++));
        str_to_number(mob_table_value->gold_max, csv_table.as_string_by_index(col++));
        str_to_number(mob_table_value->exp, csv_table.as_string_by_index(col++));
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

        metin2::log::debug("Mob {1} (vnum {0:d}) level {2:d} rank {3:d} empire {4:d}", mob_table_value->vnum, mob_table_value->locale_name, mob_table_value->level, mob_table_value->rank, mob_table_value->empire);

        return true;
    }

    auto set_proto_item_table(item_table *item_table_value, c_csv_table &csv_table, const std::map<int, std::string> &name_map) -> bool {
        int col = 0;

        int dataArray[33];
        for (size_t i = 0; i < sizeof(dataArray) / sizeof(dataArray[0]); i++) {
            int validCheck = 0;
            if (i == 2) {
                dataArray[i] = get_item_type_value(csv_table.as_string_by_index(col));
                validCheck = dataArray[i];
            } else if (i == 3) {
                dataArray[i] = get_item_sub_type_value(dataArray[i - 1], csv_table.as_string_by_index(col));
                validCheck = dataArray[i];
            } else if (i == 5) {
                dataArray[i] = get_item_anti_flag_value(csv_table.as_string_by_index(col));
                validCheck = dataArray[i];
            } else if (i == 6) {
                dataArray[i] = get_item_flag_value(csv_table.as_string_by_index(col));
                validCheck = dataArray[i];
            } else if (i == 7) {
                dataArray[i] = get_item_wear_flag_value(csv_table.as_string_by_index(col));
                validCheck = dataArray[i];
            } else if (i == 8) {
                dataArray[i] = get_item_immune_value(csv_table.as_string_by_index(col));
                validCheck = dataArray[i];
            } else if (i == 14) {
                dataArray[i] = get_item_limit_type_value(csv_table.as_string_by_index(col));
                validCheck = dataArray[i];
            } else if (i == 16) {
                dataArray[i] = get_item_limit_type_value(csv_table.as_string_by_index(col));
                validCheck = dataArray[i];
            } else if (i == 18) {
                dataArray[i] = get_item_apply_type_value(csv_table.as_string_by_index(col));
                validCheck = dataArray[i];
            } else if (i == 20) {
                dataArray[i] = get_item_apply_type_value(csv_table.as_string_by_index(col));
                validCheck = dataArray[i];
            } else if (i == 22) {
                dataArray[i] = get_item_apply_type_value(csv_table.as_string_by_index(col));
                validCheck = dataArray[i];
            } else {
                str_to_number(dataArray[i], csv_table.as_string_by_index(col));
            }

            if (validCheck == -1) {
                std::ostringstream dataStream;

                for (size_t j = 0; j < i; ++j)
                    dataStream << dataArray[j] << ",";

                metin2::log::error("Item proto: invalid value in field {:d} (column {:d}): {}", i, col, csv_table.as_string_by_index(col));
                metin2::log::error("Item proto: values of fields 0 to {:d}: {}", i, dataStream.str());

                exit(0);
            }

            col = col + 1;
        }

        {
            std::string s(csv_table.as_string_by_index(0));
            size_t pos = s.find("~");
            if (std::string::npos == pos) {
                item_table_value->vnum = dataArray[0];
                item_table_value->vnum_range = 0;
            } else {
                std::string s_start_vnum(s.substr(0, pos));
                std::string s_end_vnum(s.substr(pos + 1));

                int start_vnum = atoi(s_start_vnum.c_str());
                int end_vnum = atoi(s_end_vnum.c_str());
                if (0 == start_vnum || (0 != end_vnum && end_vnum < start_vnum)) {
                    metin2::log::error("Item proto: invalid vnum range {}", s);
                    return false;
                }
                item_table_value->vnum = start_vnum;
                item_table_value->vnum_range = end_vnum - start_vnum;
            }
        }

        utf8_strlcpy(item_table_value->name, csv_table.as_string_by_index(1));
        copy_locale_name(item_table_value->locale_name, item_table_value->name, item_table_value->vnum, name_map);
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

        for (i = 0; i < ITEM_LIMIT_MAX_NUM; ++i) {
            item_table_value->limits[i].type = dataArray[14 + i * 2];
            item_table_value->limits[i].value = dataArray[15 + i * 2];

            if (LIMIT_REAL_TIME_START_FIRST_USE == item_table_value->limits[i].type)
                item_table_value->c_limit_real_time_first_use_index = (char)i;

            if (LIMIT_TIMER_BASED_ON_WEAR == item_table_value->limits[i].type)
                item_table_value->c_limit_timer_based_on_wear_index = (char)i;
        }

        for (i = 0; i < ITEM_APPLY_MAX_NUM; ++i) {
            item_table_value->applies[i].type = dataArray[18 + i * 2];
            item_table_value->applies[i].value = dataArray[19 + i * 2];
        }

        for (i = 0; i < ITEM_VALUES_MAX_NUM; ++i)
            item_table_value->values[i] = dataArray[24 + i];

        item_table_value->specular = dataArray[30];

        item_table_value->gain_socket_pct = dataArray[31];
        item_table_value->s_addon_type = dataArray[32];

        str_to_number(item_table_value->weight, "0");

        return true;
    }

}
