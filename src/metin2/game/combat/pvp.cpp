#include <metin2/config/config.h>
#include <metin2/game/combat/pvp.h>

#include <metin2/game/app/crc32.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/desc_manager.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/world/sectree_manager.h>

#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/character_types.h>
#include <metin2/proto/chat_types.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <unordered_set>

namespace metin2::game::inline combat {

    pvp::pvp(uint32_t pid1, uint32_t pid2) {
        if (pid1 > pid2) {
            players_[0].pid = pid1;
            players_[1].pid = pid2;
            players_[0].agree = true;
        } else {
            players_[0].pid = pid2;
            players_[1].pid = pid1;
            players_[1].agree = true;
        }

        uint32_t adw_id[2];
        adw_id[0] = players_[0].pid;
        adw_id[1] = players_[1].pid;
        crc_ = get_fast_hash((const char *)&adw_id, 8);
        revenge_ = false;

        set_last_fight_time();
    }

    pvp::pvp(pvp &k) {
        players_[0] = k.players_[0];
        players_[1] = k.players_[1];

        crc_ = k.crc_;
        revenge_ = k.revenge_;

        set_last_fight_time();
    }

    pvp::~pvp() = default;

    void pvp::packet(bool removing) {
        if (!players_[0].vid || !players_[1].vid) {
            if (removing)
                metin2::log::error("PvP pair has a null vid on removal (vids {:d}, {:d})", players_[0].vid, players_[1].vid);

            return;
        }

        packet_gcpvp pack;

        pack.header = HEADER_GC_PVP;

        if (removing) {
            pack.mode = PVP_MODE_NONE;
            pack.vid_src = players_[0].vid;
            pack.vid_dst = players_[1].vid;
        } else if (is_fight()) {
            pack.mode = PVP_MODE_FIGHT;
            pack.vid_src = players_[0].vid;
            pack.vid_dst = players_[1].vid;
        } else {
            pack.mode = revenge_ ? PVP_MODE_REVENGE : PVP_MODE_AGREE;

            if (players_[0].agree) {
                pack.vid_src = players_[0].vid;
                pack.vid_dst = players_[1].vid;
            } else {
                pack.vid_src = players_[1].vid;
                pack.vid_dst = players_[0].vid;
            }
        }

        const desc_manager::desc_set &descs = get_desc_manager().client_set();
        auto it = descs.begin();

        while (it != descs.end()) {
            desc *d = *it++;

            if (d->is_phase(PHASE_GAME) || d->is_phase(PHASE_DEAD))
                d->packet(&pack, sizeof(pack));
        }
    }

    auto pvp::agree(uint32_t pid_value) -> bool {
        players_[players_[0].pid != pid_value ? 1 : 0].agree = true;

        if (is_fight()) {
            packet();
            return true;
        }

        return false;
    }

    auto pvp::is_fight() -> bool {
        return (players_[0].agree == players_[1].agree) && players_[0].agree;
    }

    void pvp::win(uint32_t pid_value) {
        int slot = players_[0].pid != pid_value ? 1 : 0;

        revenge_ = true;

        players_[slot].agree = true;
        players_[!slot].can_revenge = true;
        players_[!slot].agree = false;

        packet();
    }

    void pvp::set_vid(uint32_t pid_value, uint32_t vid_value) {
        if (players_[0].pid == pid_value)
            players_[0].vid = vid_value;
        else
            players_[1].vid = vid_value;
    }

    void pvp::set_last_fight_time() {
        last_fight_time_ = get_dword_time();
    }

    auto pvp::last_fight_time() -> uint32_t {
        return last_fight_time_;
    }

    pvp_manager::pvp_manager() = default;

    pvp_manager::~pvp_manager() = default;

