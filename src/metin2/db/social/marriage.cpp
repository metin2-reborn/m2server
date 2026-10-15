#include <metin2/db/social/marriage.h>

#include <metin2/db/clients/client_manager.h>
#include <metin2/db/storage/db_manager.h>
#include <metin2/db/app/main.h>
#include <metin2/db/clients/peer.h>

#include <metin2/proto/tables.h>
#include <metin2/common/utils.h>
#include <metin2/sql/async_sql.h>
#include <metin2/log/log.h>

#include <mysql.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <utility>

#include <sys/types.h>

namespace metin2::db::inline social {

namespace marriage
{
	const uint32_t WEDDING_LENGTH = 60 * 60; // sec

	bool operator > (const wedding& lhs, const wedding& rhs)
	{
		return lhs.time > rhs.time;
	}

	bool operator > (const wedding_info &lhs, const wedding_info& rhs)
	{
		return lhs.time > rhs.time;
	}

	manager::manager()
	{
	}

	manager::~manager()
	{
	}

	bool manager::initialize()
	{
		char szQuery[1024];

		snprintf(szQuery, sizeof(szQuery),
				"SELECT pid1, pid2, love_point, time, is_married, p1.name, p2.name FROM marriage, player%s as p1, player%s as p2 WHERE p1.id = pid1 AND p2.id = pid2",
				get_table_postfix(), get_table_postfix());

		std::unique_ptr<ql_msg> pmsg_delete(db_manager::instance().direct_query("DELETE FROM marriage WHERE is_married = 0"));
		std::unique_ptr<ql_msg> pmsg(db_manager::instance().direct_query(szQuery));

		ql_result * pRes = pmsg->get();
		metin2::log::info( "MarriageList(size={:d})", pRes->num_rows);

		if (pRes->num_rows > 0)
		{
			for (uint uiRow = 0; uiRow != pRes->num_rows; ++uiRow)
			{
				MYSQL_ROW row = mysql_fetch_row(pRes->sql_result);

				uint32_t pid1 = 0; str_to_number(pid1, row[0]);
				uint32_t pid2 = 0; str_to_number(pid2, row[1]);
				int love_point = 0; str_to_number(love_point, row[2]);
				uint32_t time = 0; str_to_number(time, row[3]);
				uint8_t is_married = 0; str_to_number(is_married, row[4]);
				const char* name1 = row[5];
				const char* name2 = row[6];

				marriage* pMarriage = new marriage(pid1, pid2, love_point, time, is_married, name1, name2);
				marriages_.insert(pMarriage);
				marriage_by_pid_.insert(std::make_pair(pid1, pMarriage));
				marriage_by_pid_.insert(std::make_pair(pid2, pMarriage));

				metin2::log::info( "Marriage {:d}: LP:{:d} TM:{:d} ST:{:d} {:10d}:{:>16} {:10d}:{:>16} ", uiRow, love_point, time, is_married, pid1, name1, pid2, name2);
			}
		}
		return true;
	}

	marriage* manager::get(uint32_t dwPlayerID)
	{
		auto it = marriage_by_pid_.find(dwPlayerID);

		if (it != marriage_by_pid_.end())
			return it->second;

		return nullptr;
	}

	void align(uint32_t& rPID1, uint32_t& rPID2)
	{
		if (rPID1 > rPID2)
			std::swap(rPID1, rPID2);
	}

	void manager::add(uint32_t dwPID1, uint32_t dwPID2, const char* szName1, const char* szName2)
	{
		uint32_t now = client_manager::instance().current_time();
		if (is_married(dwPID1) || is_married(dwPID2))
		{
			metin2::log::error("cannot marry already married character. {:d} - {:d}", dwPID1, dwPID2);
			return;
		}

		align(dwPID1, dwPID2);

		char szQuery[512];
		snprintf(szQuery, sizeof(szQuery), "INSERT INTO marriage(pid1, pid2, love_point, time) VALUES (%u, %u, 0, %u)", dwPID1, dwPID2, now);

		std::unique_ptr<ql_msg> pmsg(db_manager::instance().direct_query(szQuery));

		ql_result* res = pmsg->get();
		if (res->affected_rows == 0 || res->affected_rows == (uint32_t)-1)
		{
			metin2::log::error("cannot insert marriage");
			return;
		}

		metin2::log::info( "MARRIAGE ADD {:d} {:d}", dwPID1, dwPID2);

		marriage* pMarriage = new marriage(dwPID1, dwPID2, 0, now, 0, szName1, szName2);
		marriages_.insert(pMarriage);
		marriage_by_pid_.insert(std::make_pair(dwPID1, pMarriage));
		marriage_by_pid_.insert(std::make_pair(dwPID2, pMarriage));

		packet_marriage_add p;
		p.pi_d1 = dwPID1;
		p.pi_d2 = dwPID2;
		p.t_marry_time = now;
		strlcpy(p.name1, szName1, sizeof(p.name1));
		strlcpy(p.name2, szName2, sizeof(p.name2));
		client_manager::instance().forward_packet(HEADER_DG_MARRIAGE_ADD, &p, sizeof(p));
	}

