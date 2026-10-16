#include <metin2/config/config.h>
#include <metin2/game/admin/cmd.h>
#include <metin2/game/admin/gm.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/constants.h>
#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/characters/horsename_manager.h>
#include <metin2/game/characters/mob_manager.h>
#include <metin2/game/characters/motion.h>
#include <metin2/game/combat/skill.h>
#include <metin2/game/empire/castle.h>
#include <metin2/game/empire/monarch.h>
#include <metin2/game/empire/priv_manager.h>
#include <metin2/game/empire/start_position.h>
#include <metin2/game/guilds/building.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/guilds/guild_manager.h>
#include <metin2/game/items/cube.h>
#include <metin2/game/items/dragon_soul.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/items/refine.h>
#include <metin2/game/items/unique_item.h>
#include <metin2/game/parties/party.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/protocol/desc_manager.h>
#include <metin2/game/protocol/input.h>
#include <metin2/game/protocol/p2p.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/protocol/protocol.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/quest/questpc.h>
#include <metin2/game/social/marriage.h>
#include <metin2/game/social/wedding.h>
#include <metin2/game/text/banword.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/text/locale_service.h>
#include <metin2/game/trade/shop_manager.h>
#include <metin2/game/world/map_location.h>
#include <metin2/game/world/sectree_manager.h>

#include <metin2/common/startup.h>
#include <metin2/core/main.h>
#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/apply_types.h>
#include <metin2/proto/building.h>
#include <metin2/proto/character_types.h>
#include <metin2/proto/chat_types.h>
#include <metin2/proto/guild_types.h>
#include <metin2/proto/inventory.h>
#include <metin2/proto/limits.h>
#include <metin2/proto/tables.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <format>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>

namespace metin2::game::inline protocol {

    auto get_server_location(account_table &tab, uint8_t empire_value) -> bool {
        bool bFound = false;

        for (int i = 0; i < PLAYER_PER_ACCOUNT; ++i) {
            if (0 == tab.players[i].id)
                continue;

            bFound = true;
            int32_t lIndex = 0;

            if (!map_locations::instance().get(tab.players[i].x,
                    tab.players[i].y,
                    lIndex,
                    tab.players[i].addr,
                    tab.players[i].port)) {
                metin2::log::error("No server for location of player {} (map {:d}, {:d}x{:d}, empire {:d}), moving to empire start",
                    tab.players[i].name, lIndex, tab.players[i].x, tab.players[i].y, tab.empire);

                tab.players[i].x = empire_start_x(tab.empire);
                tab.players[i].y = empire_start_y(tab.empire);

                lIndex = 0;

                if (!map_locations::instance().get(tab.players[i].x, tab.players[i].y, lIndex, tab.players[i].addr, tab.players[i].port)) {
                    metin2::log::error("No server for empire start location (map {:d}, {:d}x{:d}) of player {}",
                        lIndex,
                        tab.players[i].x,
                        tab.players[i].y,
                        tab.players[i].name);

                    if (!proxy_ip.empty())
                        tab.players[i].addr = inet_addr(proxy_ip.c_str());

                    continue;
                }
            }

            struct in_addr in;

            if (!proxy_ip.empty())
                tab.players[i].addr = inet_addr(proxy_ip.c_str());

            in.s_addr = tab.players[i].addr;
            metin2::log::debug("Player server for character select: {}:{:d}", inet_ntoa(in), tab.players[i].port);
        }

        return bFound;
    }

    void input_db::login_success(uint32_t handle, const char *data) {
        account_table *pTab = (account_table *)data;

        desc *d = desc_manager::instance().find_by_handle(handle);

        if (!d) {
            metin2::log::warn("Login success for account {} but its connection is gone, logging out", pTab->login);

            logout_packet pack;

            strlcpy(pack.login, pTab->login, sizeof(pack.login));
            db_clientdesc->db_packet(HEADER_GD_LOGOUT, handle, &pack, sizeof(pack));
            return;
        }

        if (strcmp(pTab->status, "OK")) {
            metin2::log::info("Login refused: status {} for account {}", pTab->status, pTab->login);

            logout_packet pack;

            strlcpy(pack.login, pTab->login, sizeof(pack.login));
            db_clientdesc->db_packet(HEADER_GD_LOGOUT, handle, &pack, sizeof(pack));

            login_failure(d, pTab->status);
            return;
        }

        for (int i = 0; i != PLAYER_PER_ACCOUNT; ++i) {
            simple_player &player = pTab->players[i];
            metin2::log::debug("Account character {} (job {:d})", player.name, player.job);
        }

        bool bFound = get_server_location(*pTab, pTab->empire);

        d->bind_account_table(pTab);

        if (!bFound) {
            packet_gc_empire pe;
            pe.header = HEADER_GC_EMPIRE;
            pe.empire = number(1, 3);
            d->packet(&pe, sizeof(pe));
        } else {
            packet_gc_empire pe;
            pe.header = HEADER_GC_EMPIRE;
            pe.empire = d->empire();
            d->packet(&pe, sizeof(pe));
        }

        d->set_phase(PHASE_SELECT);
        d->send_login_success_packet();

        metin2::log::info("Login: account {}", pTab->login);
    }

    void input_db::player_create_failure(desc *d, uint8_t type_value) {
        if (!d)
            return;

        packet_gc_create_failure pack;

        pack.header = HEADER_GC_CHARACTER_CREATE_FAILURE;
        pack.type = type_value;

        d->packet(&pack, sizeof(pack));
    }

    void input_db::player_create_success(desc *d, const char *data) {
        if (!d)
            return;

        packet_dg_create_success *pPacketDB = (packet_dg_create_success *)data;

        if (pPacketDB->account_character_index >= PLAYER_PER_ACCOUNT) {
            d->packet(encode_byte(HEADER_GC_CHARACTER_CREATE_FAILURE), 1);
            return;
        }

        int32_t lIndex = 0;

        if (!game::world::map_locations::instance().get(pPacketDB->player.x,
                pPacketDB->player.y,
                lIndex,
                pPacketDB->player.addr,
                pPacketDB->player.port)) {
            metin2::log::error("No server for location of new player (map {:d}, {:d}x{:d}, name {})",
                lIndex,
                pPacketDB->player.x,
                pPacketDB->player.y,
                pPacketDB->player.name);
        }

        account_table &r_Tab = d->get_account_table();
        r_Tab.players[pPacketDB->account_character_index] = pPacketDB->player;

        packet_gc_player_create_success pack;

        pack.header = HEADER_GC_CHARACTER_CREATE_SUCCESS;
        pack.account_character_index = pPacketDB->account_character_index;
        pack.player = pPacketDB->player;

        if (!proxy_ip.empty())
            pack.player.addr = inet_addr(proxy_ip.c_str());

        d->packet(&pack, sizeof(packet_gc_player_create_success));

        log_manager::instance().char_log(pack.player.id, 0, 0, 0, "CREATE PLAYER", "", d->host_name());
    }

    void input_db::player_delete_success(desc *d, const char *data) {
        if (!d)
            return;

        uint8_t account_index;
        account_index = decode_byte(data);
        d->buffered_packet(encode_byte(HEADER_GC_CHARACTER_DELETE_SUCCESS), 1);
        d->packet(encode_byte(account_index), 1);

        d->get_account_table().players[account_index].id = 0;
    }

    void input_db::player_delete_fail(desc *d) {
        if (!d)
            return;

        d->packet(encode_byte(HEADER_GC_CHARACTER_DELETE_WRONG_SOCIAL_ID), 1);
    }

    void input_db::change_name(desc *d, const char *data) {
        if (!d)
            return;

        packet_dg_change_name *p = (packet_dg_change_name *)data;

        account_table &r = d->get_account_table();

        if (!r.id)
            return;

        for (size_t i = 0; i < PLAYER_PER_ACCOUNT; ++i)
            if (r.players[i].id == p->pid) {
                strlcpy(r.players[i].name, p->name, sizeof(r.players[i].name));
                r.players[i].change_name = 0;

                packet_gc_change_name pgc;

                pgc.header = HEADER_GC_CHANGE_NAME;
                pgc.pid = p->pid;
                strlcpy(pgc.name, p->name, sizeof(pgc.name));

                d->packet(&pgc, sizeof(packet_gc_change_name));
                break;
            }
    }

