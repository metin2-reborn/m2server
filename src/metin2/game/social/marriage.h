#pragma once

#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>

#include <metin2/common/singleton.h>

#include <cstdint>
#include <ctime>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>

namespace metin2::game::inline social {

namespace marriage
{
	struct wedding_info
	{
		uint32_t map_index;
	};

	extern const int MARRIAGE_POINT_PER_DAY;
	struct marriage
	{
		uint32_t pid1;
		uint32_t pid2;
		int   love_point;
		time_t marry_time;
		character * ch1;
		character * ch2;
		bool needs_save;
		bool is_married;
		std::string name1;
		std::string name2;

		std::unique_ptr<game::social::marriage::wedding_info> wedding_info;

		marriage(uint32_t pid1_value, uint32_t pid2_value, int _love_point, time_t _marry_time, const char* name1, const char* name2) : 
			pid1(pid1_value), 
			pid2(pid2_value), 
			love_point(_love_point),
			marry_time(_marry_time), 
			is_married(false),
			name1(name1),
			name2(name2),
			event_near_check(nullptr)
		{
			ch1 = ch2 = nullptr;
			needs_save = false;
			is_last_near = false;
			last_love_point = 0;
		}

		~marriage();

		void login(character * ch);
		void logout(uint32_t pid);

		bool is_online()
		{
			return ch1 && ch2;
		}

		bool is_near();

		uint32_t other(uint32_t PID) const
		{
			if (pid1 == PID)
				return pid2;

			if (pid2 == PID)
				return pid1;

			return 0;
		}

		int marriage_point();
		int marriage_grade();

		int bonus(uint32_t dwItemVnum, bool bShare = true, character * me = nullptr);

		void warp_to_wedding_map(uint32_t dwPID);
		void save();
		void set_married();

		void update(uint32_t point);
		void request_end_wedding();

		void start_near_check_event();
		void stop_near_check_event();
		void near_check();

		bool is_last_near;
		uint8_t last_love_point;
		event_ptr event_near_check;
	};

	class manager : public singleton<manager>
	{
		public:
			manager();
			~manager() override;

			bool	initialize();
			void	destroy();

			marriage*	get(uint32_t dwPlayerID);

			bool	is_marriage_unique_item(uint32_t dwItemVnum);

			bool	is_married(uint32_t dwPlayerID);
			bool	is_engaged(uint32_t dwPlayerID);
			bool	is_engaged_or_married(uint32_t dwPlayerID);

			void	request_add(uint32_t dwPID1, uint32_t dwPID2, const char* szName1, const char* szName2);
			void	add(uint32_t dwPID1, uint32_t dwPID2, time_t tMarryTime, const char* szName1, const char* szName2);

			void	request_update(uint32_t dwPID1, uint32_t dwPID2, int iUpdatePoint, uint8_t byMarried);
			void	update(uint32_t dwPID1, uint32_t dwPID2, int32_t lTotalPoint, uint8_t byMarried);

			void	request_remove(uint32_t dwPID1, uint32_t dwPID2);
			void	remove(uint32_t dwPID1, uint32_t dwPID2);

			void	login(character * ch);

			void	logout(uint32_t pid);
			void	logout(character * ch);

			void	wedding_ready(uint32_t dwPID1, uint32_t dwPID2, uint32_t dwMapIndex);
			void	wedding_start(uint32_t dwPID1, uint32_t dwPID2);
			void	wedding_end(uint32_t dwPID1, uint32_t dwPID2);

			void	request_end_wedding(uint32_t dwPID1, uint32_t dwPID2);

			template <typename Func>
				Func	for_each_wedding(Func f);

		private:
			std::unordered_set<marriage*> marriages_;
			std::map<uint32_t, marriage *> marriage_by_pid_;
			std::set<std::pair<uint32_t, uint32_t> > wedding_;
	};

	template <typename Func>
		Func manager::for_each_wedding(Func f)
		{
			for (auto it = wedding_.begin(); it!=wedding_.end(); ++it)
			{
				marriage* pMarriage = get(it->first);
				if (pMarriage)
					f(pMarriage);
			}
			return f;
		}

}
}
