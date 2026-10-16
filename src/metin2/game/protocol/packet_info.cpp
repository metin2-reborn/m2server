#include <metin2/game/protocol/packet_info.h>

#include <metin2/game/protocol/packet.h>

#include <cstdint>

namespace metin2::game::inline protocol {

    void packet_table::set(uint8_t header, int size, const char *name, bool sequence) {
        packet_spec &spec = specs_[header];

        if (spec.name)
            return;

        spec = {size + (sequence ? static_cast<int>(sizeof(uint8_t)) : 0), name, sequence};
    }

    void packet_table::set_sequence(uint8_t header, bool sequence) {
        packet_spec &spec = specs_[header];

        if (!spec.name || spec.sequence == sequence)
            return;

        spec.size += sequence ? 1 : -1;
        spec.sequence = sequence;
    }

    namespace {

        auto make_client_packet_table() -> packet_table {
            packet_table table;
            table.set(HEADER_CG_TEXT, sizeof(packet_cg_text), "Text", false);
            table.set(HEADER_CG_HANDSHAKE, sizeof(packet_cg_handshake), "Handshake", false);
            table.set(HEADER_CG_TIME_SYNC, sizeof(packet_cg_handshake), "TimeSync", true);
            table.set(HEADER_CG_MARK_LOGIN, sizeof(packet_cg_mark_login), "MarkLogin", false);
            table.set(HEADER_CG_MARK_IDXLIST, sizeof(packet_cg_mark_idx_list), "MarkIdxList", false);
            table.set(HEADER_CG_MARK_CRCLIST, sizeof(packet_cg_mark_crc_list), "MarkCrcList", false);
            table.set(HEADER_CG_MARK_UPLOAD, sizeof(packet_cg_mark_upload), "MarkUpload", false);
            table.set(HEADER_CG_KEY_AGREEMENT, sizeof(packet_key_agreement), "KeyAgreement", false);

            table.set(HEADER_CG_GUILD_SYMBOL_UPLOAD, sizeof(packet_cg_guild_symbol_upload), "SymbolUpload", false);
            table.set(HEADER_CG_SYMBOL_CRC, sizeof(packet_cg_symbol_crc), "SymbolCRC", false);
            table.set(HEADER_CG_LOGIN, sizeof(packet_cg_login), "Login", true);
            table.set(HEADER_CG_LOGIN2, sizeof(packet_cg_login2), "Login2", true);
            table.set(HEADER_CG_LOGIN3, sizeof(packet_cg_login3), "Login3", true);
            table.set(HEADER_CG_ATTACK, sizeof(packet_cg_attack), "Attack", true);
            table.set(HEADER_CG_CHAT, sizeof(packet_cg_chat), "Chat", true);
            table.set(HEADER_CG_WHISPER, sizeof(packet_cg_whisper), "Whisper", true);

            table.set(HEADER_CG_CHARACTER_SELECT, sizeof(packet_cg_player_select), "Select", true);
            table.set(HEADER_CG_CHARACTER_CREATE, sizeof(packet_cg_player_create), "Create", true);
            table.set(HEADER_CG_CHARACTER_DELETE, sizeof(packet_cg_player_delete), "Delete", true);
            table.set(HEADER_CG_ENTERGAME, sizeof(packet_cg_enter_game), "EnterGame", true);

            table.set(HEADER_CG_ITEM_USE, sizeof(packet_cg_item_use), "ItemUse", true);
            table.set(HEADER_CG_ITEM_DROP, sizeof(packet_cg_item_drop), "ItemDrop", true);
            table.set(HEADER_CG_ITEM_DROP2, sizeof(packet_cg_item_drop2), "ItemDrop2", true);
            table.set(HEADER_CG_ITEM_MOVE, sizeof(packet_cg_item_move), "ItemMove", true);
            table.set(HEADER_CG_ITEM_PICKUP, sizeof(packet_cg_item_pickup), "ItemPickup", true);

            table.set(HEADER_CG_QUICKSLOT_ADD, sizeof(packet_cg_quickslot_add), "QuickslotAdd", true);
            table.set(HEADER_CG_QUICKSLOT_DEL, sizeof(packet_cg_quickslot_del), "QuickslotDel", true);
            table.set(HEADER_CG_QUICKSLOT_SWAP, sizeof(packet_cg_quickslot_swap), "QuickslotSwap", true);

            table.set(HEADER_CG_SHOP, sizeof(packet_cg_shop), "Shop", true);

            table.set(HEADER_CG_ON_CLICK, sizeof(packet_cg_on_click), "OnClick", true);
            table.set(HEADER_CG_EXCHANGE, sizeof(packet_cg_exchange), "Exchange", true);
            table.set(HEADER_CG_CHARACTER_POSITION, sizeof(packet_cg_position), "Position", true);
            table.set(HEADER_CG_SCRIPT_ANSWER, sizeof(packet_cg_script_answer), "ScriptAnswer", true);
            table.set(HEADER_CG_SCRIPT_BUTTON, sizeof(packet_cg_script_button), "ScriptButton", true);
            table.set(HEADER_CG_QUEST_INPUT_STRING, sizeof(packet_cg_quest_input_string), "QuestInputString", true);
            table.set(HEADER_CG_QUEST_CONFIRM, sizeof(packet_cg_quest_confirm), "QuestConfirm", true);

            table.set(HEADER_CG_MOVE, sizeof(packet_cg_move), "Move", true);
            table.set(HEADER_CG_SYNC_POSITION, sizeof(packet_cg_sync_position), "SyncPosition", true);

            table.set(HEADER_CG_FLY_TARGETING, sizeof(packet_cg_fly_targeting), "FlyTarget", true);
            table.set(HEADER_CG_ADD_FLY_TARGETING, sizeof(packet_cg_fly_targeting), "AddFlyTarget", true);
            table.set(HEADER_CG_SHOOT, sizeof(packet_cg_shoot), "Shoot", true);

            table.set(HEADER_CG_USE_SKILL, sizeof(packet_cg_use_skill), "UseSkill", true);

            table.set(HEADER_CG_ITEM_USE_TO_ITEM, sizeof(packet_cg_item_use_to_item), "UseItemToItem", true);
            table.set(HEADER_CG_TARGET, sizeof(packet_cg_target), "Target", true);
            table.set(HEADER_CG_WARP, sizeof(packet_cg_warp), "Warp", true);
            table.set(HEADER_CG_MESSENGER, sizeof(packet_cg_messenger), "Messenger", true);

            table.set(HEADER_CG_PARTY_REMOVE, sizeof(packet_cg_party_remove), "PartyRemove", true);
            table.set(HEADER_CG_PARTY_INVITE, sizeof(packet_cg_party_invite), "PartyInvite", true);
            table.set(HEADER_CG_PARTY_INVITE_ANSWER, sizeof(packet_cg_party_invite_answer), "PartyInviteAnswer", true);
            table.set(HEADER_CG_PARTY_SET_STATE, sizeof(packet_cg_party_set_state), "PartySetState", true);
            table.set(HEADER_CG_PARTY_USE_SKILL, sizeof(packet_cg_party_use_skill), "PartyUseSkill", true);
            table.set(HEADER_CG_PARTY_PARAMETER, sizeof(packet_cg_party_parameter), "PartyParam", true);

            table.set(HEADER_CG_EMPIRE, sizeof(packet_cg_empire), "Empire", true);
            table.set(HEADER_CG_SAFEBOX_CHECKOUT, sizeof(packet_cg_safebox_checkout), "SafeboxCheckout", true);
            table.set(HEADER_CG_SAFEBOX_CHECKIN, sizeof(packet_cg_safebox_checkin), "SafeboxCheckin", true);

            table.set(HEADER_CG_SAFEBOX_ITEM_MOVE, sizeof(packet_cg_item_move), "SafeboxItemMove", true);

            table.set(HEADER_CG_GUILD, sizeof(packet_cg_guild), "Guild", true);
            table.set(HEADER_CG_ANSWER_MAKE_GUILD, sizeof(packet_cg_answer_make_guild), "AnswerMakeGuild", true);

            table.set(HEADER_CG_FISHING, sizeof(packet_cg_fishing), "Fishing", true);
            table.set(HEADER_CG_ITEM_GIVE, sizeof(packet_cg_give_item), "ItemGive", true);
            table.set(HEADER_CG_HACK, sizeof(packet_cg_hack), "Hack", true);
            table.set(HEADER_CG_MYSHOP, sizeof(packet_cg_my_shop), "MyShop", true);

            table.set(HEADER_CG_REFINE, sizeof(packet_cg_refine), "Refine", true);
            table.set(HEADER_CG_CHANGE_NAME, sizeof(packet_cg_change_name), "ChangeName", true);

            table.set(HEADER_CG_CLIENT_VERSION, sizeof(packet_cg_client_version), "Version", true);
            table.set(HEADER_CG_CLIENT_VERSION2, sizeof(packet_cg_client_version2), "Version", true);
            table.set(HEADER_CG_PONG, sizeof(uint8_t), "Pong", true);
            table.set(HEADER_CG_MALL_CHECKOUT, sizeof(packet_cg_safebox_checkout), "MallCheckout", true);

            table.set(HEADER_CG_SCRIPT_SELECT_ITEM, sizeof(packet_cg_script_select_item), "ScriptSelectItem", true);

            table.set(HEADER_CG_DRAGON_SOUL_REFINE, sizeof(packet_cg_dragon_soul_refine), "DragonSoulRefine", false);
            table.set(HEADER_CG_STATE_CHECKER, sizeof(uint8_t), "ServerStateCheck", false);
            return table;
        }