    void input_db::player_load(desc *d, const char *data) {
        player_table *pTab = (player_table *)data;

        if (!d)
            return;

        int32_t lMapIndex = pTab->map_index;
        pixel_position pos;

        if (lMapIndex == 0) {
            lMapIndex = sectree_manager::instance().map_index(pTab->x, pTab->y);

            if (lMapIndex == 0) {
                lMapIndex = empire_start_map(d->get_account_table().empire);
                pos.x = empire_start_x(d->get_account_table().empire);
                pos.y = empire_start_y(d->get_account_table().empire);
            } else {
                pos.x = pTab->x;
                pos.y = pTab->y;
            }
        }
        pTab->map_index = lMapIndex;

        if (!sectree_manager::instance().valid_location(pTab->map_index, pTab->x, pTab->y, lMapIndex, pos, d->empire())) {
            metin2::log::error("No valid location {:d}x{:d} for player {}, closing connection", pTab->x, pTab->y, pTab->name);
            d->set_phase(PHASE_CLOSE);
            return;
        }

        pTab->x = pos.x;
        pTab->y = pos.y;
        pTab->map_index = lMapIndex;

        if (d->get_character() || d->is_phase(PHASE_GAME)) {
            character *p = d->get_character();
            metin2::log::error("Player load on a connection that is already in game (player {}, {})", p->name(), static_cast<const void *>(p));
            return;
        }

        if (nullptr != character_manager::instance().find_pc(pTab->name)) {
            metin2::log::error("Player {} is already in game, load ignored", pTab->name);
            return;
        }

        character *ch = character_manager::instance().create_character(pTab->name, pTab->id);

        ch->bind_desc(d);
        ch->set_player_proto(pTab);
        ch->set_empire(d->empire());

        d->bind_character(ch);

        {
            packet_gg_login p;

            p.header = HEADER_GG_LOGIN;
            strlcpy(p.name, ch->name(), sizeof(p.name));
            p.pid = ch->player_id();
            p.empire = ch->empire();
            p.map_index = sectree_manager::instance().map_index(ch->x(), ch->y());
            p.channel = metin2::config::server().channel;

            p2p_manager::instance().send(&p, sizeof(packet_gg_login));

            const std::string hint = std::format("{} {} {} {} {}",
                ch->get_desc()->host_name(), ch->gold(), metin2::config::server().channel, ch->map_index(), ch->alignment());
            log_manager::instance().char_log(ch, 0, "LOGIN", hint.c_str());
        }

        d->set_phase(PHASE_LOADING);
        ch->main_character_packet();

        int32_t lPublicMapIndex = lMapIndex >= 10000 ? lMapIndex / 10000 : lMapIndex;

        if (!map_allow_find(lPublicMapIndex)) {
            metin2::log::error("Map {:d} is not hosted here (player {}, empire {:d}), warping to empire start",
                lMapIndex, pTab->name, d->empire());

            ch->set_warp_location(empire_start_map(d->empire()),
                empire_start_x(d->empire()) / 100,
                empire_start_y(d->empire()) / 100);

            d->set_phase(PHASE_CLOSE);
            return;
        }

        quest::quest_manager::instance().broadcast_event_flag_on_login(ch);

        for (int i = 0; i < QUICKSLOT_MAX_NUM; ++i)
            ch->set_quickslot(i, pTab->quickslot[i]);

        ch->points_packet();
        ch->skill_level_packet();

        metin2::log::info("Player loaded: {} at {:d}x{:d}x{:d}, level {:d}, move speed {:d}, job {:d}, attack grade {:d}, defence grade {:d}, gm level {:d}",
            pTab->name,
            ch->x(), ch->y(), ch->z(),
            ch->level(),
            ch->point(POINT_MOV_SPEED),
            ch->job(),
            ch->point(POINT_ATT_GRADE),
            ch->point(POINT_DEF_GRADE),
            ch->gm_level());

        ch->query_safebox_size();
    }

    namespace {

        template <typename T>
        auto read_table(const char *&data, const char *what) -> std::optional<std::span<T>> {
            if (decode_2bytes(data) != sizeof(T)) {
                metin2::log::error("Boot packet {} record size mismatch, shutting down", what);
                thecore_shutdown();
                return std::nullopt;
            }
            data += 2;

            const uint16_t count = decode_2bytes(data);
            data += 2;

            std::span<T> records(reinterpret_cast<T *>(const_cast<char *>(data)), count);
            data += count * sizeof(T);
            return records;
        }

    }

