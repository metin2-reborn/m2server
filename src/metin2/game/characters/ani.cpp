/*********************************************************************
 * date        : 2007.11.16
 * file        : ani.cpp
 * author      : mhh
 * description :
 */

#include <metin2/game/characters/ani.h>

#include <metin2/game/characters/char.h>
#include <metin2/game/items/item.h>
#include <metin2/game/app/typedef.h>

#include <metin2/proto/length.h>
#include <metin2/log/log.h>

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
    // [race][normal0 mounted1][weapon][combo]
    uint32_t speed_[MAIN_RACE_MAX_NUM][2][WEAPON_NUM_TYPES][9];

  public:
    ani();

  public:
    bool load();
    bool load_one_race(int race, const char *dir_name);
    uint32_t load_one_weapon(const char *dir_name, int weapon, uint8_t combo, bool horse);
    uint32_t attack_speed(int race, int weapon, uint8_t combo = 0, bool horse = false);
};

static class ani s_ani;

uint32_t fn_attack_speed_from_file(const char *file) {
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
                float f_speed = strtof(value, nullptr);
                speed = (int)(f_speed * 1000.0);
                break;
            }
        }
    }

    fclose(fp);
    return speed;
}

ani::ani() {
    // set default value
    for (int race = 0; race < MAIN_RACE_MAX_NUM; ++race) {
        for (int weapon = 0; weapon < WEAPON_NUM_TYPES; ++weapon) {
            for (uint8_t combo = 0; combo <= 8; ++combo) {
                speed_[race][0][weapon][combo] = 1000;
                speed_[race][1][weapon][combo] = 1000;
            }
        }
    }
}

bool ani::load() {
    const char *dir_name[MAIN_RACE_MAX_NUM] = {
        "data/pc/warrior",   // warrior (male)
        "data/pc/assassin",  // assassin (female)
        "data/pc/sura",      // sura (male)
        "data/pc/shaman",    // shaman (female)
        "data/pc2/warrior",  // warrior (female)
        "data/pc2/assassin", // assassin (male)
        "data/pc2/sura",     // sura (female)
        "data/pc2/shaman"    // shaman (male)
    };

    for (int race = 0; race < MAIN_RACE_MAX_NUM; ++race) {
        if (false == load_one_race(race, dir_name[race])) {
            metin2::log::error("ANI directory = {}", dir_name[race]);
            return false;
        }
    }

    return true;
}

uint32_t ani::load_one_weapon(const char *dir_name, int weapon, uint8_t combo, bool horse) {
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

bool ani::load_one_race(int race, const char *dir_name) {
    if (nullptr == dir_name || '\0' == dir_name[0])
        return false;

    for (int weapon = WEAPON_SWORD; weapon < WEAPON_NUM_TYPES; ++weapon) {
        metin2::log::debug( "ANI ({},{})", fn_race_name(race), fn_weapon_type(weapon));

        for (uint8_t combo = 1; combo <= 8; ++combo) {
            // when not riding a horse
            speed_[race][0][weapon][combo] = load_one_weapon(dir_name, weapon, combo, false);
            speed_[race][0][weapon][0] = std::min<int>(speed_[race][0][weapon][0], speed_[race][0][weapon][combo]); // minimum

            // when riding a horse
            speed_[race][1][weapon][combo] = load_one_weapon(dir_name, weapon, combo, true);
            speed_[race][1][weapon][0] = std::min<int>(speed_[race][1][weapon][0], speed_[race][1][weapon][combo]); // minimum

            metin2::log::debug( "combo{:02d} speed={:d} horse={:d}",
                combo, speed_[race][0][weapon][combo], speed_[race][1][weapon][combo]);
        }

        metin2::log::debug( "minspeed={:d}", speed_[race][0][weapon][0]);
    }

    return true;
}

uint32_t ani::attack_speed(int race, int weapon, uint8_t combo, bool horse) {
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

uint32_t ani_attack_speed(character * ch) {
    uint32_t speed = 1000;

    if (nullptr == ch)
        return speed;

    item * item = ch->wear(WEAR_WEAPON);

    if (nullptr == item)
        return speed;

    if (ITEM_WEAPON != item->type())
        return speed;

    int race = ch->race_num();
    int weapon = item->sub_type();

    /* for two-handed swords, the triple-slash attack and riding */
    /* have many errors, so treat as one-handed sword speed */
    if (weapon == WEAPON_TWO_HANDED)
        weapon = WEAPON_SWORD;

    return s_ani.attack_speed(race, weapon);
}

uint32_t ani_combo_speed(character * ch, uint8_t combo) {
    item * item = ch->wear(WEAR_WEAPON);

    if (nullptr == item || combo > 8)
        return 1000;

    return s_ani.attack_speed(ch->race_num(), item->sub_type(), combo, ch->is_riding());
}
}
