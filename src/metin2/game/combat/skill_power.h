
#pragma once
#include <metin2/proto/length.h>
#include <metin2/common/singleton.h>

#include <memory>

namespace metin2::game::inline combat {

class CTableBySkill : public singleton<CTableBySkill>
{
	public:
		CTableBySkill() = default;
		~CTableBySkill() override = default;

		//Check table setup
		bool 	Check() const;

		//Delete
		void 	DeleteAll();

		//Skill power table per skill level
		int 	GetSkillPowerByLevelFromType( int job, int skillgroup, int skilllevel, bool bMob ) const;
		void 	SetSkillPowerByLevelFromType( int idx, const int* aTable );
		void	DeleteSkillPowerByLevelFromType( int idx );

		//Additional skill damage table per level
		void 	DeleteSkillDamageByLevelTable();

	private:
		std::unique_ptr<int[]> m_aiSkillPowerByLevelFromType[JOB_MAX_NUM*2];
		std::unique_ptr<int[]> m_aiSkillDamageByLevel;
};
}