    void input_db::boot(const char *data) {
        signal_timer_disable();

        uint32_t dwPacketSize = decode_4bytes(data);
        data += 4;

        uint8_t bVersion = decode_byte(data);
        data += 1;

        metin2::log::debug("Boot: packet size {:d}", dwPacketSize);
        metin2::log::debug("Boot: version {:d}", bVersion);
        if (bVersion != 6) {
            metin2::log::error("Unsupported boot packet version {:d}, shutting down", bVersion);
            thecore_shutdown();
        }

        metin2::log::debug("Boot: mob_table size {:d}", sizeof(mob_table));
        metin2::log::debug("Boot: item_table size {:d}", sizeof(item_table));
        metin2::log::debug("Boot: shop_table size {:d}", sizeof(shop_table));
        metin2::log::debug("Boot: skill_table size {:d}", sizeof(skill_table));
        metin2::log::debug("Boot: refine_table size {:d}", sizeof(refine_table));
        metin2::log::debug("Boot: item_attr_table size {:d}", sizeof(item_attr_table));
        metin2::log::debug("Boot: item rare table size {:d}", sizeof(item_attr_table));
        metin2::log::debug("Boot: banword_table size {:d}", sizeof(banword_table));
        metin2::log::debug("Boot: land_table size {:d}", sizeof(building::land_table));
        metin2::log::debug("Boot: object_proto size {:d}", sizeof(building::object_proto));
        metin2::log::debug("Boot: object_table size {:d}", sizeof(building::object_table));
        metin2::log::debug("Boot: t_admin_info size {:d}", sizeof(t_admin_info));

        auto mobs = read_table<mob_table>(data, "mob table");
        if (!mobs)
            return;
        metin2::log::info("Boot: {:d} mobs", mobs->size());
        if (!mobs->empty())
            mob_manager::instance().initialize(mobs->data(), mobs->size());

        auto items = read_table<item_table>(data, "item table");
        if (!items)
            return;
        metin2::log::info("Boot: {:d} items", items->size());
        if (!items->empty())
            item_manager::instance().initialize(items->data(), items->size());

        auto shops = read_table<shop_table>(data, "shop table");
        if (!shops)
            return;
        metin2::log::info("Boot: {:d} shops", shops->size());
        if (!shops->empty() && !shop_manager::instance().initialize(shops->data(), shops->size())) {
            metin2::log::error("Cannot initialize shops, shutting down");
            thecore_shutdown();
            return;
        }

        auto skills = read_table<skill_table>(data, "skill table");
        if (!skills)
            return;
        metin2::log::info("Boot: {:d} skills", skills->size());
        if (!skills->empty() && !skill_manager::instance().initialize(skills->data(), skills->size())) {
            metin2::log::error("Cannot initialize skills, shutting down");
            thecore_shutdown();
            return;
        }

        auto refines = read_table<refine_table>(data, "refine table");
        if (!refines)
            return;
        metin2::log::info("Boot: {:d} refine recipes", refines->size());
        if (!refines->empty())
            refine_manager::instance().initialize(refines->data(), refines->size());

        auto attrs = read_table<item_attr_table>(data, "item attribute table");
        if (!attrs)
            return;
        metin2::log::info("Boot: {:d} item attributes", attrs->size());
        for (const item_attr_table &attr : *attrs) {
            if (attr.apply_index >= MAX_APPLY_NUM)
                continue;

            map_item_attr[attr.apply_index] = attr;
            metin2::log::debug("Item attribute {:d}: {} (probability {:d})", attr.apply_index, attr.apply, attr.prob);
        }

        auto rare_attrs = read_table<item_attr_table>(data, "item rare attribute table");
        if (!rare_attrs)
            return;
        metin2::log::info("Boot: {:d} item rare attributes", rare_attrs->size());
        for (const item_attr_table &attr : *rare_attrs) {
            if (attr.apply_index >= MAX_APPLY_NUM)
                continue;

            map_item_rare[attr.apply_index] = attr;
            metin2::log::debug("Item rare attribute {:d}: {} (probability {:d})", attr.apply_index, attr.apply, attr.prob);
        }

        auto banwords = read_table<banword_table>(data, "banword table");
        if (!banwords)
            return;
        banword_manager::instance().initialize(banwords->data(), banwords->size());

        {
            using namespace building;

            auto lands = read_table<land_table>(data, "land table");
            if (!lands)
                return;
            for (land_table &land : *lands)
                manager::instance().load_land(&land);

            auto object_protos = read_table<object_proto>(data, "object proto table");
            if (!object_protos)
                return;
            manager::instance().load_object_proto(object_protos->data(), object_protos->size());

            auto objects = read_table<object_table>(data, "object table");
            if (!objects)
                return;
            for (object_table &object : *objects)
                manager::instance().load_object(&object, true);
        }
        int32_t server_time;
        memcpy(&server_time, data, sizeof(server_time));
        set_global_time(server_time);
        data += sizeof(int32_t);

        auto ranges = read_table<item_id_range_table>(data, "item id range");
        if (!ranges)
            return;
        item_id_range_table *range = ranges->data();

        item_id_range_table *rangespare = (item_id_range_table *)data;
        data += ranges->size() * sizeof(item_id_range_table);

        int ChunkSize = decode_2bytes(data);
        data += 2;
        int HostSize = decode_2bytes(data);
        data += 2;
        metin2::log::debug("Boot: {:d} gm hosts ({:d} bytes each)", HostSize, ChunkSize);
        for (int n = 0; n < HostSize; ++n) {
            metin2::log::debug("Boot: gm host {}", data);
            data += ChunkSize;
        }

        data += 2;
        int adminsize = decode_2bytes(data);
        data += 2;

        for (int n = 0; n < adminsize; ++n) {
            t_admin_info &rAdminInfo = *(t_admin_info *)data;

            gm_new_insert(rAdminInfo);

            data += sizeof(rAdminInfo);
        }

        data += 2;
        data += 2;

        monarch_info &p = *(monarch_info *)data;
        data += sizeof(monarch_info);

        monarch::instance().set_monarch_info(&p);

        for (int n = 1; n < 4; ++n) {
            if (*p.name[n])
                metin2::log::info("Monarch of empire {:d}: pid {:d}, money {:d}, name {}", n, p.pid[n], p.money[n], p.name[n]);
        }

        int CandidacySize = decode_2bytes(data);
        data += 2;

        int CandidacyCount = decode_2bytes(data);
        data += 2;

        if (metin2::config::test_server())
            metin2::log::info("Monarch candidacy: record size {:d}, count {:d}", CandidacySize, CandidacyCount);

        data += CandidacySize * CandidacyCount;

        uint16_t endCheck = decode_2bytes(data);
        if (endCheck != 0xffff) {
            metin2::log::error("Boot packet end marker is {:#x} instead of 0xffff, shutting down", endCheck);
            thecore_shutdown();
            return;
        } else
            metin2::log::debug("Boot packet end marker ok ({:#x})", endCheck);
        data += 2;

        if (!item_manager::instance().set_max_item_id(*range)) {
            metin2::log::error("Item id range exhausted, shutting down");
            thecore_shutdown();
            return;
        }

        if (!item_manager::instance().set_max_spare_item_id(*rangespare)) {
            metin2::log::error("Spare item id range exhausted, shutting down");
            thecore_shutdown();
            return;
        }

        const std::string szCommonDropItemFileName = std::format("{}/common_drop_item.txt", locale_service_get_base_path());
        const std::string szETCDropItemFileName = std::format("{}/etc_drop_item.txt", locale_service_get_base_path());
        const std::string szMOBDropItemFileName = std::format("{}/mob_drop_item.txt", locale_service_get_base_path());
        const std::string szSpecialItemGroupFileName = std::format("{}/special_item_group.txt", locale_service_get_base_path());
        const std::string szDropItemGroupFileName = std::format("{}/drop_item_group.txt", locale_service_get_base_path());
        const std::string szMapIndexFileName = std::format("{}/index", locale_service_get_map_path());
        const std::string szItemVnumMaskTableFileName = std::format("{}/ori_to_new_table.txt", locale_service_get_base_path());
        const std::string szDragonSoulTableFileName = std::format("{}/dragon_soul_table.txt", locale_service_get_base_path());

        metin2::log::info("Loading cube recipes");
        if (!cube_information_initialize()) {
            metin2::log::error("Cannot load cube recipes, shutting down");
            thecore_shutdown();
            return;
        }

        metin2::log::info("Loading common drop items from {}", szCommonDropItemFileName);
        if (!item_manager::instance().read_common_drop_item_file(szCommonDropItemFileName.c_str())) {
            metin2::log::error("Cannot load common drop items from {}, shutting down", szCommonDropItemFileName);
            thecore_shutdown();
            return;
        }

        metin2::log::info("Loading etc drop items from {}", szETCDropItemFileName);
        if (!item_manager::instance().read_etc_drop_item_file(szETCDropItemFileName.c_str())) {
            metin2::log::error("Cannot load etc drop items from {}, shutting down", szETCDropItemFileName);
            thecore_shutdown();
            return;
        }

        metin2::log::info("Loading drop item groups from {}", szDropItemGroupFileName);
        if (!item_manager::instance().read_drop_item_group(szDropItemGroupFileName.c_str())) {
            metin2::log::error("Cannot load drop item groups from {}, shutting down", szDropItemGroupFileName);
            thecore_shutdown();
            return;
        }

        metin2::log::info("Loading special item groups from {}", szSpecialItemGroupFileName);
        if (!item_manager::instance().read_special_drop_item_file(szSpecialItemGroupFileName.c_str())) {
            metin2::log::error("Cannot load special item groups from {}, shutting down", szSpecialItemGroupFileName);
            thecore_shutdown();
            return;
        }

        metin2::log::info("Loading item vnum mask table from {}", szItemVnumMaskTableFileName);
        if (!item_manager::instance().read_item_vnum_mask_table(szItemVnumMaskTableFileName.c_str())) {
            metin2::log::warn("Cannot load item vnum mask table {}, continuing without it", szItemVnumMaskTableFileName);
        }

        metin2::log::info("Loading mob drop items from {}", szMOBDropItemFileName);
        if (!item_manager::instance().read_monster_drop_item_group(szMOBDropItemFileName.c_str())) {
            metin2::log::error("Cannot load mob drop items from {}, shutting down", szMOBDropItemFileName);
            thecore_shutdown();
            return;
        }

        metin2::log::info("Loading map index from {}", szMapIndexFileName);
        if (!sectree_manager::instance().build(szMapIndexFileName.c_str(), locale_service_get_map_path().c_str())) {
            metin2::log::error("Cannot load map index from {}, shutting down", szMapIndexFileName);
            thecore_shutdown();
            return;
        }

        metin2::log::info("Loading dragon soul table from {}", szDragonSoulTableFileName);
        if (!ds_manager::instance().read_dragon_soul_table_file(szDragonSoulTableFileName.c_str())) {
            metin2::log::error("Cannot load dragon soul table from {}", szDragonSoulTableFileName);
        }

        building::manager::instance().finalize_boot();

        motion_manager::instance().build();

        signal_timer_enable(30);

        if (metin2::config::test_server()) {
            mob_manager::instance().dump_regen_count("mob_count");
        }

        castle_boot();

        if (!mark_server_ready()) {
            thecore_shutdown();
            return;
        }
        metin2::log::info("Boot complete");
    }

