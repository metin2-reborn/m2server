#pragma once

#include "vector.h"

#include <common/singleton.h>

#include <cstdint>
#include <map>
#include <memory>

enum EMotionMode
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

enum EPublicMotion
{
	MOTION_NONE,                // 0 none
	MOTION_WAIT,                // 1 wait          (00.msa)
	MOTION_WALK,                // 2 walk          (02.msa)
	MOTION_RUN,                 // 3 run           (03.msa)
	MOTION_CHANGE_WEAPON,       // 4 change weapon
	MOTION_DAMAGE,              // 5 front hit     (30.msa)
	MOTION_DAMAGE_FLYING,       // 6 front knockback (32.msa)
	MOTION_STAND_UP,            // 7 front stand up (33.msa)
	MOTION_DAMAGE_BACK,         // 8 back hit      (34.msa)
	MOTION_DAMAGE_FLYING_BACK,  // 9 back knockback (35.msa)
	MOTION_STAND_UP_BACK,       // 10 back stand up (26.msa)
	MOTION_DEAD,                // 11 die          (31.msa)
	MOTION_DEAD_BACK,           // 12 back die     (37.msa)
	MOTION_NORMAL_ATTACK,		// 13 normal attack
	MOTION_COMBO_ATTACK_1,		// 14 combo attack
	MOTION_COMBO_ATTACK_2,      // 15 combo attack
	MOTION_COMBO_ATTACK_3,      // 16 combo attack
	MOTION_COMBO_ATTACK_4,      // 17 combo attack
	MOTION_COMBO_ATTACK_5,      // 18 combo attack
	MOTION_COMBO_ATTACK_6,      // 19 combo attack
	MOTION_COMBO_ATTACK_7,      // 20 combo attack
	MOTION_COMBO_ATTACK_8,      // 21 combo attack
	MOTION_INTRO_WAIT,          // 22 select screen wait
	MOTION_INTRO_SELECTED,      // 23 select screen selected
	MOTION_INTRO_NOT_SELECTED,  // 24 select screen not selected
	MOTION_SPAWN,               // 25 summon
	MOTION_FISHING_THROW,       // 26 fishing cast
	MOTION_FISHING_WAIT,        // 27 fishing wait
	MOTION_FISHING_STOP,        // 28 fishing stop
	MOTION_FISHING_REACT,       // 29 fishing bite
	MOTION_FISHING_CATCH,       // 30 fishing catch
	MOTION_FISHING_FAIL,        // 31 fishing fail
	MOTION_STOP,                // 32 horse stop
	MOTION_SPECIAL_1,           // 33 monster skill
	MOTION_SPECIAL_2,           // 34 
	MOTION_SPECIAL_3,			// 35
	MOTION_SPECIAL_4,			// 36
	MOTION_SPECIAL_5,			// 37
	PUBLIC_MOTION_END,
	MOTION_MAX_NUM = PUBLIC_MOTION_END,
};

class CMob;

class CMotion
{
	public:
		CMotion();
		~CMotion();

		bool			LoadFromFile(const char * c_pszFileName);
		bool			LoadMobSkillFromFile(const char * c_pszFileName, CMob * pMob, int iSkillIndex);

		float			GetDuration() const;
		const VECTOR &	GetAccumVector() const;

		bool			IsEmpty();

	protected:
		float			m_fDuration;
		VECTOR		m_vec3Accumulation;
};

constexpr uint32_t MAKE_MOTION_KEY(uint32_t mode, uint32_t index) { return (mode << 24) | (index << 8); }

class CMotionSet
{
	public:
		typedef std::map<uint32_t, std::unique_ptr<CMotion>>	TContainer;
		typedef TContainer::iterator		iterator;
		typedef TContainer::const_iterator	const_iterator;

	public:
		CMotionSet();
		~CMotionSet();

		void		Insert(uint32_t dwKey, std::unique_ptr<CMotion> pkMotion);
		bool		Load(const char * szFileName, int mode, int motion);

		const CMotion *	GetMotion(uint32_t dwKey) const;

	protected:
		// uint32_t = MOTION_KEY
		TContainer			m_map_pkMotion;
};

class CMotionManager : public singleton<CMotionManager>
{
	public:
		typedef std::map<uint32_t, std::unique_ptr<CMotionSet>> TContainer;
		typedef TContainer::iterator iterator;

		CMotionManager();
		~CMotionManager() override;

		bool			Build();

		const CMotionSet *	GetMotionSet(uint32_t dwVnum);
		const CMotion *		GetMotion(uint32_t dwVnum, uint32_t dwKey);
		float			    GetMotionDuration(uint32_t dwVnum, uint32_t dwKey);

		// POLYMORPH_BUG_FIX
		float			    GetNormalAttackDuration(uint32_t dwVnum);
		// END_OF_POLYMORPH_BUG_FIX

	protected:
		// uint32_t = JOB or MONSTER VNUM
		TContainer		m_map_pkMotionSet;

		// POLYMORPH_BUG_FIX
		std::map<uint32_t, float> m_map_normalAttackDuration;
		// END_OF_POLYMORPH_BUG_FIX
};