        auto make_p2p_packet_table() -> packet_table {
            packet_table table;
            table.set(HEADER_GG_SETUP, sizeof(packet_gg_setup), "Setup", false);
            table.set(HEADER_GG_LOGIN, sizeof(packet_gg_login), "Login", false);
            table.set(HEADER_GG_LOGOUT, sizeof(packet_gg_logout), "Logout", false);
            table.set(HEADER_GG_RELAY, sizeof(packet_gg_relay), "Relay", false);
            table.set(HEADER_GG_NOTICE, sizeof(packet_gg_notice), "Notice", false);
            table.set(HEADER_GG_SHUTDOWN, sizeof(packet_gg_shutdown), "Shutdown", false);
            table.set(HEADER_GG_GUILD, sizeof(packet_gg_guild), "Guild", false);
            table.set(HEADER_GG_SHOUT, sizeof(packet_gg_shout), "Shout", false);
            table.set(HEADER_GG_DISCONNECT, sizeof(packet_gg_disconnect), "Disconnect", false);
            table.set(HEADER_GG_MESSENGER_ADD, sizeof(packet_gg_messenger), "MessengerAdd", false);
            table.set(HEADER_GG_MESSENGER_REMOVE, sizeof(packet_gg_messenger), "MessengerRemove", false);
            table.set(HEADER_GG_FIND_POSITION, sizeof(packet_gg_find_position), "FindPosition", false);
            table.set(HEADER_GG_WARP_CHARACTER, sizeof(packet_gg_warp_character), "WarpCharacter", false);
            table.set(HEADER_GG_MESSENGER_MOBILE, sizeof(packet_gg_messenger_mobile), "MessengerMobile", false);
            table.set(HEADER_GG_GUILD_WAR_ZONE_MAP_INDEX, sizeof(packet_gg_guild_war_map_index), "GuildWarMapIndex", false);
            table.set(HEADER_GG_TRANSFER, sizeof(packet_gg_transfer), "Transfer", false);
            table.set(HEADER_GG_XMAS_WARP_SANTA, sizeof(packet_gg_xmas_warp_santa), "XmasWarpSanta", false);
            table.set(HEADER_GG_XMAS_WARP_SANTA_REPLY, sizeof(packet_gg_xmas_warp_santa_reply), "XmasWarpSantaReply", false);
            table.set(HEADER_GG_CHECK_CLIENT_VERSION, sizeof(uint8_t), "CheckClientVersion", false);

            table.set(HEADER_GG_BLOCK_CHAT, sizeof(packet_gg_block_chat), "BlockChat", false);
            table.set(HEADER_GG_SIEGE, sizeof(packet_gg_siege), "Siege", false);

            table.set(HEADER_GG_MONARCH_NOTICE, sizeof(packet_gg_monarch_notice), "MonarchNotice", false);
            table.set(HEADER_GG_MONARCH_TRANSFER, sizeof(packet_monarch_gg_transfer), "MonarchTransfer", false);
            table.set(HEADER_GG_CHECK_AWAKENESS, sizeof(packet_gg_check_awakeness), "CheckAwakeness", false);
            return table;
        }

    }

    auto client_packet_table() -> const packet_table & {
        static const packet_table table = make_client_packet_table();
        return table;
    }

    auto handshake_packet_table() -> const packet_table & {
        static const packet_table table = [] {
            packet_table t = client_packet_table();
            t.set_sequence(HEADER_CG_PONG, false);
            return t;
        }();
        return table;
    }

    auto p2p_packet_table() -> const packet_table & {
        static const packet_table table = make_p2p_packet_table();
        return table;
    }
}