    EVENTINFO(quest_login_event_info) {
        uint32_t pid;

        quest_login_event_info()
            : pid(0) {
        }
    };

    EVENTFUNC(quest_login_event) {
        quest_login_event_info *info = dynamic_cast<quest_login_event_info *>(event->info.get());

        if (info == nullptr) {
            metin2::log::error("Quest login event has no info");
            return 0;
        }

        uint32_t dwPID = info->pid;

        character *ch = character_manager::instance().find_by_pid(dwPID);

        if (!ch)
            return 0;

        desc *d = ch->get_desc();

        if (!d)
            return 0;

        if (d->is_phase(PHASE_HANDSHAKE) ||
            d->is_phase(PHASE_LOGIN) ||
            d->is_phase(PHASE_SELECT) ||
            d->is_phase(PHASE_DEAD) ||
            d->is_phase(PHASE_LOADING)) {
            return passes_per_sec(1);
        } else if (d->is_phase(PHASE_CLOSE)) {
            return 0;
        } else if (d->is_phase(PHASE_GAME)) {
            metin2::log::debug("Quest: login of pid {:d} (deferred)", ch->player_id());
            quest::quest_manager::instance().login(ch->player_id());
            return 0;
        } else {
            metin2::log::error("Quest login for pid {:d} in unexpected connection phase", ch->player_id());
            return 0;
        }
    }

    void input_db::quest_load(desc *d, const char *data) {
        if (nullptr == d)
            return;

        character *ch = d->get_character();

        if (nullptr == ch)
            return;

        const uint32_t dwCount = decode_4bytes(data);

        const quest_table *pQuestTable = reinterpret_cast<const quest_table *>(data + 4);

        if (nullptr != pQuestTable) {
            if (dwCount != 0) {
                if (ch->player_id() != pQuestTable[0].pid) {
                    metin2::log::error("Quest data pid mismatch: player pid {:d}, quest pid {:d}", ch->player_id(), pQuestTable[0].pid);
                    return;
                }
            }

            metin2::log::debug("Quest: loading {:d} flags", dwCount);

            quest::pc *pkPC = quest::quest_manager::instance().pc_force(ch->player_id());

            if (!pkPC) {
                metin2::log::error("Cannot create quest state for pid {:d}", pQuestTable[0].pid);
                return;
            }

            if (pkPC->is_loaded())
                return;

            for (unsigned int i = 0; i < dwCount; ++i) {
                std::string st(pQuestTable[i].name);

                st += ".";
                st += pQuestTable[i].state;

                metin2::log::debug("Quest flag {} = {:d}", st, pQuestTable[i].value);
                pkPC->set_flag(st.c_str(), pQuestTable[i].value, false);
            }

            pkPC->set_loaded();
            pkPC->build();

            if (ch->get_desc()->is_phase(PHASE_GAME)) {
                metin2::log::debug("Quest: login of pid {:d}", pQuestTable[0].pid);
                quest::quest_manager::instance().login(pQuestTable[0].pid);
            } else {
                quest_login_event_info *info = alloc_event_info<quest_login_event_info>();
                info->pid = ch->player_id();

                event_create(quest_login_event, info, passes_per_sec(1));
            }
        }
    }

    void input_db::safebox_load(desc *d, const char *data) {
        if (!d)
            return;

        safebox_table *p = (safebox_table *)data;

        if (d->get_account_table().id != p->id) {
            metin2::log::error("Safebox account id mismatch: account {:d}, safebox {:d}", d->get_account_table().id, p->id);
            return;
        }

        if (!d->get_character())
            return;

        uint8_t bSize = 1;

        character *ch = d->get_character();

        if (ch->shop_owner() || ch->get_exchange() || ch->my_shop() || ch->is_cube_open()) {
            d->get_character()->chat_packet(CHAT_TYPE_INFO, lc_text("다른거래창이 열린상태에서는 창고를 열수가 없습니다."));
            d->get_character()->cancel_safebox_load();
            return;
        }

        if (d->get_character()->premium_remain_seconds(PREMIUM_SAFEBOX) > 0 ||
            d->get_character()->is_equip_unique_group(UNIQUE_GROUP_LARGE_SAFEBOX))
            bSize = 3;

        d->get_character()->load_safebox(bSize * SAFEBOX_PAGE_SIZE, p->gold, p->item_count, (player_item *)(data + sizeof(safebox_table)));
    }

    void input_db::safebox_change_size(desc *d, const char *data) {
        if (!d)
            return;

        uint8_t bSize = *(uint8_t *)data;

        if (!d->get_character())
            return;

        d->get_character()->change_safebox_size(bSize);
    }

    void input_db::safebox_wrong_password(desc *d) {
        if (!d)
            return;

        if (!d->get_character())
            return;

        packet_cg_safebox_wrong_password p;
        p.header = HEADER_GC_SAFEBOX_WRONG_PASSWORD;
        d->packet(&p, sizeof(p));

        d->get_character()->cancel_safebox_load();
    }

    void input_db::safebox_change_password_answer(desc *d, const char *data) {
        if (!d)
            return;

        if (!d->get_character())
            return;

        safebox_change_password_packet_answer *p = (safebox_change_password_packet_answer *)data;
        if (p->flag) {
            d->get_character()->chat_packet(CHAT_TYPE_INFO, lc_text("<창고> 창고 비밀번호가 변경되었습니다."));
        } else {
            d->get_character()->chat_packet(CHAT_TYPE_INFO, lc_text("<창고> 기존 비밀번호가 틀렸습니다."));
        }
    }

    void input_db::mall_load(desc *d, const char *data) {
        if (!d)
            return;

        safebox_table *p = (safebox_table *)data;

        if (d->get_account_table().id != p->id) {
            metin2::log::error("Mall account id mismatch: account {:d}, mall {:d}", d->get_account_table().id, p->id);
            return;
        }

        if (!d->get_character())
            return;

        d->get_character()->load_mall(p->item_count, (player_item *)(data + sizeof(safebox_table)));
    }

    void input_db::login_already(desc *d, const char *data) {
        if (!d)
            return;

        {
            packet_dg_login_already *p = (packet_dg_login_already *)data;

            desc *d2 = desc_manager::instance().find_by_login_name(p->login);

            if (d2)
                d2->disconnect_of_same_login();
            else {
                packet_gg_disconnect pgg;

                pgg.header = HEADER_GG_DISCONNECT;
                strlcpy(pgg.login, p->login, sizeof(pgg.login));

                p2p_manager::instance().send(&pgg, sizeof(packet_gg_disconnect));
            }
        }

        login_failure(d, "ALREADY");
    }

    void input_db::empire_select(desc *d, const char *data) {
        metin2::log::debug("Empire select on connection {}", static_cast<const void *>(d));

        if (!d)
            return;

        account_table &rTable = d->get_account_table();
        rTable.empire = *(uint8_t *)data;

        packet_gc_empire pe;
        pe.header = HEADER_GC_EMPIRE;
        pe.empire = rTable.empire;
        d->packet(&pe, sizeof(pe));

        for (int i = 0; i < PLAYER_PER_ACCOUNT; ++i)
            if (rTable.players[i].id) {
                rTable.players[i].x = empire_start_x(rTable.empire);
                rTable.players[i].y = empire_start_y(rTable.empire);
            }

        get_server_location(d->get_account_table(), rTable.empire);

        d->send_login_success_packet();
    }

    void input_db::map_locations(const char *data) {
        uint8_t bCount = *(uint8_t *)(data++);

        metin2::log::debug("Received {:d} map locations", bCount);

        map_location *pLoc = (map_location *)data;

        while (bCount--) {
            for (int i = 0; i < MAX_MAPS_PER_SERVER; ++i) {
                if (0 == pLoc->maps[i])
                    break;

                game::world::map_locations::instance().insert(pLoc->maps[i], pLoc->host, pLoc->port);
            }

            pLoc++;
        }
    }

