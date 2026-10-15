#include <metin2/game/protocol/packet_info.h>

#include <metin2/game/protocol/packet.h>

#include <metin2/core/utils.h>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <map>

namespace metin2::game::inline protocol {

packet_info::packet_info()
	: current_packet_(nullptr), start_time_(0)
{
}

packet_info::~packet_info()
{
}

void packet_info::set(int header, int size_value, const char * name_value, bool seq)
{
	if (packet_map_.find(header) != packet_map_.end())
		return;

	packet_element * element = &packet_map_[header];

	element->size = size_value;
	element->name.assign(name_value);
	element->called = 0;
	element->load = 0;

	element->sequence_packet = seq;

	if (element->sequence_packet)
		element->size += sizeof(uint8_t);
}

auto packet_info::get(int header, int * size, const char ** ppsz_name) -> bool
{
	std::map<int, packet_element>::iterator it = packet_map_.find(header);

	if (it == packet_map_.end())
		return false;

	*size = it->second.size;
	*ppsz_name = it->second.name.c_str();

	current_packet_ = &it->second;
	return true;
}

auto packet_info::is_sequence(int header) -> bool
{
	packet_element * pkElement = element(header);
	return pkElement ? pkElement->sequence_packet : false;
}

void packet_info::set_sequence(int header, bool seq)
{
	packet_element * pkElem = element(header);

	if (pkElem)
	{
		if (seq)
		{
			if (!pkElem->sequence_packet)
				pkElem->size++;
		}
		else
		{
			if (pkElem->sequence_packet)
				pkElem->size--;
		}

		pkElem->sequence_packet = seq;
	}
}

auto packet_info::element(int header) -> packet_element *
{
	std::map<int, packet_element>::iterator it = packet_map_.find(header);

	if (it == packet_map_.end())
		return nullptr;

	return &it->second;
}

void packet_info::start()
{
	assert(current_packet_ != nullptr);
	start_time_ = get_dword_time();
}

void packet_info::end()
{
	++current_packet_->called;
	current_packet_->load += get_dword_time() - start_time_;
}

void packet_info::log(const char * file_name)
{
	FILE * fp;

	fp = fopen(file_name, "w");

	if (!fp)
		return;

	std::map<int, packet_element>::iterator it = packet_map_.begin();

	fprintf(fp, "Name             Called     Load       Ratio\n");

	while (it != packet_map_.end())
	{
		packet_element * p = &it->second;
		++it;

		fprintf(fp, "%-16s %-10d %-10u %.2f\n",
				p->name.c_str(),
				p->called,
				p->load,
				p->called != 0 ? (float) p->load / p->called : 0.0f);
	}

	fclose(fp);
}

packet_info_cg::packet_info_cg()
{
	set(HEADER_CG_TEXT, sizeof(packet_cg_text), "Text", false);
	set(HEADER_CG_HANDSHAKE, sizeof(packet_cg_handshake), "Handshake", false);
	set(HEADER_CG_TIME_SYNC, sizeof(packet_cg_handshake), "TimeSync", true);
	set(HEADER_CG_MARK_LOGIN, sizeof(packet_cg_mark_login), "MarkLogin", false);
	set(HEADER_CG_MARK_IDXLIST, sizeof(packet_cg_mark_idx_list), "MarkIdxList", false);
	set(HEADER_CG_MARK_CRCLIST, sizeof(packet_cg_mark_crc_list), "MarkCrcList", false);
	set(HEADER_CG_MARK_UPLOAD, sizeof(packet_cg_mark_upload), "MarkUpload", false);
	set(HEADER_CG_KEY_AGREEMENT, sizeof(packet_key_agreement), "KeyAgreement", false);

	set(HEADER_CG_GUILD_SYMBOL_UPLOAD, sizeof(packet_cg_guild_symbol_upload), "SymbolUpload", false);
	set(HEADER_CG_SYMBOL_CRC, sizeof(packet_cg_symbol_crc), "SymbolCRC", false);
	set(HEADER_CG_LOGIN, sizeof(packet_cg_login), "Login", true);
	set(HEADER_CG_LOGIN2, sizeof(packet_cg_login2), "Login2", true);
	set(HEADER_CG_LOGIN3, sizeof(packet_cg_login3), "Login3", true);
	set(HEADER_CG_ATTACK, sizeof(packet_cg_attack), "Attack", true);
	set(HEADER_CG_CHAT, sizeof(packet_cg_chat), "Chat", true);
	set(HEADER_CG_WHISPER, sizeof(packet_cg_whisper), "Whisper", true);

	set(HEADER_CG_CHARACTER_SELECT, sizeof(packet_cg_player_select), "Select", true);
	set(HEADER_CG_CHARACTER_CREATE, sizeof(packet_cg_player_create), "Create", true);
	set(HEADER_CG_CHARACTER_DELETE, sizeof(packet_cg_player_delete), "Delete", true);
	set(HEADER_CG_ENTERGAME, sizeof(packet_cg_enter_game), "EnterGame", true);

	set(HEADER_CG_ITEM_USE, sizeof(packet_cg_item_use), "ItemUse", true);
	set(HEADER_CG_ITEM_DROP, sizeof(packet_cg_item_drop), "ItemDrop", true);
	set(HEADER_CG_ITEM_DROP2, sizeof(packet_cg_item_drop2), "ItemDrop2", true);
	set(HEADER_CG_ITEM_MOVE, sizeof(packet_cg_item_move), "ItemMove", true);
	set(HEADER_CG_ITEM_PICKUP, sizeof(packet_cg_item_pickup), "ItemPickup", true);

	set(HEADER_CG_QUICKSLOT_ADD, sizeof(packet_cg_quickslot_add), "QuickslotAdd", true);
	set(HEADER_CG_QUICKSLOT_DEL, sizeof(packet_cg_quickslot_del), "QuickslotDel", true);
	set(HEADER_CG_QUICKSLOT_SWAP, sizeof(packet_cg_quickslot_swap), "QuickslotSwap", true);

	set(HEADER_CG_SHOP, sizeof(packet_cg_shop), "Shop", true);

	set(HEADER_CG_ON_CLICK, sizeof(packet_cg_on_click), "OnClick", true);
	set(HEADER_CG_EXCHANGE, sizeof(packet_cg_exchange), "Exchange", true);
	set(HEADER_CG_CHARACTER_POSITION, sizeof(packet_cg_position), "Position", true);
	set(HEADER_CG_SCRIPT_ANSWER, sizeof(packet_cg_script_answer), "ScriptAnswer", true);
	set(HEADER_CG_SCRIPT_BUTTON, sizeof(packet_cg_script_button), "ScriptButton", true);
	set(HEADER_CG_QUEST_INPUT_STRING, sizeof(packet_cg_quest_input_string), "QuestInputString", true);
	set(HEADER_CG_QUEST_CONFIRM, sizeof(packet_cg_quest_confirm), "QuestConfirm", true);

	set(HEADER_CG_MOVE, sizeof(packet_cg_move), "Move", true);
	set(HEADER_CG_SYNC_POSITION, sizeof(packet_cg_sync_position), "SyncPosition", true);

	set(HEADER_CG_FLY_TARGETING, sizeof(packet_cg_fly_targeting), "FlyTarget", true);
	set(HEADER_CG_ADD_FLY_TARGETING, sizeof(packet_cg_fly_targeting), "AddFlyTarget", true);
	set(HEADER_CG_SHOOT, sizeof(packet_cg_shoot), "Shoot", true);

	set(HEADER_CG_USE_SKILL, sizeof(packet_cg_use_skill), "UseSkill", true);

	set(HEADER_CG_ITEM_USE_TO_ITEM, sizeof(packet_cg_item_use_to_item), "UseItemToItem", true);
	set(HEADER_CG_TARGET, sizeof(packet_cg_target), "Target", true);
	set(HEADER_CG_WARP, sizeof(packet_cg_warp), "Warp", true);
	set(HEADER_CG_MESSENGER, sizeof(packet_cg_messenger), "Messenger", true);

	set(HEADER_CG_PARTY_REMOVE, sizeof(packet_cg_party_remove), "PartyRemove", true);
	set(HEADER_CG_PARTY_INVITE, sizeof(packet_cg_party_invite), "PartyInvite", true);
	set(HEADER_CG_PARTY_INVITE_ANSWER, sizeof(packet_cg_party_invite_answer), "PartyInviteAnswer", true);
	set(HEADER_CG_PARTY_SET_STATE, sizeof(packet_cg_party_set_state), "PartySetState", true);
	set(HEADER_CG_PARTY_USE_SKILL, sizeof(packet_cg_party_use_skill), "PartyUseSkill", true);
	set(HEADER_CG_PARTY_PARAMETER, sizeof(packet_cg_party_parameter), "PartyParam", true);

	set(HEADER_CG_EMPIRE, sizeof(packet_cg_empire), "Empire", true);
	set(HEADER_CG_SAFEBOX_CHECKOUT, sizeof(packet_cg_safebox_checkout), "SafeboxCheckout", true);
	set(HEADER_CG_SAFEBOX_CHECKIN, sizeof(packet_cg_safebox_checkin), "SafeboxCheckin", true);

	set(HEADER_CG_SAFEBOX_ITEM_MOVE, sizeof(packet_cg_item_move), "SafeboxItemMove", true);

	set(HEADER_CG_GUILD, sizeof(packet_cg_guild), "Guild", true);
	set(HEADER_CG_ANSWER_MAKE_GUILD, sizeof(packet_cg_answer_make_guild), "AnswerMakeGuild", true);

	set(HEADER_CG_FISHING, sizeof(packet_cg_fishing), "Fishing", true);
	set(HEADER_CG_ITEM_GIVE, sizeof(packet_cg_give_item), "ItemGive", true);
	set(HEADER_CG_HACK, sizeof(packet_cg_hack), "Hack", true);
	set(HEADER_CG_MYSHOP, sizeof(packet_cg_my_shop), "MyShop", true);

	set(HEADER_CG_REFINE, sizeof(packet_cg_refine), "Refine", true);
	set(HEADER_CG_CHANGE_NAME, sizeof(packet_cg_change_name), "ChangeName", true);

	set(HEADER_CG_CLIENT_VERSION, sizeof(packet_cg_client_version), "Version", true);
	set(HEADER_CG_CLIENT_VERSION2, sizeof(packet_cg_client_version2), "Version", true);
	set(HEADER_CG_PONG, sizeof(uint8_t), "Pong", true);
	set(HEADER_CG_MALL_CHECKOUT, sizeof(packet_cg_safebox_checkout), "MallCheckout", true);

	set(HEADER_CG_SCRIPT_SELECT_ITEM, sizeof(packet_cg_script_select_item), "ScriptSelectItem", true);

	set(HEADER_CG_DRAGON_SOUL_REFINE, sizeof(packet_cg_dragon_soul_refine), "DragonSoulRefine", false);
	set(HEADER_CG_STATE_CHECKER, sizeof(uint8_t), "ServerStateCheck", false);
	
}

packet_info_cg::~packet_info_cg()
{
	log("packet_info.txt");
}

packet_info_gg::packet_info_gg()
{
	set(HEADER_GG_SETUP,		sizeof(packet_gg_setup),		"Setup", false);
	set(HEADER_GG_LOGIN,		sizeof(packet_gg_login),		"Login", false);
	set(HEADER_GG_LOGOUT,		sizeof(packet_gg_logout),	"Logout", false);
	set(HEADER_GG_RELAY,		sizeof(packet_gg_relay),		"Relay", false);
	set(HEADER_GG_NOTICE,		sizeof(packet_gg_notice),	"Notice", false);
	set(HEADER_GG_SHUTDOWN,		sizeof(packet_gg_shutdown),	"Shutdown", false);
	set(HEADER_GG_GUILD,		sizeof(packet_gg_guild),		"Guild", false);
	set(HEADER_GG_SHOUT,		sizeof(packet_gg_shout),		"Shout", false);
	set(HEADER_GG_DISCONNECT,	    	sizeof(packet_gg_disconnect),	"Disconnect", false);
	set(HEADER_GG_MESSENGER_ADD,	sizeof(packet_gg_messenger),	"MessengerAdd", false);
	set(HEADER_GG_MESSENGER_REMOVE,	sizeof(packet_gg_messenger),	"MessengerRemove", false);
	set(HEADER_GG_FIND_POSITION,	sizeof(packet_gg_find_position),	"FindPosition", false);
	set(HEADER_GG_WARP_CHARACTER,	sizeof(packet_gg_warp_character),	"WarpCharacter", false);
	set(HEADER_GG_MESSENGER_MOBILE,	sizeof(packet_gg_messenger_mobile), "MessengerMobile", false);
	set(HEADER_GG_GUILD_WAR_ZONE_MAP_INDEX, sizeof(packet_gg_guild_war_map_index), "GuildWarMapIndex", false);
	set(HEADER_GG_TRANSFER,		sizeof(packet_gg_transfer),	"Transfer", false);
	set(HEADER_GG_XMAS_WARP_SANTA,	sizeof(packet_gg_xmas_warp_santa),	"XmasWarpSanta", false);
	set(HEADER_GG_XMAS_WARP_SANTA_REPLY, sizeof(packet_gg_xmas_warp_santa_reply), "XmasWarpSantaReply", false);
	set(HEADER_GG_CHECK_CLIENT_VERSION, sizeof(uint8_t),			"CheckClientVersion", false);

	set(HEADER_GG_BLOCK_CHAT,		sizeof(packet_gg_block_chat),	"BlockChat", false);
	set(HEADER_GG_SIEGE,	sizeof(packet_gg_siege),	"Siege", false);

	set(HEADER_GG_MONARCH_NOTICE,		sizeof(packet_gg_monarch_notice),	"MonarchNotice", false);
	set(HEADER_GG_MONARCH_TRANSFER,		sizeof(packet_monarch_gg_transfer),	"MonarchTransfer", false);
	set(HEADER_GG_CHECK_AWAKENESS,		sizeof(packet_gg_check_awakeness),	"CheckAwakeness",		false);
}

packet_info_gg::~packet_info_gg()
{
	log("p2p_packet_info.txt");
}
}
