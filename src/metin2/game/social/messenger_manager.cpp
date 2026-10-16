#include <metin2/game/social/messenger_manager.h>

#include <metin2/game/admin/gm.h>
#include <metin2/game/app/crc32.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/p2p.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/text/locale.hpp>

#include <metin2/log/log.h>
#include <metin2/proto/character_types.h>
#include <metin2/proto/chat_types.h>
#include <metin2/sql/async_sql.h>
#include <metin2/sql/query.h>

#include <mysql.h>

#include <cstdint>
#include <cstring>
#include <format>
#include <map>
#include <set>
#include <string>

#include <sys/types.h>

namespace metin2::game::inline social {

    messenger_manager::messenger_manager() = default;

    messenger_manager::~messenger_manager() = default;

    void messenger_manager::initialize() {
    }

    void messenger_manager::p2p_login(messenger_manager::key_a account) {
        login(account);
    }

    void messenger_manager::p2p_logout(messenger_manager::key_a account) {
        logout(account);
    }

    void messenger_manager::login(messenger_manager::key_a account) {
        if (set_login_account_.find(account) != set_login_account_.end())
            return;

        get_db_manager().func_query([this](sql_msg &msg) { load_list(msg); },
            sql::format("SELECT account, companion FROM messenger_list WHERE account={}", account));

        set_login_account_.insert(account);
    }

    void messenger_manager::load_list(sql_msg &msg) {
        if (nullptr == msg.get())
            return;

        if (msg.get()->num_rows == 0)
            return;

        std::string account;

        for (uint i = 0; i < msg.get()->num_rows; ++i) {
            MYSQL_ROW row = mysql_fetch_row(msg.get()->res.get());

            if (row[0] && row[1]) {
                if (account.length() == 0)
                    account = row[0];

                relation_[row[0]].insert(row[1]);
                inverse_relation_[row[1]].insert(row[0]);
            }
        }

        send_list(account);

        std::set<messenger_manager::key_t>::iterator it;

        for (it = inverse_relation_[account].begin(); it != inverse_relation_[account].end(); ++it)
            send_login(*it, account);
    }

    void messenger_manager::logout(messenger_manager::key_a account) {
        if (set_login_account_.find(account) == set_login_account_.end())
            return;

        set_login_account_.erase(account);

        std::set<messenger_manager::key_t>::iterator it;

        for (it = inverse_relation_[account].begin(); it != inverse_relation_[account].end(); ++it) {
            send_logout(*it, account);
        }

        auto it2 = relation_.begin();

        while (it2 != relation_.end()) {
            it2->second.erase(account);
            ++it2;
        }

        relation_.erase(account);
    }