	void manager::update(uint32_t dwPID1, uint32_t dwPID2, int32_t iLovePoint, uint8_t byMarried)
	{
		marriage* pMarriage = get(dwPID1);
		if (!pMarriage || pMarriage->other(dwPID1) != dwPID2)
		{
			metin2::log::error("not under marriage : {:d} {:d}", dwPID1, dwPID2);
			return;
		}

		align(dwPID1, dwPID2);

		char szQuery[512];
		snprintf(szQuery, sizeof(szQuery), "UPDATE marriage SET love_point = %d, is_married = %d WHERE pid1 = %u AND pid2 = %u", 
				iLovePoint, byMarried, pMarriage->pid1, pMarriage->pid2);

		std::unique_ptr<ql_msg> pmsg(db_manager::instance().direct_query(szQuery));

		ql_result* res = pmsg->get();
		if (res->affected_rows == 0 || res->affected_rows == (uint32_t)-1)
		{
			metin2::log::error("cannot update marriage : PID:{:d} {:d}", dwPID1, dwPID2);
			return;
		}

		metin2::log::info( "MARRIAGE UPDATE PID:{:d} {:d} LP:{:d} ST:{:d}", dwPID1, dwPID2, iLovePoint, byMarried);
		pMarriage->love_point = iLovePoint;
		pMarriage->is_married = byMarried;

		packet_marriage_update p;
		p.pi_d1 = dwPID1;
		p.pi_d2 = dwPID2;
		p.love_point = pMarriage->love_point;
		p.married = pMarriage->is_married;
		client_manager::instance().forward_packet(HEADER_DG_MARRIAGE_UPDATE, &p, sizeof(p));
	}

	void manager::remove(uint32_t dwPID1, uint32_t dwPID2)
	{
		marriage* pMarriage = get(dwPID1);

		if (pMarriage)
		{
			metin2::log::info( "Break Marriage pid1 {:d} pid2 {:d} Other {:d}", dwPID1, dwPID2, pMarriage->other(dwPID1));
		}
		if (!pMarriage || pMarriage->other(dwPID1) != dwPID2)
		{
			auto it = marriage_by_pid_.begin();

			for (; it != marriage_by_pid_.end(); ++it)
			{
				metin2::log::info( "Marriage List pid1 {:d} pid2 {:d}", it->second->pid1, it->second->pid2);
			}
			metin2::log::error("not under marriage : PID:{:d} {:d}", dwPID1, dwPID2);
			return;
		}

		align(dwPID1, dwPID2);

		char szQuery[512];
		snprintf(szQuery, sizeof(szQuery), "DELETE FROM marriage WHERE pid1 = %u AND pid2 = %u", dwPID1, dwPID2);

		std::unique_ptr<ql_msg> pmsg(db_manager::instance().direct_query(szQuery));

		ql_result* res = pmsg->get();
		if (res->affected_rows == 0 || res->affected_rows == (uint32_t)-1)
		{
			metin2::log::error("cannot delete marriage : PID:{:d} {:d}", dwPID1, dwPID2);
			return;
		}

		metin2::log::info( "MARRIAGE REMOVE PID:{:d} {:d}", dwPID1, dwPID2);

		marriages_.erase(pMarriage);
		marriage_by_pid_.erase(dwPID1);
		marriage_by_pid_.erase(dwPID2);

		packet_marriage_remove p;
		p.pi_d1 = dwPID1;
		p.pi_d2 = dwPID2;
		client_manager::instance().forward_packet(HEADER_DG_MARRIAGE_REMOVE, &p, sizeof(p));

		delete pMarriage;
	}