    void pvp_manager::insert(character *chr, character *victim) {
        if (chr->is_dead() || victim->is_dead())
            return;

        pvp pvp_value(chr->player_id(), victim->player_id());

        pvp *pk_pvp;

        if ((pk_pvp = find(pvp_value.crc_))) {
            if (pk_pvp->agree(chr->player_id())) {
                victim->chat_packet(CHAT_TYPE_INFO, lc_text("%s님과의 대결 시작!"), chr->name());
                chr->chat_packet(CHAT_TYPE_INFO, lc_text("%s님과의 대결 시작!"), victim->name());
            }
            return;
        }

        auto new_pvp = std::make_unique<pvp>(pvp_value);
        pk_pvp = new_pvp.get();

        pk_pvp->set_vid(chr->player_id(), chr->get_vid());
        pk_pvp->set_vid(victim->player_id(), victim->get_vid());

        pvps_.emplace(pk_pvp->crc_, std::move(new_pvp));

        pvp_sets_by_id_[chr->player_id()].insert(pk_pvp);
        pvp_sets_by_id_[victim->player_id()].insert(pk_pvp);

        pk_pvp->packet();

        char msg[CHAT_MAX_LEN + 1];
        snprintf(msg, sizeof(msg), lc_text("%s님이 대결신청을 했습니다. 승낙하려면 대결동의를 하세요."), chr->name());

        victim->chat_packet(CHAT_TYPE_INFO, msg);
        chr->chat_packet(CHAT_TYPE_INFO, lc_text("%s에게 대결신청을 했습니다."), victim->name());

        desc *victim_desc = victim->get_desc();
        if (victim_desc) {
            packet_gc_whisper pack;

            int len = std::min<int>(CHAT_MAX_LEN, strlen(msg) + 1);

            pack.header = HEADER_GC_WHISPER;
            pack.size = sizeof(packet_gc_whisper) + len;
            pack.type = WHISPER_TYPE_SYSTEM;
            strlcpy(pack.name_from, chr->name(), sizeof(pack.name_from));

            temp_buffer buf;

            buf.write(&pack, sizeof(packet_gc_whisper));
            buf.write(msg, len);

            victim_desc->packet(buf.read_peek(), buf.size());
        }
    }

    void pvp_manager::connect_ex(character *chr, bool disconnect) {
        auto it = pvp_sets_by_id_.find(chr->player_id());

        if (it == pvp_sets_by_id_.end())
            return;

        uint32_t vid = disconnect ? 0 : chr->get_vid();

        auto it2 = it->second.begin();

        while (it2 != it->second.end()) {
            pvp *pvp = *it2++;
            pvp->set_vid(chr->player_id(), vid);
        }
    }

    void pvp_manager::connect(character *chr) {
        connect_ex(chr, false);
    }

    void pvp_manager::disconnect(character *chr) {
    }

    auto pvp_manager::dead(character *chr, uint32_t killer_pid) -> bool {
        auto it = pvp_sets_by_id_.find(chr->player_id());

        if (it == pvp_sets_by_id_.end())
            return false;

        bool found = false;

        metin2::log::debug("Pvp: player {:d} died", chr->player_id());
        auto it2 = it->second.begin();

        while (it2 != it->second.end()) {
            pvp *pvp = *it2++;

            uint32_t companion_pid;

            if (pvp->players_[0].pid == chr->player_id())
                companion_pid = pvp->players_[1].pid;
            else
                companion_pid = pvp->players_[0].pid;

            if (companion_pid == killer_pid) {
                if (pvp->is_fight()) {
                    pvp->set_last_fight_time();
                    pvp->win(killer_pid);
                    found = true;
                    break;
                } else if (get_dword_time() - pvp->last_fight_time() <= 15000) {
                    found = true;
                    break;
                }
            }
        }

        return found;
    }