    void messenger_manager::request_to_add(character *ch, character *target) {
        if (!ch->is_pc() || !target->is_pc())
            return;

        if (quest::get_quest_manager().pc_force(ch->player_id())->is_running() == true) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("상대방이 친구 추가를 받을 수 없는 상태입니다."));
            return;
        }

        if (quest::get_quest_manager().pc_force(target->player_id())->is_running() == true)
            return;

        uint32_t dw1 = get_cr_c32(ch->name(), strlen(ch->name()));
        uint32_t dw2 = get_cr_c32(target->name(), strlen(target->name()));

        const std::string buf = std::format("{}:{}", dw1, dw2);
        uint32_t complex = get_cr_c32(buf.c_str(), buf.size());

        set_request_to_add_.insert(complex);

        target->chat_packet(CHAT_TYPE_COMMAND, "messenger_auth %s", ch->name());
    }

    auto messenger_manager::auth_to_add(messenger_manager::key_a account, messenger_manager::key_a companion, bool deny) -> bool {
        uint32_t dw1 = get_cr_c32(companion.c_str(), companion.length());
        uint32_t dw2 = get_cr_c32(account.c_str(), account.length());

        const std::string buf = std::format("{}:{}", dw1, dw2);
        uint32_t complex = get_cr_c32(buf.c_str(), buf.size());

        if (set_request_to_add_.find(complex) == set_request_to_add_.end()) {
            metin2::log::warn("Messenger: no pending friend request between {} and {}", companion, account);
            return false;
        }

        set_request_to_add_.erase(complex);

        if (!deny) {
            add_to_list(companion, account);
            add_to_list(account, companion);
        }

        return true;
    }

    void messenger_manager::apply_add_to_list(messenger_manager::key_a account, messenger_manager::key_a companion) {
        relation_[account].insert(companion);
        inverse_relation_[companion].insert(account);

        character *ch = get_character_manager().find_pc(account.c_str());
        desc *d = ch ? ch->get_desc() : nullptr;

        if (d) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("<메신져> %s 님을 친구로 추가하였습니다."), companion.c_str());
        }

        character *tch = get_character_manager().find_pc(companion.c_str());

        if (tch)
            send_login(account, companion);
        else
            send_logout(account, companion);
    }

    void messenger_manager::add_to_list(messenger_manager::key_a account, messenger_manager::key_a companion) {
        if (companion.size() == 0)
            return;

        if (relation_[account].find(companion) != relation_[account].end())
            return;

        metin2::log::info("Messenger: {} added {} as a friend", account, companion);
        get_db_manager().query(sql::format("INSERT INTO messenger_list VALUES ({}, {})", account, companion));

        apply_add_to_list(account, companion);

        packet_gg_messenger p2ppck;

        p2ppck.header = HEADER_GG_MESSENGER_ADD;
        strlcpy(p2ppck.account, account.c_str(), sizeof(p2ppck.account));
        strlcpy(p2ppck.companion, companion.c_str(), sizeof(p2ppck.companion));
        get_p2p_manager().send(&p2ppck, sizeof(packet_gg_messenger));
    }

    void messenger_manager::apply_remove_from_list(messenger_manager::key_a account, messenger_manager::key_a companion) {
        relation_[account].erase(companion);
        inverse_relation_[companion].erase(account);

        character *ch = get_character_manager().find_pc(account.c_str());
        desc *d = ch ? ch->get_desc() : nullptr;

        if (d)
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("<메신져> %s 님을 메신저에서 삭제하였습니다."), companion.c_str());
    }

    auto messenger_manager::is_in_list(messenger_manager::key_a account, messenger_manager::key_a companion) -> bool {
        if (relation_.find(account) == relation_.end())
            return false;

        if (relation_[account].empty())
            return false;

        return relation_[account].find(companion) != relation_[account].end();
    }

    void messenger_manager::remove_from_list(messenger_manager::key_a account, messenger_manager::key_a companion) {
        if (companion.empty())
            return;

        if (companion.size() == 0)
            return;

        if (!is_in_list(account, companion))
            return;

        metin2::log::debug("Messenger: {} removed {} as a friend", account, companion);

        get_db_manager().query(sql::format("DELETE FROM messenger_list WHERE account={} AND companion = {}", account, companion));

        apply_remove_from_list(account, companion);

        packet_gg_messenger p2ppck;

        p2ppck.header = HEADER_GG_MESSENGER_REMOVE;
        strlcpy(p2ppck.account, account.c_str(), sizeof(p2ppck.account));
        strlcpy(p2ppck.companion, companion.c_str(), sizeof(p2ppck.companion));
        get_p2p_manager().send(&p2ppck, sizeof(packet_gg_messenger));
    }

    void messenger_manager::remove_all_list(key_a account) {
        std::set<key_t> company(relation_[account]);

        get_db_manager().query(sql::format("DELETE FROM messenger_list WHERE account={} OR companion={}", account, account));

        for (const key_t &companion : company)
            remove_from_list(account, companion);
    }

    void messenger_manager::send_list(messenger_manager::key_a account) {
        character *ch = get_character_manager().find_pc(account.c_str());

        if (!ch)
            return;

        desc *d = ch->get_desc();

        if (!d)
            return;

        if (relation_.find(account) == relation_.end())
            return;

        if (relation_[account].empty())
            return;

        packet_gc_messenger pack;

        pack.header = HEADER_GC_MESSENGER;
        pack.subheader = MESSENGER_SUBHEADER_GC_LIST;
        pack.size = sizeof(packet_gc_messenger);

        packet_gc_messenger_list_offline pack_offline;
        packet_gc_messenger_list_online pack_online;

        temp_buffer buf(128 * 1024);

        auto it = relation_[account].begin(), eit = relation_[account].end();

        while (it != eit) {
            if (set_login_account_.find(*it) != set_login_account_.end()) {
                pack_online.connected = 1;

                pack_online.length = it->size();

                buf.write(&pack_online, sizeof(packet_gc_messenger_list_online));
                buf.write(it->c_str(), it->size());
            } else {
                pack_offline.connected = 0;

                pack_offline.length = it->size();

                buf.write(&pack_offline, sizeof(packet_gc_messenger_list_offline));
                buf.write(it->c_str(), it->size());
            }

            ++it;
        }

        pack.size += buf.size();

        d->buffered_packet(&pack, sizeof(packet_gc_messenger));
        d->packet(buf.read_peek(), buf.size());
    }

    void messenger_manager::send_login(messenger_manager::key_a account, messenger_manager::key_a companion) {
        character *ch = get_character_manager().find_pc(account.c_str());
        desc *d = ch ? ch->get_desc() : nullptr;

        if (!d)
            return;

        if (!d->get_character())
            return;

        if (ch->gm_level() == GM_PLAYER && gm_get_level(companion.c_str()) != GM_PLAYER)
            return;

        uint8_t len = companion.size();

        packet_gc_messenger pack;

        pack.header = HEADER_GC_MESSENGER;
        pack.subheader = MESSENGER_SUBHEADER_GC_LOGIN;
        pack.size = sizeof(packet_gc_messenger) + sizeof(uint8_t) + len;

        d->buffered_packet(&pack, sizeof(packet_gc_messenger));
        d->buffered_packet(&len, sizeof(uint8_t));
        d->packet(companion.c_str(), companion.size());
    }

    void messenger_manager::send_logout(messenger_manager::key_a account, messenger_manager::key_a companion) {
        if (!companion.size())
            return;

        character *ch = get_character_manager().find_pc(account.c_str());
        desc *d = ch ? ch->get_desc() : nullptr;

        if (!d)
            return;

        uint8_t len = companion.size();

        packet_gc_messenger pack;

        pack.header = HEADER_GC_MESSENGER;
        pack.subheader = MESSENGER_SUBHEADER_GC_LOGOUT;
        pack.size = sizeof(packet_gc_messenger) + sizeof(uint8_t) + len;

        d->buffered_packet(&pack, sizeof(packet_gc_messenger));
        d->buffered_packet(&len, sizeof(uint8_t));
        d->packet(companion.c_str(), companion.size());
    }
}
