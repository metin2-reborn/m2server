// vim: ts=4 sw=4
#pragma once

#include <metin2/common/singleton.h>
#include <metin2/proto/tables.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

namespace metin2::db::inline empire {

class CMonarch : public singleton<CMonarch>
{
	public:
		typedef std::map<uint32_t, std::unique_ptr<MonarchElectionInfo>> MAP_MONARCHELECTION;
		typedef std::vector<MonarchCandidacy> VEC_MONARCHCANDIDACY;

		CMonarch();
		~CMonarch() override;

		bool VoteMonarch(uint32_t pid, uint32_t selectedpid);
		void ElectMonarch();

		bool IsCandidacy(uint32_t pid);
		bool AddCandidacy(uint32_t pid, const char * name);
		bool DelCandidacy(const char * name);

		bool LoadMonarch();
		bool SetMonarch(const char * name);
		bool DelMonarch(const char * name);

		bool IsMonarch(int Empire, uint32_t pid);
		bool AddMoney(int Empire, int64_t Money);
		bool DecMoney(int Empire, int64_t Money);
		bool TakeMoney(int Empire, uint32_t pid, int64_t Money);

		MonarchInfo* GetMonarch()
		{
			return &m_MonarchInfo;
		}

		VEC_MONARCHCANDIDACY& GetVecMonarchCandidacy()
		{
			return m_vec_MonarchCandidacy;
		}

		size_t MonarchCandidacySize()
		{
			return m_vec_MonarchCandidacy.size();
		}

	private:
		int GetCandidacyIndex(uint32_t pid);

		MAP_MONARCHELECTION m_map_MonarchElection;
		VEC_MONARCHCANDIDACY m_vec_MonarchCandidacy;
		MonarchInfo m_MonarchInfo;

};
}
