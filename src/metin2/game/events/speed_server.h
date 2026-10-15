#pragma once

#include <metin2/proto/length.h>
#include <metin2/common/singleton.h>

#include <cstdint>
#include <list>
#include <map>
#include <string>

namespace metin2::game::inline events {


class hme
{
	public :
		int hour;
		int min;
		int exp;

		hme (int h=0, int m=0, int e=0){
			hour =  h; min = m; 
			exp = e;
		}

		bool operator==(const hme &rhs) const
		{
			return hour == rhs.hour
				&& min == rhs.min
				&& exp == rhs.exp;
		}

};

class date
{
	public : 
		int year;
		int mon;
		int day;

		date (int y = 0, int m = 0, int d = 0)
		{
			year = y; mon = m; day = d;
		}

		bool operator<(const date &rhs) const
		{
			return (year<rhs.year)
				|| ((year==rhs.year) && (mon<rhs.mon))
				|| ((year==rhs.year) && (mon==rhs.mon) && (day<rhs.day));
		}
};

class speed_server_empire_exp
{
	public :
		speed_server_empire_exp();
		~speed_server_empire_exp();

		bool initialize (uint8_t e);
		
		std::list <hme>& wday_exp_table(int wday);
		void set_wday_exp_table(int wday, hme hme);

		std::list <hme>& holiday_exp_table(date date, bool &is_exist);
		void set_holiday_exp_table(date date, hme hme);

		void init_wday_exp_table(int wday);
		void init_holiday_exp_table(date date);
		hme current_exp_priv (int &duration, bool &is_change);

		bool write_exp_table();

	private :
		bool load_exp_table ();
		bool load_wday_exp_table (int wday, char *str);

		uint8_t empire_;
		std::string file_name_;
		hme current_hme_;
		std::map <date, std::list <hme> > holiday_map_;
		std::list <hme> wday_exp_table_[7];
};

class speed_server_manager : public singleton<speed_server_manager>
{
	public:
		speed_server_manager();
		~speed_server_manager() override;

		bool initialize ();

		std::list <hme>& wday_exp_table_of_empire (uint8_t empire, int wday);
		void set_wday_exp_table_of_empire (uint8_t empire, int wday, hme hme);
		void init_wday_exp_table_of_empire (uint8_t empire, int wday);
		
		std::list <hme>& holiday_exp_table_of_empire (uint8_t empire, date date, bool &is_exist);
		void set_holiday_exp_table_of_empire (uint8_t empire, date date, hme hme);
		void init_holiday_exp_table_of_empire (uint8_t empire, date date);

		
		hme current_exp_priv_of_empire (uint8_t empire, int &duration, bool &is_change);

	private:
		speed_server_empire_exp empire_[EMPIRE_MAX_NUM];
		
};
}