    void input_db::p2p(const char *data) {

        packet_dg_p2p *p = (packet_dg_p2p *)data;

        p2p_manager &mgr = p2p_manager::instance();

        if (false == desc_manager::instance().is_p2p_desc_exist(p->host, p->port)) {
            client_desc *pkDesc = nullptr;
            metin2::log::info("Connecting to peer {}:{:d}", p->host, p->port);
            pkDesc = desc_manager::instance().create_connection_desc(p->host, p->port, PHASE_P2P, false);
            mgr.register_connector(pkDesc);
            pkDesc->set_p2p(p->host, p->port);
        }
    }

    void input_db::guild_load(const char *data) {
        guild_manager::instance().load_guild(*(uint32_t *)data);
    }

    void input_db::guild_skill_update(const char *data) {
        packet_guild_skill_update *p = (packet_guild_skill_update *)data;

        guild *g = guild_manager::instance().touch_guild(p->guild_id);

        if (g) {
            g->update_skill(p->skill_point, p->skill_levels);
            g->guild_point_change(POINT_SP, p->amount, p->save ? true : false);
        }
    }

    void input_db::guild_war(const char *data) {
        packet_guild_war *p = (packet_guild_war *)data;

        metin2::log::info("Guild war: guild {:d} vs {:d}, state {:d}", p->guild_from, p->guild_to, p->war);

        switch (p->war) {
        case GUILD_WAR_SEND_DECLARE:
        case GUILD_WAR_RECV_DECLARE:
            guild_manager::instance().declare_war(p->guild_from, p->guild_to, p->type);
            break;

        case GUILD_WAR_REFUSE:
            guild_manager::instance().refuse_war(p->guild_from, p->guild_to);
            break;

        case GUILD_WAR_WAIT_START:
            guild_manager::instance().wait_start_war(p->guild_from, p->guild_to);
            break;

        case GUILD_WAR_CANCEL:
            guild_manager::instance().cancel_war(p->guild_from, p->guild_to);
            break;

        case GUILD_WAR_ON_WAR:
            guild_manager::instance().start_war(p->guild_from, p->guild_to);
            break;

        case GUILD_WAR_END:
            guild_manager::instance().end_war(p->guild_from, p->guild_to);
            break;

        case GUILD_WAR_OVER:
            guild_manager::instance().war_over(p->guild_from, p->guild_to, p->type);
            break;

        case GUILD_WAR_RESERVE:
            guild_manager::instance().reserve_war(p->guild_from, p->guild_to, p->type);
            break;

        default:
            metin2::log::error("Unknown guild war state {:d} (guild {:d} vs {:d})", p->war, p->guild_from, p->guild_to);
            break;
        }
    }

    void input_db::guild_war_score(const char *data) {
        packet_guild_war_score *p = (packet_guild_war_score *)data;
        guild *g = guild_manager::instance().touch_guild(p->guild_gain_point);
        g->set_war_score_against_to(p->guild_opponent, p->score);
    }

    void input_db::guild_skill_recharge() {
        guild_manager::instance().skill_recharge();
    }

    void input_db::guild_exp_update(const char *data) {
        packet_guild_skill_update *p = (packet_guild_skill_update *)data;
        metin2::log::debug("Guild {:d}: exp update {:d}", p->guild_id, p->amount);

        guild *g = guild_manager::instance().touch_guild(p->guild_id);

        if (g)
            g->guild_point_change(POINT_EXP, p->amount);
    }

    void input_db::guild_add_member(const char *data) {
        packet_dg_guild_member *p = (packet_dg_guild_member *)data;
        guild *g = guild_manager::instance().touch_guild(p->guild);

        if (g)
            g->add_member(p);
    }

    void input_db::guild_remove_member(const char *data) {
        packet_guild *p = (packet_guild *)data;
        guild *g = guild_manager::instance().touch_guild(p->guild);

        if (g)
            g->remove_member(p->info);
    }

    void input_db::guild_change_grade(const char *data) {
        packet_guild *p = (packet_guild *)data;
        guild *g = guild_manager::instance().touch_guild(p->guild);

        if (g)
            g->p2p_change_grade((uint8_t)p->info);
    }

    void input_db::guild_change_member_data(const char *data) {
        packet_guild_change_member_data *p = (packet_guild_change_member_data *)data;
        guild *g = guild_manager::instance().touch_guild(p->guild_id);

        if (g)
            g->change_member_data(p->pid, p->offer, p->level, p->grade);
    }

    void input_db::guild_disband(const char *data) {
        packet_guild *p = (packet_guild *)data;
        guild_manager::instance().disband_guild(p->guild);
    }

    void input_db::guild_ladder(const char *data) {
        packet_guild_ladder *p = (packet_guild_ladder *)data;
        metin2::log::debug("Guild {:d}: ladder points {:d}, wins {:d}, draws {:d}, losses {:d}", p->guild, p->ladder_point, p->win, p->draw, p->loss);
        guild *g = guild_manager::instance().touch_guild(p->guild);

        g->set_ladder_point(p->ladder_point);
        g->set_war_data(p->win, p->draw, p->loss);
    }

    void input_db::item_load(desc *d, const char *data) {
        character *ch;

        if (!d || !(ch = d->get_character()))
            return;

        if (ch->is_item_loaded())
            return;

        uint32_t dwCount = decode_4bytes(data);
        data += sizeof(uint32_t);

        metin2::log::debug("Loading items of player {}: {:d}", ch->name(), dwCount);

        std::vector<item *> v;

        player_item *p = (player_item *)data;

        for (uint32_t i = 0; i < dwCount; ++i, ++p) {
            item *item = item_manager::instance().create_item(p->vnum, p->count, p->id);

            if (!item) {
                metin2::log::error("Cannot create item vnum {:d} for player {} (item id {:d})", p->vnum, ch->name(), p->id);
                continue;
            }

            item->set_skip_save(true);
            item->set_sockets(p->sockets);
            item->set_attributes(p->attr);

            if ((p->window == INVENTORY && ch->inventory_item(p->pos)) ||
                (p->window == EQUIPMENT && ch->wear(p->pos))) {
                metin2::log::debug("Player {}: item {} has no free slot, restoring later", ch->name(), item->name());
                v.push_back(item);
            } else {
                switch (p->window) {
                case INVENTORY:
                case DRAGON_SOUL_INVENTORY:
                    item->add_to_character(ch, item_pos(p->window, p->pos));
                    break;

                case EQUIPMENT:
                    if (item->check_item_use_level(ch->level()) == true) {
                        if (item->equip_to(ch, p->pos) == false) {
                            v.push_back(item);
                        }
                    } else {
                        v.push_back(item);
                    }
                    break;
                }
            }

            if (false == item->on_after_created_item())
                metin2::log::error("Item post-create setup failed (vnum {:d}, id {:d})", item->vnum(), item->id());

            item->set_skip_save(false);
        }

        auto it = v.begin();

        while (it != v.end()) {
            game::items::item *item = *(it++);

            int pos = ch->empty_inventory(item->size());

            if (pos < 0) {
                pixel_position coord;
                coord.x = ch->x();
                coord.y = ch->y();

                item->add_to_ground(ch->map_index(), coord);
                item->set_ownership(ch, 180);
                item->start_destroy_event();
            } else
                item->add_to_character(ch, item_pos(INVENTORY, pos));
        }

        ch->check_maximum_points();
        ch->points_packet();

        ch->set_item_loaded();
    }

    void input_db::affect_load(desc *d, const char *data) {
        if (!d)
            return;

        if (!d->get_character())
            return;

        character *ch = d->get_character();

        uint32_t dwPID = decode_4bytes(data);
        data += sizeof(uint32_t);

        uint32_t dwCount = decode_4bytes(data);
        data += sizeof(uint32_t);

        if (ch->player_id() != dwPID)
            return;

        ch->load_affect(dwCount, (packet_affect_element *)data);
    }

    void input_db::party_create(const char *data) {
        packet_party_create *p = (packet_party_create *)data;
        party_manager::instance().p2p_create_party(p->leader_pid);
    }

