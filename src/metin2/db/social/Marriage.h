// vim: ts=4 sw=4
#pragma once

#include <metin2/db/clients/Peer.h>

#include <metin2/common/singleton.h>

#include <cstdint>
#include <functional>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace metin2::db::inline social {

namespace marriage
{
	struct wedding_info
	{
		uint32_t time;
		uint32_t pi_d1;
		uint32_t pi_d2;
		wedding_info(uint32_t time_value, uint32_t pid1, uint32_t pid2)
			: time(time_value),
			pi_d1(pid1),
			pi_d2(pid2)
		{
		}
	};

	struct wedding
	{
		uint32_t time;
		uint32_t map_index;
		uint32_t pi_d1;
		uint32_t pi_d2;

		wedding(uint32_t time_value, uint32_t dwMapIndex, uint32_t pid1, uint32_t pid2)
			: time(time_value),
			map_index(dwMapIndex),
			pi_d1(pid1),
			pi_d2(pid2)
		{
		}
	};

	extern bool operator < (const wedding& lhs, const wedding& rhs);
	extern bool operator > (const wedding& lhs, const wedding& rhs);
	extern bool operator > (const wedding_info& lhs, const wedding_info& rhs);

	struct marriage
	{
		uint32_t pid1;
		uint32_t pid2;
		int   love_point;
		uint32_t time;
		uint8_t is_married; // false : engaged, true : married
		std::string name1;
		std::string name2;

		marriage(uint32_t _pid1, uint32_t _pid2, int _love_point, uint32_t _time, uint8_t _is_married, const char* name1, const char* name2)
			: pid1(_pid1), pid2(_pid2), love_point(_love_point), time(_time), is_married(_is_married), name1(name1), name2(name2)
		{
		}

		uint32_t other(uint32_t PID)
		{
			if (pid1 == PID)
				return pid2;

			if (pid2 == PID)
				return pid1;

			return 0;
		}
	};

	class manager : public singleton<manager>
	{
		public:
			manager();
			~manager() override;

			bool initialize();

			marriage* get(uint32_t dwPlayerID);
			bool is_married(uint32_t dwPlayerID)
			{
				return get(dwPlayerID) != nullptr;
			}

			void	add(uint32_t dwPID1, uint32_t dwPID2, const char* szName1, const char* szName2);
			void	remove(uint32_t dwPID1, uint32_t dwPID2);
			void	update(uint32_t dwPID1, uint32_t dwPID2, int32_t iLovePoint, uint8_t byMarried);

			void	ready_wedding(uint32_t dwMapIndex, uint32_t dwPID1, uint32_t dwPID2);
			void	end_wedding(uint32_t dwPID1, uint32_t dwPID2);

			void	on_setup(peer* peer);

			void	update();

		private:
			std::set<marriage *> marriages_;
			std::map<uint32_t, marriage *> marriage_by_pid_;

			std::priority_queue<wedding, std::vector<wedding>, std::greater<wedding> > pq_wedding_start_;

			std::priority_queue<wedding_info, std::vector<wedding_info>, std::greater<wedding_info> > pq_wedding_end_;

			std::map<std::pair<uint32_t, uint32_t>, wedding> running_wedding_;
	};
}
}
