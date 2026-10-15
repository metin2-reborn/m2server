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
static const std::string	service_base_path = "locale/english";
static const std::string	service_map_path = "locale/english/map";

std::string locale = "latin1";
std::string locale_filename = "locale/english/locale_string.txt";


static_assert(PLAYER_PER_ACCOUNT == 4);

static int check_name_independent(const char * str)
{
	if (banword_manager::instance().check_string(str, strlen(str)))
		return 0;

	// Cannot be made with a monster name.
	char szTmp[256];
	str_lower(str, szTmp, sizeof(szTmp));

	if (mob_manager::instance().get(szTmp, false))
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

void locale_service_load_locale_string_file()
{
	if (locale_filename.empty())
		return;

	if (metin2::config::is_auth())
		return;

	fprintf(stderr, "LocaleService %s\n", locale_filename.c_str());

	locale_init(locale_filename.c_str());
}

void locale_service_load_empire_text_convert_tables()
{
	char szFileName[256];

	for (int iEmpire = 1; iEmpire <= 3; ++iEmpire)
	{
		snprintf(szFileName, sizeof(szFileName), "%s/lang%d.cvt", locale_service_get_base_path().c_str(), iEmpire);
		metin2::log::info( "Load {}", szFileName);

		load_empire_text_convert_table(iEmpire, szFileName);
	}
}

bool locale_service_init(const std::string& rst_service_name)
{
	if (rst_service_name != "english")
	{
		metin2::log::error("unsupported locale {} (only english)", rst_service_name.c_str());
		return false;
	}

	quest_dir = service_base_path + "/quest";
	quest_object_dir.clear();
	quest_object_dir.insert(quest_dir + "/object");

	fprintf(stdout, "Setting Locale \"%s\" (Path: %s)\n", rst_service_name.c_str(), service_base_path.c_str());
	return true;
}

void locale_service_transfer_default_setting()
{
	if (!table_by_skill::instance().check())
		exit(1);
}



const std::string& locale_service_get_base_path()
{
	return service_base_path;
}

const std::string& locale_service_get_map_path()
{
	return service_map_path;
}

const std::string& locale_service_get_quest_path()
{
	return quest_dir;
}
}