    void input_db::party_delete(const char *data) {
        packet_party_delete *p = (packet_party_delete *)data;
        party_manager::instance().p2p_delete_party(p->leader_pid);
    }

    void input_db::party_add(const char *data) {
        packet_party_add *p = (packet_party_add *)data;
        party_manager::instance().p2p_join_party(p->leader_pid, p->pid, p->state);
    }

    void input_db::party_remove(const char *data) {
        packet_party_remove *p = (packet_party_remove *)data;
        party_manager::instance().p2p_quit_party(p->pid);
    }

    void input_db::party_state_change(const char *data) {
        packet_party_state_change *p = (packet_party_state_change *)data;
        party *pParty = party_manager::instance().p2p_create_party(p->leader_pid);

        if (!pParty)
            return;

        pParty->set_role(p->pid, p->role, p->flag);
    }

    void input_db::party_set_member_level(const char *data) {
        packet_party_set_member_level *p = (packet_party_set_member_level *)data;
        party *pParty = party_manager::instance().p2p_create_party(p->leader_pid);

        if (!pParty)
            return;

        pParty->p2p_set_member_level(p->pid, p->level);
    }

    void input_db::time(const char *data) {
        int32_t server_time;
        memcpy(&server_time, data, sizeof(server_time));
        set_global_time(server_time);
    }

    void input_db::reload_proto(const char *data) {
        uint16_t wSize;

        wSize = decode_2bytes(data);
        data += sizeof(uint16_t);
        if (wSize) skill_manager::instance().initialize((skill_table *)data, wSize);
        data += sizeof(skill_table) * wSize;

        wSize = decode_2bytes(data);
        data += sizeof(uint16_t);
        banword_manager::instance().initialize((banword_table *)data, wSize);
        data += sizeof(banword_table) * wSize;

        wSize = decode_2bytes(data);
        data += 2;
        metin2::log::info("Reload: {:d} items", wSize);

        if (wSize) {
            item_manager::instance().initialize((item_table *)data, wSize);
            data += wSize * sizeof(item_table);
        }

        wSize = decode_2bytes(data);
        data += 2;
        metin2::log::info("Reload: {:d} mobs", wSize);

        if (wSize) {
            mob_manager::instance().initialize((mob_table *)data, wSize);
            data += wSize * sizeof(mob_table);
        }

        motion_manager::instance().build();

        character_manager::instance().for_each_pc([](character *ch) { ch->compute_points(); });
    }

    void input_db::guild_skill_usable_change(const char *data) {
        packet_guild_skill_usable_change *p = (packet_guild_skill_usable_change *)data;

        guild *g = guild_manager::instance().touch_guild(p->guild);

        g->skill_usable_change(p->skill_vnum, p->usable ? true : false);
    }

    void input_db::auth_login(desc *d, const char *data) {
        if (!d)
            return;

        uint8_t bResult = *(uint8_t *)data;

        packet_gc_auth_success ptoc;

        ptoc.header = HEADER_GC_AUTH_SUCCESS;

        if (bResult) {
            ptoc.login_key = d->get_login_key();
        } else {
            ptoc.login_key = 0;
        }

        ptoc.result = bResult;

        d->packet(&ptoc, sizeof(packet_gc_auth_success));
        metin2::log::debug("Auth login result {:d}, login key {:d}", bResult, d->get_login_key());
    }
    void input_db::change_empire_priv(const char *data) {
        packet_dg_change_empire_priv *p = (packet_dg_change_empire_priv *)data;

        priv_manager::instance().give_empire_priv(p->empire, p->type, p->value, p->log, p->end_time_sec);
    }

    void input_db::change_guild_priv(const char *data) {
        packet_dg_change_guild_priv *p = (packet_dg_change_guild_priv *)data;

        priv_manager::instance().give_guild_priv(p->guild_id, p->type, p->value, p->log, p->end_time_sec);
    }

    void input_db::change_character_priv(const char *data) {
        packet_dg_change_character_priv *p = (packet_dg_change_character_priv *)data;
        priv_manager::instance().give_character_priv(p->pid, p->type, p->value, p->log);
    }

    void input_db::money_log(const char *data) {
        packet_money_log *p = (packet_money_log *)data;

        if (p->type == 4)
            return;

        if (metin2::config::is_auth() == true)
            return;

        log_manager::instance().money_log(p->type, p->vnum, p->gold);
    }

    void input_db::guild_money_change(const char *data) {
        packet_dg_guild_money_change *p = (packet_dg_guild_money_change *)data;

        guild *g = guild_manager::instance().touch_guild(p->guild);
        if (g) {
            g->recv_money_change(p->total_gold);
        }
    }

    void input_db::guild_withdraw_money(const char *data) {
        packet_dg_guild_money_withdraw *p = (packet_dg_guild_money_withdraw *)data;

        guild *g = guild_manager::instance().touch_guild(p->guild);
        if (g) {
            g->recv_withdraw_money_give(p->change_gold);
        }
    }

    void input_db::set_event_flag(const char *data) {
        packet_set_event_flag *p = (packet_set_event_flag *)data;
        quest::quest_manager::instance().set_event_flag(p->flag_name, p->value);
    }

    void input_db::create_object(const char *data) {
        using namespace building;
        manager::instance().load_object((object_table *)data);
    }

    void input_db::delete_object(const char *data) {
        using namespace building;
        manager::instance().delete_object(*(uint32_t *)data);
    }

    void input_db::update_land(const char *data) {
        using namespace building;
        manager::instance().update_land((land_table *)data);
    }

    void input_db::notice(const char *data) {

        char szBuf[256 + 1];
        strlcpy(szBuf, data, sizeof(szBuf));

        metin2::log::info("Notice: {}", szBuf);

        send_notice(szBuf);
    }

    void input_db::guild_war_reserve_add(guild_war_reserve *p) {
        guild_manager::instance().reserve_war_add(p);
    }

    void input_db::guild_war_reserve_delete(uint32_t id) {
        guild_manager::instance().reserve_war_delete(id);
    }

    void input_db::guild_war_bet(packet_gd_guild_war_bet *p) {
        guild_manager::instance().reserve_war_bet(p);
    }

    void input_db::marriage_add(packet_marriage_add *p) {
        metin2::log::info("Marriage: pid {:d} and {:d} married at {:d} ({} and {})", p->pid1, p->pid2, (uint32_t)p->t_marry_time, p->name1, p->name2);
        marriage::manager::instance().add(p->pid1, p->pid2, p->t_marry_time, p->name1, p->name2);
    }

    void input_db::marriage_update(packet_marriage_update *p) {
        metin2::log::debug("Marriage: pid {:d} and {:d}, love points {:d}, married {:d}", p->pid1, p->pid2, p->love_point, p->married);
        marriage::manager::instance().update(p->pid1, p->pid2, p->love_point, p->married);
    }

    void input_db::marriage_remove(packet_marriage_remove *p) {
        metin2::log::info("Marriage: pid {:d} and {:d} divorced", p->pid1, p->pid2);
        marriage::manager::instance().remove(p->pid1, p->pid2);
    }

    void input_db::wedding_request(packet_wedding_request *p) {
        marriage::wedding_manager::instance().request(p->pid1, p->pid2);
    }

    void input_db::wedding_ready(packet_wedding_ready *p) {
        metin2::log::info("Wedding: pid {:d} and {:d} ready on map {:d}", p->pid1, p->pid2, p->map_index);
        marriage::manager::instance().wedding_ready(p->pid1, p->pid2, p->map_index);
    }

    void input_db::wedding_start(packet_wedding_start *p) {
        metin2::log::info("Wedding: pid {:d} and {:d} started", p->pid1, p->pid2);
        marriage::manager::instance().wedding_start(p->pid1, p->pid2);
    }

    void input_db::wedding_end(packet_wedding_end *p) {
        metin2::log::info("Wedding: pid {:d} and {:d} ended", p->pid1, p->pid2);
        marriage::manager::instance().wedding_end(p->pid1, p->pid2);
    }

