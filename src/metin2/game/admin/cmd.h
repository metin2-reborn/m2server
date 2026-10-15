#pragma once

#include <metin2/game/app/typedef.h>

#include <cstddef>
#include <cstdint>
#include <string>

#define ACMD(name)  void (name)(character * ch, const char *argument, int cmd, int subcmd)

namespace metin2::game::inline admin {

struct command_info
{
	const char * command;
	void (*command_pointer) (character * ch, const char *argument, int cmd, int subcmd);
	int subcmd;
	int minimum_position;
	int gm_level;
};

extern struct command_info cmd_info[];

extern void interpret_command(character * ch, const char * argument, size_t len);
extern void interpreter_set_privilege(const char * cmd, int lvl);

enum scmd_action
{
	SCMD_SLAP,
	SCMD_KISS,
	SCMD_FRENCH_KISS,
	SCMD_HUG,
	SCMD_LONG_HUG,
};

enum scmd_cmd
{
	SCMD_LOGOUT,
	SCMD_QUIT,
	SCMD_PHASE_SELECT,
};

enum scmd_restart
{
	SCMD_RESTART_TOWN,
	SCMD_RESTART_HERE
};

enum scmd_xmas
{
	SCMD_XMAS_BOOM,
	SCMD_XMAS_SNOW,
	SCMD_XMAS_SANTA,
};

extern void shutdown(int iSec);
extern void send_notice(const char * c_pszBuf);		// Notice to this game server only
extern void send_log(const char * c_pszBuf);		// Notice to GMs only
extern void broadcast_notice(const char * c_pszBuf);	// Notice to all servers
extern void send_notice_map(const char* c_pszBuf, int nMapIndex, bool bBigFont); // Notice to the specified map only
extern void send_monarch_notice(uint8_t bEmpire, const char * c_pszBuf);		// Notice to the same empire

// LUA_ADD_BGM_INFO
// END_OF_LUA_ADD_BGM_INFO

// LUA_ADD_GOTO_INFO
extern void character_add_goto_info(const std::string& c_st_name, uint8_t empire, int mapIndex, uint32_t x, uint32_t y);
// END_OF_LUA_ADD_GOTO_INFO

ACMD(do_block_chat);
ACMD(do_monarch_mob);
ACMD(do_all_skill_master);
ACMD(do_item_full_set);
ACMD(do_attr_full_set);
ACMD(do_in_game_mall);
}
