

#include <metin2/game/combat/skill_power.h>

#include <metin2/proto/length.h>

#include <cstdio>
#include <cstring>
#include <memory>
#include <utility>

namespace metin2::game::inline combat {

bool table_by_skill::check() const
{
	for (int job = 0; job < (JOB_MAX_NUM * 2); ++job)
	{
		if (!skill_power_by_level_from_type_[job])
		{
			fprintf( stderr, "[NO SETTING SKILL] aiSkillPowerByLevelFromType[%d]", job);
			return false;
		}
	}

	return true;
}

void table_by_skill::delete_all()
{
	for ( int job = 0; job < JOB_MAX_NUM * 2; ++job )
	{
		delete_skill_power_by_level_from_type( job );
	}

	delete_skill_damage_by_level_table();
}

int table_by_skill::skill_power_by_level_from_type(int job, int skillgroup, int skilllevel, bool mob) const
{
	if (mob)
	{
		return skill_power_by_level_from_type_[0][skilllevel];
	}

	if (job >= JOB_MAX_NUM || skillgroup == 0)
		return 0;

	int idx = (job * 2) + (skillgroup - 1);

	return skill_power_by_level_from_type_[idx][skilllevel];
}

void table_by_skill::set_skill_power_by_level_from_type(int idx, const int* table)
{
	delete_skill_power_by_level_from_type(idx);

	auto aiSkillTable = std::make_unique_for_overwrite<int[]>(SKILL_MAX_LEVEL+1);

	memcpy (aiSkillTable.get(), table, sizeof(int) * (SKILL_MAX_LEVEL + 1));
	skill_power_by_level_from_type_[idx] = std::move(aiSkillTable);
}

void table_by_skill::delete_skill_power_by_level_from_type(int idx)
{
	skill_power_by_level_from_type_[idx].reset();
}

void table_by_skill::delete_skill_damage_by_level_table()
{
	skill_damage_by_level_.reset();
}
}
