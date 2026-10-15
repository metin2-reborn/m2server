#include <metin2/game/events/SpeedServer.h>

#include <metin2/game/text/locale_service.h>
#include <metin2/game/app/utils.h>

#include <metin2/proto/length.h>
#include <metin2/common/utils.h>
#include <metin2/log/log.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <list>
#include <map>
#include <string>
#include <utility>

namespace metin2::game::inline events {

// Speed server bonus experience system
// by rtsummit

CSpeedServerManager::CSpeedServerManager()
{
}

CSpeedServerManager::~CSpeedServerManager()
{
}

CSpeedServerEmpireExp::CSpeedServerEmpireExp()
{
}

CSpeedServerEmpireExp::~CSpeedServerEmpireExp()
{
}

bool CSpeedServerManager::Initialize()
{
	for (int i = 1; i < EMPIRE_MAX_NUM; i++)
	{
		metin2::log::info("speed manager init");
		if(!Empire[i].Initialize (i))
		{
			metin2::log::error("EMPIRE {:d} Exp Bonus Manager Init fail",i);
			return false;
		}
	}
	return true;
}

bool CSpeedServerEmpireExp::Initialize (uint8_t e)
{
	empire = e;
	metin2::log::info( "empire exp init {:d}", empire);
	file_name = LocaleService_GetBasePath() + "/exp_bonus_table_" + std::to_string(empire) + ".txt";

	for (int i = 1; i < 6; i++)
	{
		wday_exp_table[i].push_back (HME (18, 0, 50));
		wday_exp_table[i].push_back (HME (24, 0, 100));
	}

	wday_exp_table[0].push_back (HME (18, 0, 100));
	wday_exp_table[0].push_back (HME (24, 0, 150));
	wday_exp_table[6].push_back (HME (18, 0, 100));
	wday_exp_table[6].push_back (HME (24, 0, 150));

	LoadExpTable();
	return true;
}

bool CSpeedServerEmpireExp::LoadWdayExpTable(int wday, char *str)
{
	std::list <HME> &lst = wday_exp_table[wday];
	lst.clear();
	char *p, *n;
	const char	*delim = " \t\r\n";
	char *t;
	char *h, *m, *e;
	int hour, min, exp;
	metin2::log::info( "str {}", str);
	strtok (str, delim);
	p = strtok (nullptr, ";");
	n = strtok (nullptr, ";");
	while (p != nullptr)
	{
		t = strtok (p, delim);
		e = strtok (nullptr, delim);
		h = strtok (t, ":");
		m = strtok (nullptr, delim);
		if (!str_to_number (hour, h) || !str_to_number (min, m) || !str_to_number (exp, e))
		{
			metin2::log::info( "h m e : {} {} {}",h, m, e);
			metin2::log::error("Invalid argument. Please insert hh:mm exp");
			return false;
		}
			metin2::log::info( "h m e : {} {} {}",h, m, e);
		
		lst.push_back (HME (hour, min, exp));
		p = strtok (n, ";");
		n = strtok (nullptr, ";");
	}
	return true;
}

bool CSpeedServerEmpireExp::WriteExpTable()
{
	FILE *fp;

	metin2::log::info( "write");
	if (file_name.empty())
		return false;

	if ((fp = fopen(file_name.c_str(), "w"))==nullptr)
	{
		return false;
	}

	char wday_name[7][4] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
	for (int i = 0; i < 7; i++)
	{
		fprintf (fp, "%s", wday_name[i]);
		for (std::list <HME>::iterator it = wday_exp_table[i].begin(); it != wday_exp_table[i].end(); it++)
		{
			fprintf (fp, " %d:%d %d;", it->hour, it->min, it->exp);
		}
		fprintf(fp, "\n");
	}
	
	for (std::map <Date, std::list <HME> >::iterator holi_it = holiday_map.begin(); holi_it != holiday_map.end(); holi_it++)
	{
		fprintf (fp, "HOLIDAY %d.%d.%d", holi_it->first.year + 1900, holi_it->first.mon + 1, holi_it->first.day);
		for (std::list <HME>::iterator it = holi_it->second.begin(); it != holi_it->second.end(); it++)
		{
			fprintf (fp, " %d:%d %d;", it->hour, it->min, it->exp);
		}
		fprintf(fp, "\n");
	}
	fclose (fp);
	return true;
}

bool CSpeedServerEmpireExp::LoadExpTable()
{
	FILE	*fp;
	char	one_line[256];
	char	temp[256];
	const char	*delim = " \t\r\n";

	metin2::log::info( "load");
	if (file_name.empty())
		return false;

	if ((fp = fopen(file_name.c_str(), "r"))==nullptr)
		return false;

	while (fgets(one_line, 256, fp))
	{
		if (one_line[0]=='#')
			continue;

		strlcpy(temp, one_line, sizeof(temp));

		const char* token_string = strtok(one_line, delim);

		if (nullptr==token_string)
			continue;

		TOKEN("SUN")
		{
			LoadWdayExpTable (0, temp);
		}
		else TOKEN("MON")
		{
			LoadWdayExpTable (1, temp);
		}
		else TOKEN("TUE")
		{
			LoadWdayExpTable (2, temp);
		}
		else TOKEN("WED")
		{
			LoadWdayExpTable (3, temp);
		}
		else TOKEN("THU")
		{
			LoadWdayExpTable (4, temp);
		}
		else TOKEN("FRI")
		{
			LoadWdayExpTable (5, temp);
		}
		else TOKEN("SAT")
		{
			LoadWdayExpTable (6, temp);
		}
		else TOKEN("HOLIDAY")
		{
			std::list <HME> lst;
			lst.clear();
			char *p, *n;
			char *t, *v;
			char *h, *m, *e;
			int hour, min, exp;

			v = strtok (temp, delim);
			v = strtok (nullptr, delim);
			metin2::log::info( "holiday {}", v);

			p = strtok (nullptr, ";");
			n = strtok (nullptr, ";");
			while (p != nullptr)
			{
				t = strtok (p, delim);
				e = strtok (nullptr, delim);
				h = strtok (t, ":");
				m = strtok (nullptr, delim);
				if (!str_to_number (hour, h) || !str_to_number (min, m) || !str_to_number (exp, e))
				{
					metin2::log::info( "h m e : {} {} {}",h, m, e);
					metin2::log::error("Invalid argument. Please insert hh:mm exp");
					return false;
				}
				metin2::log::info( "h m e : {} {} {}",h, m, e);

				lst.push_back (HME (hour, min, exp));
				p = strtok (n, ";");
				n = strtok (nullptr, ";");
			}
			int year, mon, day;
			if (!str_to_number (year, strtok (v, "."))
					|| !str_to_number ( mon, strtok (nullptr, "."))
					|| !str_to_number ( day, strtok (nullptr, ".")))
			{
				metin2::log::error("Invalid Date");
				return false;
			}

			metin2::log::info( "y m d {:d} {:d} {:d}",year, mon, day);

			holiday_map.insert (std::pair <Date, std::list <HME> > (Date (year - 1900, mon - 1, day), lst));
		}
	}

	fclose(fp);

	return true;
}

std::list <HME>& CSpeedServerManager::GetWdayExpTableOfEmpire(uint8_t empire, int wday)
{
	return Empire[empire].GetWdayExpTable(wday);
}

std::list <HME>& CSpeedServerEmpireExp::GetWdayExpTable(int wday)
{
	return wday_exp_table[wday];
}

void CSpeedServerManager::SetWdayExpTableOfEmpire (uint8_t empire, int wday, HME hme)
{
	Empire[empire].SetWdayExpTable (wday, hme);
}

void CSpeedServerEmpireExp::SetWdayExpTable (int wday, HME hme)
{
	wday_exp_table[wday].push_back (hme);
	WriteExpTable();
}

void CSpeedServerManager::InitWdayExpTableOfEmpire (uint8_t empire, int wday)
{
	if (empire > EMPIRE_MAX_NUM)
	{
		metin2::log::error("invalid empire");
		return;
	}

	Empire[empire].InitWdayExpTable (wday);
}

void CSpeedServerEmpireExp::InitWdayExpTable(int wday)
{
	wday_exp_table[wday].clear();
}


std::list <HME>& CSpeedServerManager::GetHolidayExpTableOfEmpire(uint8_t empire, Date date, bool &is_exist)
{
	return Empire[empire].GetHolidayExpTable(date, is_exist);
}

std::list <HME>& CSpeedServerEmpireExp::GetHolidayExpTable(Date date, bool &is_exist)
{
	std::map <Date, std::list <HME> >::iterator it = holiday_map.find(date);
	if (it != holiday_map.end())
	{
		is_exist = true;
		return it->second;
	}
	else
	{
		is_exist = false;
		metin2::log::error("Cannot find Holiday {:d} {:d} {:d}",date.year, date.mon, date.day);
	}
	return it->second;
}

void CSpeedServerManager::SetHolidayExpTableOfEmpire (uint8_t empire, Date date, HME hme)
{
	Empire[empire].SetHolidayExpTable (date, hme);
}

void CSpeedServerEmpireExp::SetHolidayExpTable (Date date, HME hme)
{
	std::map <Date, std::list <HME> >::iterator it = holiday_map.find(date);
	if (it != holiday_map.end())
	{
		it->second.push_back (hme);
	}
	WriteExpTable();
}

void CSpeedServerManager::InitHolidayExpTableOfEmpire (uint8_t empire, Date date)
{
	if (empire > EMPIRE_MAX_NUM)
	{
		metin2::log::error("invalid empire");
		return;
	}

	Empire[empire].InitHolidayExpTable (date);
}

void CSpeedServerEmpireExp::InitHolidayExpTable(Date date)
{
	metin2::log::info( "init holiday");
	std::map <Date, std::list <HME> >::iterator it = holiday_map.find(date);
	if (it == holiday_map.end())
	{
		std::list <HME> lst;
		holiday_map.insert (std::pair <Date, std::list <HME> > (date, lst));
	}
	else
	{
		it->second.clear();
	}
}

HME CSpeedServerManager::GetCurrentExpPrivOfEmpire (uint8_t empire, int &duration, bool &is_change)
{
	return Empire[empire].GetCurrentExpPriv (duration, is_change);
}

HME CSpeedServerEmpireExp::GetCurrentExpPriv(int &duration, bool &is_change)
{
	struct tm* datetime;
	time_t t;
	t = time(nullptr);
	datetime = localtime(&t);

	Date date (datetime -> tm_year, datetime -> tm_mon, 
			datetime -> tm_mday);

	std::map <Date, std::list <HME> >::iterator holi_it = holiday_map.find(date);

	int total_sec = datetime->tm_hour * 3600 + datetime->tm_min * 60 + datetime->tm_sec;

	HME hme;

	// If today is a holiday, apply the holiday bonus.
	if (holi_it != holiday_map.end())
	{
		for (std::list <HME>::iterator it = holi_it->second.begin();
				it != wday_exp_table[datetime->tm_wday].end(); it++)
		{
			// If the current time falls within the time range,
			if (total_sec < (it->hour * 3600 + it->min * 60 ))
			{
				hme = *it;
				break;
			}
		}
	}
	else
	{
		for (std::list <HME>::iterator it =  wday_exp_table[datetime->tm_wday].begin();
				it != wday_exp_table[datetime->tm_wday].end(); it++)
		{
			// If the current time falls within the time range,
			if (total_sec < (it->hour * 3600 + it->min * 60 ))
			{
				hme = *it;
				break;
			}
		}
	}

	duration = hme.hour * 3600 + hme.min * 60 - total_sec;
	
	is_change = !(hme == current_hme);
	current_hme = hme;

	return hme;

}
}
