#include <metin2/db/empire/monarch.h>

#include <metin2/db/storage/db_manager.h>
#include <metin2/db/app/main.h>

#include <metin2/proto/tables.h>
#include <metin2/common/utils.h>
#include <metin2/sql/async_sql.h>
#include <metin2/sql/query.h>
#include <metin2/config/config.h>
#include <metin2/log/log.h>

#include <mysql.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace metin2::db::inline empire {


monarch::monarch()
{
	memset(&monarch_info_, 0, sizeof(monarch_info));
}

monarch::~monarch()
{
}

bool monarch::vote_monarch(uint32_t pid, uint32_t selectdpid)
{
	map_monarchelection::iterator it = map_monarch_election_.find(pid);

	if (it == map_monarch_election_.end())
	{
		auto p = std::make_unique<monarch_election_info>();
		p->pid = pid;
		p->selectedpid= selectdpid;
		map_monarch_election_.insert(map_monarchelection::value_type(pid, std::move(p)));

		char szQuery[256];
		snprintf(szQuery, sizeof(szQuery),
				"INSERT INTO monarch_election(pid, selectedpid, electiondata) VALUES(%d, %d, now())", pid, selectdpid);

		db_manager::instance().async_query(szQuery);
		return 1;	
	}

	return 0;
}

void monarch::elect_monarch()
{
	int size = vec_monarch_candidacy().size();

	auto s = std::make_unique<int[]>(size);

	auto it = map_monarch_election_.begin();

	int idx = 0;

	for (; it != map_monarch_election_.end(); ++it)	
	{
		if ((idx =  candidacy_index(it->second->pid)) < 0)
			continue;

		++s[idx];

		if (metin2::config::test_server())
			metin2::log::info( "[MONARCH_VOTE] pid({:d}) come to vote candidacy pid({:d})", it->second->pid, vec_monarch_candidacy_[idx].pid);
	}
}

bool monarch::is_candidacy(uint32_t pid)
{
	vec_monarchcandidacy::iterator it = vec_monarch_candidacy_.begin();
	
	for (; it != vec_monarch_candidacy_.end(); ++it)
	{
		if (it->pid == pid)
			return false;
	}
			
	return true;
}

bool monarch::add_candidacy(uint32_t pid, const char * name)
{
	if (is_candidacy(pid) == false)
		return false;

	monarch_candidacy info;

	info.pid = pid;
	strlcpy(info.name, name, sizeof(info.name));
	vec_monarch_candidacy_.push_back(info);	

	char szQuery[256];
	snprintf(szQuery, sizeof(szQuery),
			"INSERT INTO monarch_candidacy(pid, date) VALUES(%d, now())", pid);

	db_manager::instance().async_query(szQuery);
	return true;
}

bool monarch::del_candidacy(const char * name)
{
	auto it = vec_monarch_candidacy_.begin();
	for (; it != vec_monarch_candidacy_.end(); ++it)
	{
		if (0 == strncmp(it->name, name, sizeof(it->name)))
		{
			char szQuery[256];
			snprintf(szQuery, sizeof(szQuery),
					"DELETE FROM monarch_candidacy WHERE pid=%d ", it->pid);
			db_manager::instance().async_query(szQuery);
		
			vec_monarch_candidacy_.erase (it);
			return true;
		}
	}
	return false;

}

bool monarch::is_monarch(int Empire, uint32_t pid)
{
	if (monarch_info_.pid[Empire] != pid)
		return false;
	return true;
}

bool monarch::add_money(int Empire, int64_t Money)
{
	if (monarch_info_.money[Empire] + Money > 2000000000)
		return true;

	monarch_info_.money[Empire] += Money;

	int64_t Money64 = monarch_info_.money[Empire];

	char szQuery[1024];	
	snprintf(szQuery, sizeof(szQuery), "UPDATE monarch set money=%ld where empire=%d", Money64, Empire);

	db_manager::instance().async_query(szQuery);

	return true;
}

bool monarch::dec_money(int Empire, int64_t Money)
{
	if (monarch_info_.money[Empire] - Money <  0)
		return false;
	monarch_info_.money[Empire] -= Money;

	int64_t Money64 = monarch_info_.money[Empire];

	char szQuery[1024];	
	snprintf(szQuery, sizeof(szQuery), "UPDATE monarch set money=%ld where empire=%d", Money64, Empire);

	db_manager::instance().async_query(szQuery);
	return true;
}

