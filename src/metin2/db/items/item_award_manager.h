// vim:ts=8 sw=4
#pragma once
#include <metin2/proto/length.h>
#include <metin2/common/singleton.h>
#include <metin2/sql/async_sql.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>

namespace metin2::db::inline items {

struct item_award {
    uint32_t	id;
    char	login[LOGIN_MAX_LEN+1];
    uint32_t	vnum;
    uint32_t	count;
    uint32_t	socket0;
    uint32_t	socket1;
    uint32_t	socket2;
    char	why[ITEM_AWARD_WHY_MAX_LEN+1];
    bool	taken;
    bool	mall;
};

class item_award_manager : public singleton<item_award_manager>
{
    public:
	item_award_manager();
	~item_award_manager() override;

	void				request_load();
	void				load(ql_msg * msg);
	std::set<item_award *> *	by_login(const char * login);

	void				taken(uint32_t award_id, uint32_t item_id);
	// gift notify
	std::map<uint32_t, item_award *>& map_award();
	std::map<std::string, std::set<item_award *> >& mapk_set_award_by_login();
    private:
	// ID, ItemAward pair
	std::map<uint32_t, item_award *>			map_award_;
	// PID, ItemAward pair
	std::map<std::string, std::set<item_award *> >	map_k_set_award_by_login_;
};
}
