
#include <metin2/game/characters/ani.h>

#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/items/item.h>

#include <metin2/log/log.h>
#include <metin2/proto/inventory.h>
#include <metin2/proto/item_types.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <strings.h>

namespace metin2::game::inline characters {

    auto fn_race_name(int race) -> const char * {
#define FN_NAME(race) \
    case race: return #race
        switch (race) {
            FN_NAME(MAIN_RACE_WARRIOR_M);
            FN_NAME(MAIN_RACE_ASSASSIN_W);
            FN_NAME(MAIN_RACE_SURA_M);
            FN_NAME(MAIN_RACE_SHAMAN_W);
            FN_NAME(MAIN_RACE_WARRIOR_W);
            FN_NAME(MAIN_RACE_ASSASSIN_M);
            FN_NAME(MAIN_RACE_SURA_W);
            FN_NAME(MAIN_RACE_SHAMAN_M);
            FN_NAME(MAIN_RACE_MAX_NUM);
        }

        return "UNKNOWN";
#undef FN_NAME
    }

    auto fn_weapon_type(int weapon) -> const char * {
#define FN_NAME(weapon) \
    case weapon: return #weapon
        switch (weapon) {
            FN_NAME(WEAPON_SWORD);
            FN_NAME(WEAPON_DAGGER);
            FN_NAME(WEAPON_BOW);
            FN_NAME(WEAPON_TWO_HANDED);
            FN_NAME(WEAPON_BELL);
            FN_NAME(WEAPON_FAN);
            FN_NAME(WEAPON_ARROW);
            FN_NAME(WEAPON_MOUNT_SPEAR);
            FN_NAME(WEAPON_NUM_TYPES);
        }

        return "UNKNOWN";
#undef FN_NAME
    }

    class ani {
      protected:
        uint32_t speed_[MAIN_RACE_MAX_NUM][2][WEAPON_NUM_TYPES][9];

      public:
        ani();

      public:
        auto load() -> bool;
        auto load_one_race(int race, const char *dir_name) -> bool;
        auto load_one_weapon(const char *dir_name, int weapon, uint8_t combo, bool horse) -> uint32_t;
        auto attack_speed(int race, int weapon, uint8_t combo = 0, bool horse = false) -> uint32_t;
    };

    static class ani s_ani;

    auto fn_attack_speed_from_file(const char *file) -> uint32_t {
        FILE *fp = fopen(file, "r");

        if (nullptr == fp)
            return 0;

        int speed = 1000;

        const char *key = "DirectInputTime";
        const char *delim = " \t\r\n";
        const char *field, *value;

        char buf[1024];

        while (fgets(buf, 1024, fp)) {
            field = strtok(buf, delim);
            value = strtok(nullptr, delim);

            if (field && value) {
                if (0 == strcasecmp(field, key)) {
                    float speed_value = strtof(value, nullptr);
                    speed = (int)(speed_value * 1000.0);
                    break;
                }
            }
        }

        fclose(fp);
        return speed;
    }

    ani::ani() {
        for (int race = 0; race < MAIN_RACE_MAX_NUM; ++race) {
            for (int weapon = 0; weapon < WEAPON_NUM_TYPES; ++weapon) {
                for (uint8_t combo = 0; combo <= 8; ++combo) {
                    speed_[race][0][weapon][combo] = 1000;
                    speed_[race][1][weapon][combo] = 1000;
                }
            }
        }
    }

    auto ani::load() -> bool {
        const char *dir_name[MAIN_RACE_MAX_NUM] = {
            "data/pc/warrior",
            "data/pc/assassin",
            "data/pc/sura",
            "data/pc/shaman",
            "data/pc2/warrior",
            "data/pc2/assassin",
            "data/pc2/sura",
            "data/pc2/shaman"};

        for (int race = 0; race < MAIN_RACE_MAX_NUM; ++race) {
            if (false == load_one_race(race, dir_name[race])) {
                metin2::log::error("Cannot load animation data from {}", dir_name[race]);
                return false;
            }
        }

        return true;
    }