    void input_db::myshop_pricelist_res(desc *d, const packet_myshop_pricelist_header *p) {
        character *ch;

        if (!d || !(ch = d->get_character()))
            return;

        metin2::log::debug("Shop price list received for player {}", ch->name());
        ch->use_silk_botary_real(p);
    }

    void input_db::reload_admin(const char *data) {
        gm_new_clear();
        int ChunkSize = decode_2bytes(data);
        data += 2;
        int HostSize = decode_2bytes(data);
        data += 2 + ChunkSize * HostSize;

        data += 2;
        int size = decode_2bytes(data);
        data += 2;

        for (int n = 0; n < size; ++n) {
            t_admin_info &rAdminInfo = *(t_admin_info *)data;

            gm_new_insert(rAdminInfo);

            data += sizeof(t_admin_info);

            character *pChar = character_manager::instance().find_pc(rAdminInfo.name);
            if (pChar) {
                pChar->set_gm_level();
            }
        }
    }

    auto input_db::analyze(desc *d, uint8_t header, const char *data) -> int {
        desc *handle_desc = desc_manager::instance().find_by_handle(handle_);

        switch (header) {
        case HEADER_DG_BOOT:
            boot(data);
            break;

        case HEADER_DG_LOGIN_SUCCESS:
            login_success(handle_, data);
            break;

        case HEADER_DG_LOGIN_NOT_EXIST:
            login_failure(handle_desc, "NOID");
            break;

        case HEADER_DG_LOGIN_WRONG_PASSWD:
            login_failure(handle_desc, "WRONGPWD");
            break;

        case HEADER_DG_LOGIN_ALREADY:
            login_already(handle_desc, data);
            break;

        case HEADER_DG_PLAYER_LOAD_SUCCESS:
            player_load(handle_desc, data);
            break;

        case HEADER_DG_PLAYER_CREATE_SUCCESS:
            player_create_success(handle_desc, data);
            break;

        case HEADER_DG_PLAYER_CREATE_FAILED:
            player_create_failure(handle_desc, 0);
            break;

        case HEADER_DG_PLAYER_CREATE_ALREADY:
            player_create_failure(handle_desc, 1);
            break;

        case HEADER_DG_PLAYER_DELETE_SUCCESS:
            player_delete_success(handle_desc, data);
            break;

        case HEADER_DG_PLAYER_LOAD_FAILED:
            break;

        case HEADER_DG_PLAYER_DELETE_FAILED:
            player_delete_fail(handle_desc);
            break;

        case HEADER_DG_ITEM_LOAD:
            item_load(handle_desc, data);
            break;

        case HEADER_DG_QUEST_LOAD:
            quest_load(handle_desc, data);
            break;

        case HEADER_DG_AFFECT_LOAD:
            affect_load(handle_desc, data);
            break;

        case HEADER_DG_SAFEBOX_LOAD:
            safebox_load(handle_desc, data);
            break;

        case HEADER_DG_SAFEBOX_CHANGE_SIZE:
            safebox_change_size(handle_desc, data);
            break;

        case HEADER_DG_SAFEBOX_WRONG_PASSWORD:
            safebox_wrong_password(handle_desc);
            break;

        case HEADER_DG_SAFEBOX_CHANGE_PASSWORD_ANSWER:
            safebox_change_password_answer(handle_desc, data);
            break;

        case HEADER_DG_MALL_LOAD:
            mall_load(handle_desc, data);
            break;

        case HEADER_DG_EMPIRE_SELECT:
            empire_select(handle_desc, data);
            break;

        case HEADER_DG_MAP_LOCATIONS:
            map_locations(data);
            break;

        case HEADER_DG_P2P:
            p2p(data);
            break;

        case HEADER_DG_GUILD_SKILL_UPDATE:
            guild_skill_update(data);
            break;

        case HEADER_DG_GUILD_LOAD:
            guild_load(data);
            break;

        case HEADER_DG_GUILD_SKILL_RECHARGE:
            guild_skill_recharge();
            break;

        case HEADER_DG_GUILD_EXP_UPDATE:
            guild_exp_update(data);
            break;

        case HEADER_DG_PARTY_CREATE:
            party_create(data);
            break;

        case HEADER_DG_PARTY_DELETE:
            party_delete(data);
            break;

        case HEADER_DG_PARTY_ADD:
            party_add(data);
            break;

        case HEADER_DG_PARTY_REMOVE:
            party_remove(data);
            break;

        case HEADER_DG_PARTY_STATE_CHANGE:
            party_state_change(data);
            break;

        case HEADER_DG_PARTY_SET_MEMBER_LEVEL:
            party_set_member_level(data);
            break;

        case HEADER_DG_TIME:
            time(data);
            break;

        case HEADER_DG_GUILD_ADD_MEMBER:
            guild_add_member(data);
            break;

        case HEADER_DG_GUILD_REMOVE_MEMBER:
            guild_remove_member(data);
            break;

        case HEADER_DG_GUILD_CHANGE_GRADE:
            guild_change_grade(data);
            break;

        case HEADER_DG_GUILD_CHANGE_MEMBER_DATA:
            guild_change_member_data(data);
            break;

        case HEADER_DG_GUILD_DISBAND:
            guild_disband(data);
            break;

        case HEADER_DG_RELOAD_PROTO:
            reload_proto(data);
            break;

        case HEADER_DG_GUILD_WAR:
            guild_war(data);
            break;

        case HEADER_DG_GUILD_WAR_SCORE:
            guild_war_score(data);
            break;

        case HEADER_DG_GUILD_LADDER:
            guild_ladder(data);
            break;

        case HEADER_DG_GUILD_SKILL_USABLE_CHANGE:
            guild_skill_usable_change(data);
            break;

        case HEADER_DG_CHANGE_NAME:
            change_name(handle_desc, data);
            break;

        case HEADER_DG_AUTH_LOGIN:
            auth_login(handle_desc, data);
            break;

        case HEADER_DG_CHANGE_EMPIRE_PRIV:
            change_empire_priv(data);
            break;

        case HEADER_DG_CHANGE_GUILD_PRIV:
            change_guild_priv(data);
            break;

        case HEADER_DG_CHANGE_CHARACTER_PRIV:
            change_character_priv(data);
            break;

        case HEADER_DG_MONEY_LOG:
            money_log(data);
            break;

        case HEADER_DG_GUILD_WITHDRAW_MONEY_GIVE:
            guild_withdraw_money(data);
            break;

        case HEADER_DG_GUILD_MONEY_CHANGE:
            guild_money_change(data);
            break;

        case HEADER_DG_SET_EVENT_FLAG:
            set_event_flag(data);
            break;

        case HEADER_DG_CREATE_OBJECT:
            create_object(data);
            break;

        case HEADER_DG_DELETE_OBJECT:
            delete_object(data);
            break;

        case HEADER_DG_UPDATE_LAND:
            update_land(data);
            break;

        case HEADER_DG_NOTICE:
            notice(data);
            break;

        case HEADER_DG_GUILD_WAR_RESERVE_ADD:
            guild_war_reserve_add((guild_war_reserve *)data);
            break;

        case HEADER_DG_GUILD_WAR_RESERVE_DEL:
            guild_war_reserve_delete(*(uint32_t *)data);
            break;

        case HEADER_DG_GUILD_WAR_BET:
            guild_war_bet((packet_gd_guild_war_bet *)data);
            break;

        case HEADER_DG_MARRIAGE_ADD:
            marriage_add((packet_marriage_add *)data);
            break;

        case HEADER_DG_MARRIAGE_UPDATE:
            marriage_update((packet_marriage_update *)data);
            break;

        case HEADER_DG_MARRIAGE_REMOVE:
            marriage_remove((packet_marriage_remove *)data);
            break;

        case HEADER_DG_WEDDING_REQUEST:
            wedding_request((packet_wedding_request *)data);
            break;

        case HEADER_DG_WEDDING_READY:
            wedding_ready((packet_wedding_ready *)data);
            break;

        case HEADER_DG_WEDDING_START:
            wedding_start((packet_wedding_start *)data);
            break;

        case HEADER_DG_WEDDING_END:
            wedding_end((packet_wedding_end *)data);
            break;

        case HEADER_DG_MYSHOP_PRICELIST_RES:
            myshop_pricelist_res(handle_desc, (packet_myshop_pricelist_header *)data);
            break;
        case HEADER_DG_RELOAD_ADMIN:
            reload_admin(data);
            break;

        case HEADER_DG_ADD_MONARCH_MONEY:
            add_monarch_money(handle_desc, data);
            break;

        case HEADER_DG_DEC_MONARCH_MONEY:
            dec_monarch_money(handle_desc, data);
            break;

        case HEADER_DG_TAKE_MONARCH_MONEY:
            take_monarch_money(handle_desc, data);
            break;

        case HEADER_DG_RMCANDIDACY:
            monarch_admin_result(handle_desc, data, "Removed the candidacy of %s.", "%s is not a candidate.");
            break;

        case HEADER_DG_SETMONARCH:
            monarch_admin_result(handle_desc, data, "%s is now the monarch.", "Cannot make %s the monarch.");
            break;

        case HEADER_DG_RMMONARCH:
            monarch_admin_result(handle_desc, data, "%s is no longer the monarch.", "%s is not a monarch.");
            break;

        case HEADER_DG_CHANGE_MONARCH_LORD_ACK:
            change_monarch_lord((packet_change_monarch_lord_ack *)data);
            break;

        case HEADER_DG_UPDATE_MONARCH_INFO:
            update_monarch_info((monarch_info *)data);
            break;

        case HEADER_DG_ACK_CHANGE_GUILD_MASTER:
            this->guild_change_master((packet_change_guild_master *)data);
            break;
        case HEADER_DG_ACK_SPARE_ITEM_ID_RANGE:
            item_manager::instance().set_max_spare_item_id(*((item_id_range_table *)data));
            break;

        case HEADER_DG_UPDATE_HORSE_NAME:
        case HEADER_DG_ACK_HORSE_NAME:
            horse_name_manager::instance().update_horse_name(
                ((packet_update_horse_name *)data)->player_id,
                ((packet_update_horse_name *)data)->horse_name);
            break;

        case HEADER_DG_NEED_LOGIN_LOG:
            detail_log((packet_need_login_log_info *)data);
            break;
        case HEADER_DG_ITEMAWARD_INFORMER:
            item_award_informer((packet_item_award_infromer *)data);
            break;
        case HEADER_DG_RESPOND_CHANNELSTATUS:
            respond_channel_status(handle_desc, data);
            break;
        default:
            return (-1);
        }

        return 0;
    }

