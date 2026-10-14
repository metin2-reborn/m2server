
#ifndef __POLYMORPH_UTILS__
#define __POLYMORPH_UTILS__

#include "char.h"
#include "typedef.h"

#include <common/singleton.h>

#include <cstdint>
#include <unordered_map>

inline constexpr int POLYMORPH_SKILL_ID = 129;
inline constexpr int POLYMORPH_BOOK_ID = 50322;
enum POLYMORPH_BONUS_TYPE
{
	POLYMORPH_NO_BONUS,
	POLYMORPH_ATK_BONUS,
	POLYMORPH_DEF_BONUS,
	POLYMORPH_SPD_BONUS,
};

class CPolymorphUtils : public singleton<CPolymorphUtils>
{
	private :
		std::unordered_map<uint32_t, uint32_t> m_mapSPDType;
		std::unordered_map<uint32_t, uint32_t> m_mapATKType;
		std::unordered_map<uint32_t, uint32_t> m_mapDEFType;

	public :
		CPolymorphUtils();

		POLYMORPH_BONUS_TYPE GetBonusType(uint32_t dwVnum);

		bool PolymorphCharacter(CHARACTER * pChar, CItem * pItem, const CMob* pMob);
		bool UpdateBookPracticeGrade(CHARACTER * pChar, CItem * pItem);
		bool GiveBook(CHARACTER * pChar, uint32_t dwMobVnum, uint32_t dwPracticeCount, uint8_t BookLevel, uint8_t LevelLimit);
		bool BookUpgrade(CHARACTER * pChar, CItem * pItem);
};

#endif /*__POLYMORPH_UTILS__*/