bool monarch::take_money(int Empire, uint32_t pid, int64_t Money)
{
	if (is_monarch(Empire, pid) == false)
		return false;

	if (monarch_info_.money[Empire] < Money)
		return false;
	
	monarch_info_.money[Empire] -= Money;

	char szQuery[1024];	
	snprintf(szQuery, sizeof(szQuery), 
			"UPDATE monarch SET money=%ld WHERE empire=%d", monarch_info_.money[Empire], Empire);

	db_manager::instance().async_query(szQuery);

	if (metin2::config::test_server())
		metin2::log::info( "[MONARCH] Take money empire({:d}) money({:d})", Empire, monarch_info_.money[Empire]);
	return true;
}

bool monarch::load_monarch()
{
	monarch_info * p = &monarch_info_;
    char szQuery[256];
	snprintf(szQuery, sizeof(szQuery), "SELECT empire, pid, name, money, windate FROM monarch a, player%s b where a.pid=b.id", get_table_postfix());
    std::unique_ptr<ql_msg> pMsg(db_manager::instance().direct_query(szQuery, SQL_PLAYER));

    if (pMsg->get()->num_rows == 0)
    {
        return false;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(pMsg->get()->sql_result)) != nullptr)
    {
        int idx = 0;
        int Empire = 0; str_to_number(Empire, row[idx++]);

        str_to_number(p->pid[Empire], row[idx++]);
		strlcpy(p->name[Empire], row[idx++], sizeof(p->name[Empire]));

        str_to_number(p->money[Empire], row[idx++]);
		strlcpy(p->date[Empire], row[idx++], sizeof(p->date[Empire]));

		if (metin2::config::test_server())
		metin2::log::info( "[LOAD_MONARCH] Empire {:d} pid {:d} money {:d} windate {}", Empire, p->pid[Empire], p->money[Empire], p->date[Empire]);
    }

    return true;
}

bool monarch::set_monarch(const char * name)
{
	monarch_info * p = &monarch_info_;
	std::string query = sql::format(
			"SELECT b.empire, a.id, a.name FROM player{} AS a, player_index{} AS b WHERE a.account_id=b.id AND a.name={}",
			sql::raw(get_table_postfix()), sql::raw(get_table_postfix()), std::string_view(name, strnlen(name, CHARACTER_NAME_MAX_LEN)));
    std::unique_ptr<ql_msg> pMsg(db_manager::instance().direct_query(query.c_str(), SQL_PLAYER));

    if (pMsg->get()->num_rows == 0)
    {
        return false;
    }

    MYSQL_ROW row;
	int Empire = 0;
    while ((row = mysql_fetch_row(pMsg->get()->sql_result)) != nullptr)
    {
        int idx = 0;
        str_to_number(Empire, row[idx++]);

        str_to_number(p->pid[Empire], row[idx++]);
		strlcpy(p->name[Empire], row[idx++], sizeof(p->name[Empire]));

		if (metin2::config::test_server())
			metin2::log::info( "[Set_MONARCH] Empire {:d} pid {:d} money {:d} windate {}", Empire, p->pid[Empire], p->money[Empire], p->date[Empire]);
    }

	//Insert into db
	query = sql::format(
					"REPLACE INTO monarch (empire, pid, windate, money) VALUES({}, {}, NOW(), {})", Empire, p->pid[Empire], p->money[Empire]);

 	db_manager::instance().async_query(query.c_str(), SQL_PLAYER);	
    return true;
}

bool monarch::del_monarch(const char * name)
{
	for (int n = 1; n < 4; ++n)
	{
		if (0 == strncmp(monarch_info_.name[n], name, sizeof(monarch_info_.name[n])))
		{
			char szQuery[256];

			int Empire = n;
			snprintf(szQuery, sizeof(szQuery), "DELETE FROM monarch WHERE empire=%d", Empire);
			std::unique_ptr<ql_msg> pMsg(db_manager::instance().direct_query(szQuery, SQL_PLAYER));

			if (pMsg->get()->affected_rows == 0 || pMsg->get()->affected_rows == (uint32_t)-1)
			{
				metin2::log::error(" DirectQuery failed({})", szQuery);
				return false;
			}

			memset(monarch_info_.name[Empire], 0, 32);
			monarch_info_.money[Empire] = 0;
			monarch_info_.pid[Empire] = 0;
			return true;
		}
	}
		
	return false;
}

int monarch::candidacy_index(uint32_t pid)
{
	auto it = vec_monarch_candidacy_.begin();

	for (int n = 0; it != vec_monarch_candidacy_.end(); ++it, ++n)
	{
		if (it->pid == pid)
			return n;		
	}

	return -1;
}
}
