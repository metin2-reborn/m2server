
#ifndef METIN2_TABLE_BY_SKILL_
#define METIN2_TABLE_BY_SKILL_
#include <common/length.h>
#include <common/singleton.h>

class CTableBySkill : public singleton<CTableBySkill>
{
	public:
		CTableBySkill()
			: m_aiSkillDamageByLevel(nullptr)
		{
			//Initialize additional damage per skill level
			for ( int job = 0; job < JOB_MAX_NUM * 2; ++job )
				m_aiSkillPowerByLevelFromType[job] = nullptr;
		}

		~CTableBySkill() override
		{
			DeleteAll();
		}

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
		int * m_aiSkillPowerByLevelFromType[JOB_MAX_NUM*2];
		int * m_aiSkillDamageByLevel;
};

#endif /* METIN2_TABLE_BY_SKILL_ */