    auto ani::load_one_weapon(const char *dir_name, int weapon, uint8_t combo, bool horse) -> uint32_t {
        const char *format;
        char filename[256];

        switch (weapon) {
        case WEAPON_SWORD:
            format = "%s/%sonehand_sword/combo_%02d.msa";
            break;

        case WEAPON_DAGGER:
            format = "%s/%sdualhand_sword/combo_%02d.msa";
            break;

        case WEAPON_BOW:
            format = "%s/%sbow/attack.msa";
            break;

        case WEAPON_TWO_HANDED:
            format = "%s/%stwohand_sword/combo_%02d.msa";
            break;

        case WEAPON_BELL:
            format = "%s/%sbell/combo_%02d.msa";
            break;

        case WEAPON_FAN:
            format = "%s/%sfan/combo_%02d.msa";
            break;

        default:
            return 1000;
        }

        snprintf(filename, sizeof(filename), format, dir_name, horse ? "horse_" : "", combo);
        uint32_t speed = fn_attack_speed_from_file(filename);

        if (speed == 0)
            return 1000;

        return speed;
    }

    auto ani::load_one_race(int race, const char *dir_name) -> bool {
        if (nullptr == dir_name || '\0' == dir_name[0])
            return false;

        for (int weapon = WEAPON_SWORD; weapon < WEAPON_NUM_TYPES; ++weapon) {
            metin2::log::debug("Animation speeds for {} with {}", fn_race_name(race), fn_weapon_type(weapon));

            for (uint8_t combo = 1; combo <= 8; ++combo) {
                speed_[race][0][weapon][combo] = load_one_weapon(dir_name, weapon, combo, false);
                speed_[race][0][weapon][0] = std::min<int>(speed_[race][0][weapon][0], speed_[race][0][weapon][combo]);

                speed_[race][1][weapon][combo] = load_one_weapon(dir_name, weapon, combo, true);
                speed_[race][1][weapon][0] = std::min<int>(speed_[race][1][weapon][0], speed_[race][1][weapon][combo]);

                metin2::log::debug("Combo {:d}: speed {:d}, on horse {:d}",
                    combo, speed_[race][0][weapon][combo], speed_[race][1][weapon][combo]);
            }

            metin2::log::debug("Min speed {:d}", speed_[race][0][weapon][0]);
        }

        return true;
    }

    auto ani::attack_speed(int race, int weapon, uint8_t combo, bool horse) -> uint32_t {
        switch (race) {
        case MAIN_RACE_WARRIOR_M:
        case MAIN_RACE_ASSASSIN_W:
        case MAIN_RACE_SURA_M:
        case MAIN_RACE_SHAMAN_W:
        case MAIN_RACE_WARRIOR_W:
        case MAIN_RACE_ASSASSIN_M:
        case MAIN_RACE_SURA_W:
        case MAIN_RACE_SHAMAN_M:
            break;
        default:
            return 1000;
        }

        switch (weapon) {
        case WEAPON_SWORD:
        case WEAPON_DAGGER:
        case WEAPON_BOW:
        case WEAPON_TWO_HANDED:
        case WEAPON_BELL:
        case WEAPON_FAN:
        case WEAPON_ARROW:
        case WEAPON_MOUNT_SPEAR:
            break;
        default:
            return 1000;
        }

        return speed_[race][horse ? 1 : 0][weapon][combo];
    }

    void ani_init() {
        s_ani.load();
    }

    auto ani_attack_speed(character *ch) -> uint32_t {
        uint32_t speed = 1000;

        if (nullptr == ch)
            return speed;

        item *item = ch->wear(WEAR_WEAPON);

        if (nullptr == item)
            return speed;

        if (ITEM_WEAPON != item->type())
            return speed;

        int race = ch->race_num();
        int weapon = item->sub_type();

        if (weapon == WEAPON_TWO_HANDED)
            weapon = WEAPON_SWORD;

        return s_ani.attack_speed(race, weapon);
    }

    auto ani_combo_speed(character *ch, uint8_t combo) -> uint32_t {
        item *item = ch->wear(WEAR_WEAPON);

        if (nullptr == item || combo > 8)
            return 1000;

        return s_ani.attack_speed(ch->race_num(), item->sub_type(), combo, ch->is_riding());
    }
}
