#pragma once

#include <metin2/game/world/vector.h>

#include <metin2/common/singleton.h>

#include <cstdint>
#include <map>
#include <memory>

namespace metin2::game::inline characters {

enum motion_mode
{
	MOTION_MODE_GENERAL,
	MOTION_MODE_ONEHAND_SWORD,
	MOTION_MODE_TWOHAND_SWORD,
	MOTION_MODE_DUALHAND_SWORD,
	MOTION_MODE_BOW,
	MOTION_MODE_BELL,
	MOTION_MODE_FAN,
	MOTION_MODE_HORSE,
	MOTION_MODE_MAX_NUM
};

enum public_motion
{
	MOTION_NONE,
	MOTION_WAIT,
	MOTION_WALK,
	MOTION_RUN,
	MOTION_CHANGE_WEAPON,
	MOTION_DAMAGE,
	MOTION_DAMAGE_FLYING,
	MOTION_STAND_UP,
	MOTION_DAMAGE_BACK,
	MOTION_DAMAGE_FLYING_BACK,
	MOTION_STAND_UP_BACK,
	MOTION_DEAD,
	MOTION_DEAD_BACK,
	MOTION_NORMAL_ATTACK,
	MOTION_COMBO_ATTACK_1,
	MOTION_COMBO_ATTACK_2,
	MOTION_COMBO_ATTACK_3,
	MOTION_COMBO_ATTACK_4,
	MOTION_COMBO_ATTACK_5,
	MOTION_COMBO_ATTACK_6,
	MOTION_COMBO_ATTACK_7,
	MOTION_COMBO_ATTACK_8,
	MOTION_INTRO_WAIT,
	MOTION_INTRO_SELECTED,
	MOTION_INTRO_NOT_SELECTED,
	MOTION_SPAWN,
	MOTION_FISHING_THROW,
	MOTION_FISHING_WAIT,
	MOTION_FISHING_STOP,
	MOTION_FISHING_REACT,
	MOTION_FISHING_CATCH,
	MOTION_FISHING_FAIL,
	MOTION_STOP,
	MOTION_SPECIAL_1,
	MOTION_SPECIAL_2,
	MOTION_SPECIAL_3,
	MOTION_SPECIAL_4,
	MOTION_SPECIAL_5,
	PUBLIC_MOTION_END,
	MOTION_MAX_NUM = PUBLIC_MOTION_END,
};

class mob;

class motion
{
	public:
		motion();
		~motion();

		auto			load_from_file(const char * file_name) -> bool;
		auto			load_mob_skill_from_file(const char * file_name, mob * mob_value, int skill_index) -> bool;

		auto			duration() const -> float;
		auto	accum_vector() const -> const vector &;

		auto			is_empty() -> bool;

	protected:
		float			duration_;
		vector		vec3_accumulation_;
};

constexpr auto make_motion_key(uint32_t mode, uint32_t index) -> uint32_t { return (mode << 24) | (index << 8); }

class motion_set
{
	public:
		typedef std::map<uint32_t, std::unique_ptr<motion>>	container;
		typedef container::iterator		iterator;
		typedef container::const_iterator	const_iterator;

	public:
		motion_set();
		~motion_set();

		void		insert(uint32_t key, std::unique_ptr<motion> motion_value);
		auto		load(const char * file_name, int mode, int motion) -> bool;

		auto	get_motion(uint32_t key) const -> const motion *;

	protected:
		container			map_pk_motion_;
};

class motion_manager : public singleton<motion_manager>
{
	public:
		typedef std::map<uint32_t, std::unique_ptr<motion_set>> container;
		typedef container::iterator iterator;

		motion_manager();
		~motion_manager() override;

		auto			build() -> bool;

		auto	get_motion_set(uint32_t vnum) -> const motion_set *;
		auto		get_motion(uint32_t vnum, uint32_t key) -> const motion *;
		auto			    motion_duration(uint32_t vnum, uint32_t key) -> float;

		auto			    normal_attack_duration(uint32_t vnum) -> float;

	protected:
		container		map_pk_motion_set_;

		std::map<uint32_t, float> map_normal_attack_duration_;
};
}