    auto input_db::process(desc *d, const void *orig, int bytes, int &bytes_processed) -> bool {
        const char *c_pData = (const char *)orig;
        uint8_t bHeader, bLastHeader = 0;
        int iSize;
        int iLastPacketLen = 0;

        for (buffer_left_ = bytes; buffer_left_ > 0;) {
            if (buffer_left_ < 9)
                return true;

            bHeader = *((uint8_t *)(c_pData));
            handle_ = *((uint32_t *)(c_pData + 1));
            iSize = *((uint32_t *)(c_pData + 5));

            metin2::log::debug("Db packet: header {:d}, handle {:d}, size {:d}, bytes {:d}", bHeader, handle_, iSize, bytes);

            if (buffer_left_ - 9 < iSize)
                return true;

            const char *pRealData = (c_pData + 9);

            if (analyze(d, bHeader, pRealData) < 0) {
                metin2::log::error("Unknown db packet header {:d} (last header {:d}, length {:d}, {:d} bytes left, host {})",
                    bHeader, bLastHeader, iLastPacketLen, buffer_left_, d->host_name());
            }

            c_pData += 9 + iSize;
            buffer_left_ -= 9 + iSize;
            bytes_processed += 9 + iSize;

            iLastPacketLen = 9 + iSize;
            bLastHeader = bHeader;
        }

        return true;
    }

    void input_db::add_monarch_money(desc *d, const char *data) {
        int Empire = *(int *)data;
        data += sizeof(int);

        int Money = *(int *)data;
        data += sizeof(int);

        monarch::instance().add_money(Money, Empire);

        uint32_t pid = monarch::instance().monarch_pid(Empire);

        character *ch = character_manager::instance().find_by_pid(pid);

        if (ch) {
            if (number(1, 100) > 95)
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("현재 %s 국고에는 %u 의 돈이 있습니다"), empire_name(Empire), monarch::instance().money(Empire));
        }
    }

    void input_db::dec_monarch_money(desc *d, const char *data) {
        int Empire = *(int *)data;
        data += sizeof(int);

        int Money = *(int *)data;
        data += sizeof(int);

        monarch::instance().dec_money(Money, Empire);

        uint32_t pid = monarch::instance().monarch_pid(Empire);

        character *ch = character_manager::instance().find_by_pid(pid);

        if (ch) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("현재 %s 국고에는 %d 의 돈이 있습니다"), empire_name(Empire), monarch::instance().money(Empire));
        }
    }

    void input_db::take_monarch_money(desc *d, const char *data) {
        int Empire = *(int *)data;
        data += sizeof(int);

        int Money = *(int *)data;
        data += sizeof(int);

        if (!monarch::instance().dec_money(Money, Empire)) {
            if (!d)
                return;

            if (!d->get_character())
                return;

            character *ch = d->get_character();
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("국고에 돈이 부족하거나 돈을 가져올수 없는 상황입니다"));
        }
    }

    void input_db::change_monarch_lord(packet_change_monarch_lord_ack *info) {
        char notice[256];
        snprintf(notice, sizeof(notice), lc_text("%s의 군주가 %s 님으로 교체되었습니다."), empire_name(info->empire), info->name);
        send_notice(notice);
    }

    void input_db::monarch_admin_result(desc *d, const char *data, const char *success, const char *failure) {
        const int result = *reinterpret_cast<const int *>(data);
        char name[32 + 1] = {};
        std::memcpy(name, data + sizeof(int), 32);

        if (d && d->get_character())
            d->get_character()->chat_packet(CHAT_TYPE_INFO, result == 1 ? success : failure, name);
    }

    void input_db::update_monarch_info(monarch_info *info) {
        monarch::instance().set_monarch_info(info);
        metin2::log::info("Monarch info updated");
    }

    void input_db::guild_change_master(packet_change_guild_master *p) {
        guild_manager::instance().change_master(p->guild_id);
    }

    void input_db::detail_log(const packet_need_login_log_info *info) {
        character *pChar = character_manager::instance().find_by_pid(info->player_id);

        if (nullptr != pChar) {
            log_manager::instance().detail_login_log(true, pChar);
        }
    }

    void input_db::item_award_informer(packet_item_award_infromer *data) {
        desc *d = desc_manager::instance().find_by_login_name(data->login);

        if (d == nullptr)
            return;
        else {
            if (d->get_character()) {
                character *ch = d->get_character();
                ch->set_item_award_vnum(data->vnum);
                ch->set_item_award_cmd(data->command);

                if (d->is_phase(PHASE_GAME)) {
                    quest::quest_manager::instance().item_informer(ch->player_id(), ch->item_award_vnum());
                }
            }
        }
    }

    void input_db::respond_channel_status(desc *desc, const char *pc_data) {
        if (!desc) {
            return;
        }
        const int nSize = decode_4bytes(pc_data);
        pc_data += sizeof(nSize);

        uint8_t bHeader = HEADER_GC_RESPOND_CHANNELSTATUS;
        desc->buffered_packet(&bHeader, sizeof(uint8_t));
        desc->buffered_packet(&nSize, sizeof(nSize));
        if (0 < nSize) {
            desc->buffered_packet(pc_data, sizeof(channel_status) * nSize);
        }
        uint8_t bSuccess = 1;
        desc->packet(&bSuccess, sizeof(bSuccess));
        desc->set_channel_status_requested(false);
    }
}
