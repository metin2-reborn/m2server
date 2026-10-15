
#pragma once
#include <metin2/proto/length.h>
#include <metin2/common/singleton.h>

#include <memory>

namespace metin2::game::inline combat {

class table_by_skill : public singleton<table_by_skill>
{
	public:
		table_by_skill() = default;
		~table_by_skill() override = default;

		//Check table setup
		bool 	check() const;

		//Delete
		void 	delete_all();

		//Skill power table per skill level
		int 	skill_power_by_level_from_type( int job, int skillgroup, int skilllevel, bool bMob ) const;
		void 	set_skill_power_by_level_from_type( int idx, const int* aTable );
		void	delete_skill_power_by_level_from_type( int idx );

		//Additional skill damage table per level
		void 	delete_skill_damage_by_level_table();

	private:
		std::unique_ptr<int[]> skill_power_by_level_from_type_[JOB_MAX_NUM*2];
		std::unique_ptr<int[]> skill_damage_by_level_;
};
}
