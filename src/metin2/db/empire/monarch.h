#pragma once

#include <metin2/common/singleton.h>
#include <metin2/proto/tables.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

namespace metin2::db::inline empire {

class monarch : public singleton<monarch>
{
	public:
		typedef std::map<uint32_t, std::unique_ptr<monarch_election_info>> map_monarchelection;
		typedef std::vector<monarch_candidacy> vec_monarchcandidacy;

		monarch();
		~monarch() override;

		bool vote_monarch(uint32_t pid, uint32_t selectdpid);
		void elect_monarch();

		bool is_candidacy(uint32_t pid);
		bool add_candidacy(uint32_t pid, const char * name);
		bool del_candidacy(const char * name);

		bool load_monarch();
		bool set_monarch(const char * name);
		bool del_monarch(const char * name);

		bool is_monarch(int empire, uint32_t pid);
		bool add_money(int empire, int64_t money_value);
		bool dec_money(int empire, int64_t money_value);
		bool take_money(int empire, uint32_t pid, int64_t money_value);

		monarch_info* get_monarch()
		{
			return &monarch_info_;
		}

		vec_monarchcandidacy& vec_monarch_candidacy()
		{
			return vec_monarch_candidacy_;
		}

		size_t monarch_candidacy_size()
		{
			return vec_monarch_candidacy_.size();
		}

	private:
		int candidacy_index(uint32_t pid);

		map_monarchelection map_monarch_election_;
		vec_monarchcandidacy vec_monarch_candidacy_;
		monarch_info monarch_info_;

};
}
