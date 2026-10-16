#include <metin2/config/config.h>
#include <metin2/game/empire/monarch.h>

#include <metin2/game/app/config.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/world/sectree_manager.h>

#include <metin2/core/main.h>
#include <metin2/log/log.h>
#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>

#include <cstdint>
#include <cstring>
#include <iterator>

namespace metin2::game::inline empire {

    monarch::monarch() {
        memset(&monarch_info_, 0, sizeof(monarch_info_));

        initialize();
    }

    monarch::~monarch() {
    }

    auto monarch::initialize() -> bool {
        memset(power_up_, 0, sizeof(power_up_));
        memset(defense_up_, 0, sizeof(defense_up_));
        memset(power_up_ct_, 0, sizeof(power_up_ct_));
        memset(defense_up_ct_, 0, sizeof(defense_up_ct_));

        return 0;
    }

    auto monarch::heal_my_empire(character *ch, uint32_t price) -> int {
        uint8_t Empire = ch->empire();
        uint32_t pid = ch->player_id();

        metin2::log::info("Monarch heal: pid {:d}, empire {:d}, price {:d}", pid, Empire, price);

        if (is_monarch(pid, Empire) == 0) {
            if (!ch->is_gm()) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("군주의 자격을 가지고 있지 않습니다"));
                metin2::log::warn("Monarch heal refused: pid {:d} is not the monarch", pid);
                return 0;
            }
        }

        if (!ch->is_mcok(character::MI_HEAL)) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("%d 초 후에 군주의 축복을 사용할 수 있습니다"), ch->mcl_time(character::MI_HEAL));

            return 0;
        }

        if (!is_money_ok(price, Empire)) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("국고에 돈이 부족합니다. 현재 : %u 필요금액 : %u"), money(Empire), price);
            return 0;
        }

        int iMapIndex = ch->map_index();

        sectree_manager::instance().for_each(iMapIndex, [Empire](entity *ent) {
            if (ent->is_type(ENTITY_CHARACTER)) {
                character *ch = (character *)ent;

                if (ch->is_pc() && Empire == ch->empire()) {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("군주가 내린 축복으로 모든 에너지가 가득 채워집니다"));
                    ch->point_change(POINT_HP, ch->max_hp() - ch->hp());
                    ch->point_change(POINT_SP, ch->max_sp() - ch->sp());
                    ch->effect_packet(SE_SPUP_BLUE);
                    ch->effect_packet(SE_HPUP_RED);
                }
            }
        });

        sendto_db_dec_money(price, Empire, ch);

        ch->set_mc(character::MI_HEAL);

        if (metin2::config::test_server())
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("[TEST_ONLY]현재 국고 : %d "), money(Empire) - price);
        return 1;
    }

    void monarch::set_monarch_info(monarch_info *info) {
        memcpy(&monarch_info_, info, sizeof(monarch_info));
    }

    auto monarch::is_monarch(uint32_t pid, uint8_t empire) -> bool {
        if (empire >= std::size(monarch_info_.pid))
            return false;

        return (monarch_info_.pid[empire] == pid);
    }

    auto monarch::is_money_ok(int price, uint8_t empire) -> bool {
        metin2::log::debug("Monarch funds of empire {:d}: {:d}, price {:d}", empire, money(empire), price);
        return (money(empire) >= price);
    }

    auto monarch::sendto_db_add_money(int money_value, uint8_t empire, character *ch) -> bool {
        if (money(empire) + money_value > 2000000000)
            return false;

        if (money(empire) + money_value < 0)
            return false;

        int nEmpire = empire;
        db_clientdesc->db_packet_header(HEADER_GD_ADD_MONARCH_MONEY, ch->get_desc()->handle(), sizeof(int) + sizeof(int));
        db_clientdesc->packet(&nEmpire, sizeof(int));
        db_clientdesc->packet(&money_value, sizeof(int));
        return true;
    }

    auto monarch::sendto_db_dec_money(int money_value, uint8_t empire, character *ch) -> bool {
        if (empire >= std::size(monarch_info_.money))
            return false;

        if (money(empire) - money_value < 0)
            return false;

        int nEmpire = empire;

        db_clientdesc->db_packet_header(HEADER_GD_DEC_MONARCH_MONEY, ch->get_desc()->handle(), sizeof(int) + sizeof(int));
        db_clientdesc->packet(&nEmpire, sizeof(int));
        db_clientdesc->packet(&money_value, sizeof(int));
        return true;
    }

    auto monarch::add_money(int money_value, uint8_t empire) -> bool {
        if (empire >= std::size(monarch_info_.money))
            return false;

        if (money(empire) + money_value > 2000000000)
            return false;

        monarch_info_.money[empire] += money_value;
        return true;
    }

    auto monarch::dec_money(int money_value, uint8_t empire) -> bool {
        if (empire >= std::size(monarch_info_.money))
            return false;

        if (money(empire) - money_value < 0)
            return false;

        monarch_info_.money[empire] -= money_value;
        return true;
    }

    auto monarch::money(uint8_t empire) -> int {
        if (empire >= std::size(monarch_info_.money))
            return 0;

        return monarch_info_.money[empire];
    }

    auto monarch::get_monarch() -> monarch_info * {
        return &monarch_info_;
    }

    auto monarch::monarch_pid(uint8_t empire) -> uint32_t {
        return empire < std::size(monarch_info_.pid) ? monarch_info_.pid[empire] : 0;
    }

    auto monarch::is_power_up(uint8_t empire) -> bool {
        return empire < std::size(power_up_) ? power_up_[empire] : false;
    }

    auto monarch::is_defence_up(uint8_t empire) -> bool {
        return empire < std::size(defense_up_) ? defense_up_[empire] : false;
    }

    auto monarch::check_power_up_ct(uint8_t empire) -> bool {
        if (empire >= std::size(power_up_ct_))
            return false;

        if (power_up_ct_[empire] > thecore_pulse()) {
            if (metin2::config::test_server())
                metin2::log::debug("Monarch power up on cooldown until pulse {:d} (now {:d}, cooldown {:d} pulses)", power_up_ct_[empire], thecore_pulse(), passes_per_sec(60 * 10));
            return false;
        }

        return true;
    }

    auto monarch::check_defense_up_ct(uint8_t empire) -> bool {
        if (empire >= std::size(defense_up_ct_))
            return false;

        if (defense_up_ct_[empire] > thecore_pulse()) {
            if (metin2::config::test_server())
                metin2::log::debug("Monarch defence up on cooldown until pulse {:d} (now {:d}, cooldown {:d} pulses)", defense_up_ct_[empire], thecore_pulse(), passes_per_sec(60 * 10));
            return false;
        }

        return true;
    }

    void monarch::power_up(uint8_t empire, bool on) {
        if (empire >= std::size(power_up_ct_))
            return;

        power_up_[empire] = on;

        power_up_ct_[empire] = thecore_pulse() + passes_per_sec(60 * 10);
    }

    void monarch::defense_up(uint8_t empire, bool on) {
        if (empire >= std::size(defense_up_ct_))
            return;

        defense_up_[empire] = on;

        defense_up_ct_[empire] = thecore_pulse() + passes_per_sec(60 * 10);
    }

    auto is_monarch_warp_zone(int map_idx) -> bool {
        if (map_idx >= 10000)
            map_idx /= 10000;

        switch (map_idx) {
        case 301:
        case 302:
        case 303:
        case 304:
        case 351:
        case 352:
            return false;
        }

        return (map_idx != 208 && map_idx != 216 && map_idx != 217);
    }
}