    auto pvp_manager::can_attack(character *chr, character *victim) -> bool {
        switch (victim->get_char_type()) {
        case CHAR_TYPE_NPC:
        case CHAR_TYPE_WARP:
        case CHAR_TYPE_GOTO:
            return false;
        }

        if (chr == victim)
            return false;

        if (victim->is_npc() && chr->is_npc() && !chr->is_guard_npc())
            return false;

        if (true == chr->is_horse_riding()) {
            if (chr->horse_level() > 0 && 1 == chr->horse_grade())
                return false;
        } else {
            switch (chr->get_mount_vnum()) {
            case 0:
            case 20030:
            case 20110:
            case 20111:
            case 20112:
            case 20113:
            case 20114:
            case 20115:
            case 20116:
            case 20117:
            case 20118:
            case 20205:
            case 20206:
            case 20207:
            case 20208:
            case 20209:
            case 20210:
            case 20211:
            case 20212:
            case 20119:
            case 20219:
            case 20220:
            case 20221:
            case 20222:
            case 20120:
            case 20121:
            case 20122:
            case 20123:
            case 20124:
            case 20125:
            case 20214:
            case 20215:
            case 20217:
            case 20218:
            case 20224:
            case 20229:
            case 20225:
            case 20230:
                break;

            default:
                return false;
            }
        }

        if (victim->is_npc() || chr->is_npc()) {
            return true;
        }

        if (victim->is_observer_mode() || chr->is_observer_mode())
            return false;

        {
            uint8_t map_empire = get_sectree_manager().empire_from_map_index(chr->map_index());

            if ((chr->pk_mode() == PK_MODE_PROTECT && chr->empire() == map_empire) ||
                (victim->pk_mode() == PK_MODE_PROTECT && victim->empire() == map_empire)) {
                return false;
            }
        }

        if (chr->empire() != victim->empire()) {

            return true;
        }

        bool be_killer_mode = false;

        if (victim->get_party() && victim->get_party() == chr->get_party()) {
            return false;
        } else {
            if (victim->is_killer_mode()) {
                return true;
            }

            if (chr->alignment() < 0 && victim->alignment() >= 0) {
                if (metin2::config::game().protect_normal_player) {
                    if (PK_MODE_PEACE == victim->pk_mode())
                        return false;
                }
            }

            switch (chr->pk_mode()) {
            case PK_MODE_PEACE:
            case PK_MODE_REVENGE:
                if (victim->get_guild() && victim->get_guild() == chr->get_guild())
                    break;

                if (chr->pk_mode() == PK_MODE_REVENGE) {
                    if (chr->alignment() < 0 && victim->alignment() >= 0) {
                        chr->set_killer_mode(true);
                        return true;
                    } else if (chr->alignment() >= 0 && victim->alignment() < 0)
                        return true;
                }
                break;

            case PK_MODE_GUILD:
                if (!chr->get_guild() || (victim->get_guild() != chr->get_guild())) {
                    if (victim->alignment() >= 0)
                        chr->set_killer_mode(true);
                    else if (chr->alignment() < 0 && victim->alignment() < 0)
                        chr->set_killer_mode(true);

                    return true;
                }
                break;

            case PK_MODE_FREE:
                if (victim->alignment() >= 0)
                    chr->set_killer_mode(true);
                else if (chr->alignment() < 0 && victim->alignment() < 0)
                    chr->set_killer_mode(true);

                return true;
                break;
            }
        }

        pvp pvp_value(chr->player_id(), victim->player_id());
        pvp *pvp = find(pvp_value.crc_);

        if (!pvp || !pvp->is_fight()) {
            if (be_killer_mode)
                chr->set_killer_mode(true);

            return (be_killer_mode);
        }

        pvp->set_last_fight_time();
        return true;
    }

    auto pvp_manager::find(uint32_t crc) -> pvp * {
        auto it = pvps_.find(crc);

        if (it == pvps_.end())
            return nullptr;

        return it->second.get();
    }

    void pvp_manager::remove(pvp *pvp_value) {
        auto it = pvps_.find(pvp_value->crc_);

        if (it == pvps_.end())
            return;

        pvp_sets_by_id_[pvp_value->players_[0].pid].erase(pvp_value);
        pvp_sets_by_id_[pvp_value->players_[1].pid].erase(pvp_value);
        pvps_.erase(it);
    }

    void pvp_manager::send_list(desc *d) {
        auto it = pvps_.begin();

        uint32_t vid_value = d->get_character()->get_vid();

        packet_gcpvp pack;

        pack.header = HEADER_GC_PVP;

        while (it != pvps_.end()) {
            pvp *pvp = (it++)->second.get();

            if (!pvp->players_[0].vid || !pvp->players_[1].vid)
                continue;

            if (pvp->is_fight()) {
                pack.mode = PVP_MODE_FIGHT;
                pack.vid_src = pvp->players_[0].vid;
                pack.vid_dst = pvp->players_[1].vid;
            } else {
                pack.mode = pvp->revenge_ ? PVP_MODE_REVENGE : PVP_MODE_AGREE;

                if (pvp->players_[0].agree) {
                    pack.vid_src = pvp->players_[0].vid;
                    pack.vid_dst = pvp->players_[1].vid;
                } else {
                    pack.vid_src = pvp->players_[1].vid;
                    pack.vid_dst = pvp->players_[0].vid;
                }
            }

            d->packet(&pack, sizeof(pack));
            metin2::log::debug("Pvp: sending pair vid {:d} vs {:d}", pack.vid_src, pack.vid_dst);

            if (pvp->players_[0].vid == vid_value) {
                character *ch = get_character_manager().find(pvp->players_[1].vid);
                if (ch && ch->get_desc()) {
                    desc *d = ch->get_desc();
                    d->packet(&pack, sizeof(pack));
                }
            } else if (pvp->players_[1].vid == vid_value) {
                character *ch = get_character_manager().find(pvp->players_[0].vid);
                if (ch && ch->get_desc()) {
                    desc *d = ch->get_desc();
                    d->packet(&pack, sizeof(pack));
                }
            }
        }
    }

    void pvp_manager::process() {
        auto it = pvps_.begin();

        while (it != pvps_.end()) {
            pvp *pvp = (it++)->second.get();

            if (get_dword_time() - pvp->last_fight_time() > 600000) {
                pvp->packet(true);
                remove(pvp);
            }
        }
    }
}