	void manager::on_setup(peer* peer)
	{
		// Send married couples
		for (auto it = marriages_.begin(); it != marriages_.end(); ++it)
		{
			marriage* pMarriage = *it;

			{
				packet_marriage_add p;
				p.pi_d1 = pMarriage->pid1;
				p.pi_d2 = pMarriage->pid2;
				p.t_marry_time = pMarriage->time;
				strlcpy(p.name1, pMarriage->name1.c_str(), sizeof(p.name1));
				strlcpy(p.name2, pMarriage->name2.c_str(), sizeof(p.name2));
				peer->encode_header(HEADER_DG_MARRIAGE_ADD, 0, sizeof(p));
				peer->encode(&p, sizeof(p));
			}

			{
				packet_marriage_update p;
				p.pi_d1 = pMarriage->pid1;
				p.pi_d2 = pMarriage->pid2;
				p.love_point = pMarriage->love_point;
				p.married	= pMarriage->is_married;
				peer->encode_header(HEADER_DG_MARRIAGE_UPDATE, 0, sizeof(p));
				peer->encode(&p, sizeof(p));
			}
		}

		// Send weddings
		for (auto it = running_wedding_.begin(); it != running_wedding_.end(); ++it)
		{
			const wedding& t = it->second;

			packet_wedding_ready p;
			p.pi_d1 = t.pi_d1;
			p.pi_d2 = t.pi_d2;
			p.map_index = t.map_index;

			peer->encode_header(HEADER_DG_WEDDING_READY, 0, sizeof(p));
			peer->encode(&p, sizeof(p));

			packet_wedding_start p2;
			p2.pi_d1 = t.pi_d1;
			p2.pi_d2 = t.pi_d2;

			peer->encode_header(HEADER_DG_WEDDING_START, 0, sizeof(p2));
			peer->encode(&p2, sizeof(p2));
		}
	}

	void manager::ready_wedding(uint32_t dwMapIndex, uint32_t dwPID1, uint32_t dwPID2)
	{
		uint32_t dwStartTime = client_manager::instance().current_time();
		pq_wedding_start_.push(wedding(dwStartTime + 5, dwMapIndex, dwPID1, dwPID2));
	}

	void manager::end_wedding(uint32_t dwPID1, uint32_t dwPID2)
	{
		auto it = running_wedding_.find(std::make_pair(dwPID1, dwPID2));
		if (it == running_wedding_.end())
		{
			metin2::log::error("try to end wedding {:d} {:d}", dwPID1, dwPID2);
			return;
		}

		wedding& w = it->second;

		packet_wedding_end p;
		p.pi_d1 = w.pi_d1;
		p.pi_d2 = w.pi_d2;
		client_manager::instance().forward_packet(HEADER_DG_WEDDING_END, &p, sizeof(p));
		running_wedding_.erase(it);
	}

	void manager::update()
	{
		uint32_t now = client_manager::instance().current_time();

		if (!pq_wedding_end_.empty())
		{
			while (!pq_wedding_end_.empty() && pq_wedding_end_.top().time <= now)
			{
				wedding_info wi = pq_wedding_end_.top();
				pq_wedding_end_.pop();

				auto it = running_wedding_.find(std::make_pair(wi.pi_d1, wi.pi_d2));
				if (it == running_wedding_.end())
					continue;

				wedding& w = it->second;

				packet_wedding_end p;
				p.pi_d1 = w.pi_d1;
				p.pi_d2 = w.pi_d2;
				client_manager::instance().forward_packet(HEADER_DG_WEDDING_END, &p, sizeof(p));
				running_wedding_.erase(it);

				auto it_marriage = marriage_by_pid_.find(w.pi_d1);

				if (it_marriage != marriage_by_pid_.end())
				{
					marriage* pMarriage = it_marriage->second;
					if (!pMarriage->is_married)
					{
						remove(pMarriage->pid1, pMarriage->pid2);
					}
				}
			}
		}
		if (!pq_wedding_start_.empty())
		{
			while (!pq_wedding_start_.empty() && pq_wedding_start_.top().time <= now)
			{
				wedding w = pq_wedding_start_.top();
				pq_wedding_start_.pop();

				packet_wedding_start p;
				p.pi_d1 = w.pi_d1;
				p.pi_d2 = w.pi_d2;
				client_manager::instance().forward_packet(HEADER_DG_WEDDING_START, &p, sizeof(p));

				w.time += WEDDING_LENGTH;
				pq_wedding_end_.push(wedding_info(w.time, w.pi_d1, w.pi_d2));
				running_wedding_.insert(make_pair(std::make_pair(w.pi_d1, w.pi_d2), w));
			}
		}
	}
}
}
