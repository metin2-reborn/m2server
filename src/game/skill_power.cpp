

#include "skill_power.h"

#include <common/length.h>

#include <cstdio>
#include <cstring>
#include <memory>
#include <utility>

bool CTableBySkill::Check() const
{
	for (int job = 0; job < (JOB_MAX_NUM * 2); ++job)
	{
		if (!m_aiSkillPowerByLevelFromType[job])
		{
			fprintf( stderr, "[NO SETTING SKILL] aiSkillPowerByLevelFromType[%d]", job);
			return false;
		}
	}

	return true;
}

void CTableBySkill::DeleteAll()
{
	for ( int job = 0; job < JOB_MAX_NUM * 2; ++job )
	{
		DeleteSkillPowerByLevelFromType( job );
	}

	DeleteSkillDamageByLevelTable();
}

int CTableBySkill::GetSkillPowerByLevelFromType(int job, int skillgroup, int skilllevel, bool bMob) const
{
	if (bMob)
	{
		return m_aiSkillPowerByLevelFromType[0][skilllevel];
	}

	if (job >= JOB_MAX_NUM || skillgroup == 0)
		return 0;

	int idx = (job * 2) + (skillgroup - 1);

	return m_aiSkillPowerByLevelFromType[idx][skilllevel];
}

void CTableBySkill::SetSkillPowerByLevelFromType(int idx, const int* aTable)
{
	DeleteSkillPowerByLevelFromType(idx);

	auto aiSkillTable = std::make_unique_for_overwrite<int[]>(SKILL_MAX_LEVEL+1);

	memcpy (aiSkillTable.get(), aTable, sizeof(int) * (SKILL_MAX_LEVEL + 1));
	m_aiSkillPowerByLevelFromType[idx] = std::move(aiSkillTable);
}

void CTableBySkill::DeleteSkillPowerByLevelFromType(int idx)
{
	m_aiSkillPowerByLevelFromType[idx].reset();
}

void CTableBySkill::DeleteSkillDamageByLevelTable()
{
	m_aiSkillDamageByLevel.reset();
}

