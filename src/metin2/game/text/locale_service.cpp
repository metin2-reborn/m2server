#include <metin2/game/text/locale_service.h>

#include <metin2/game/text/banword.h>
#include <metin2/game/app/config.h>
#include <metin2/game/empire/empire_text_convert.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/characters/mob_manager.h>
#include <metin2/game/combat/skill_power.h>
#include <metin2/game/app/utils.h>

#include <metin2/proto/length.h>
#include <metin2/log/log.h>

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>

namespace metin2::game::inline text {


// The server runs the english locale only (common.locale LOCALE = english).
static const std::string	g_stServiceBasePath = "locale/english";
static const std::string	g_stServiceMapPath = "locale/english/map";

std::string g_stLocale = "latin1";
std::string g_stLocaleFilename = "locale/english/locale_string.txt";


static_assert(PLAYER_PER_ACCOUNT == 4);

static int check_name_independent(const char * str)
{
	if (CBanwordManager::instance().CheckString(str, strlen(str)))
		return 0;

	// Cannot be made with a monster name.
	char szTmp[256];
	str_lower(str, szTmp, sizeof(szTmp));

	if (CMobManager::instance().Get(szTmp, false))
		return 0;

	return 1;
}

// Names: letters and digits only.
int check_name(const char * str)
{
	const char*	tmp;

	if (!str || !*str)
		return 0;

	if (strlen(str) < 2)
		return 0;

	for (tmp = str; *tmp; ++tmp)
	{
		// Only letters and digits allowed
		if (isdigit(*tmp) || isalpha(*tmp))
			continue;
		else
			return 0;
	}

	return check_name_independent(str);
}
// DISABLE_SPECIAL_CHAR_NAMING

void LocaleService_LoadLocaleStringFile()
{
	if (g_stLocaleFilename.empty())
		return;

	if (metin2::config::is_auth())
		return;

	fprintf(stderr, "LocaleService %s\n", g_stLocaleFilename.c_str());

	locale_init(g_stLocaleFilename.c_str());
}

void LocaleService_LoadEmpireTextConvertTables()
{
	char szFileName[256];

	for (int iEmpire = 1; iEmpire <= 3; ++iEmpire)
	{
		snprintf(szFileName, sizeof(szFileName), "%s/lang%d.cvt", LocaleService_GetBasePath().c_str(), iEmpire);
		metin2::log::info( "Load {}", szFileName);

		LoadEmpireTextConvertTable(iEmpire, szFileName);
	}
}

bool LocaleService_Init(const std::string& c_rstServiceName)
{
	if (c_rstServiceName != "english")
	{
		metin2::log::error("unsupported locale {} (only english)", c_rstServiceName.c_str());
		return false;
	}

	g_stQuestDir = g_stServiceBasePath + "/quest";
	g_setQuestObjectDir.clear();
	g_setQuestObjectDir.insert(g_stQuestDir + "/object");

	fprintf(stdout, "Setting Locale \"%s\" (Path: %s)\n", c_rstServiceName.c_str(), g_stServiceBasePath.c_str());
	return true;
}

void LocaleService_TransferDefaultSetting()
{
	if (!CTableBySkill::instance().Check())
		exit(1);
}



const std::string& LocaleService_GetBasePath()
{
	return g_stServiceBasePath;
}

const std::string& LocaleService_GetMapPath()
{
	return g_stServiceMapPath;
}

const std::string& LocaleService_GetQuestPath()
{
	return g_stQuestDir;
}
}
