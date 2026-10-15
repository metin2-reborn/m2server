#include "polymorph.h"

#include "affect.h"
#include "char.h"
#include "item.h"
#include "locale.hpp"
#include "mob_manager.h"

#include <common/length.h>
#include <metin2/log/log.h>
#include <libthecore/utils.h>

#include <cstdint>
#include <unordered_map>
#include <utility>

namespace metin2::game {

CPolymorphUtils::CPolymorphUtils()
{
	m_mapSPDType.insert(std::make_pair(101, 101));
	m_mapSPDType.insert(std::make_pair(1901, 1901));
}

POLYMORPH_BONUS_TYPE CPolymorphUtils::GetBonusType(uint32_t dwVnum)
{
	std::unordered_map<uint32_t, uint32_t>::iterator iter;

	iter = m_mapSPDType.find(dwVnum);

	if (iter != m_mapSPDType.end())
		return POLYMORPH_SPD_BONUS;

	iter = m_mapATKType.find(dwVnum);

	if (iter != m_mapATKType.end())
		return POLYMORPH_ATK_BONUS;

	iter = m_mapDEFType.find(dwVnum);

	if (iter != m_mapDEFType.end())
		return POLYMORPH_DEF_BONUS;

	return POLYMORPH_NO_BONUS;
}

bool CPolymorphUtils::PolymorphCharacter(CHARACTER * pChar, CItem * pItem, const CMob* pMob)
{
	uint8_t bySkillLevel = pChar->GetSkillLevel(POLYMORPH_SKILL_ID);
	uint32_t dwDuration = 0;
	uint32_t dwBonusPercent = 0;
	int iPolyPercent = 0;

	switch (pChar->GetSkillMasterType(POLYMORPH_SKILL_ID))
	{
		case SKILL_NORMAL:
			dwDuration = 10;
			break;

		case SKILL_MASTER:
			dwDuration = 15;
			break;

		case SKILL_GRAND_MASTER:
			dwDuration = 20;
			break;

		case SKILL_PERFECT_MASTER:
			dwDuration = 25;
			break;

		default:
			return false;
	}

	// Polymorph chance = character level - mob level + polymorph book level + 29 + polymorph skill level
	iPolyPercent = pChar->GetLevel() - pMob->m_table.bLevel + pItem->GetSocket(2) + (29 + bySkillLevel);

	if (iPolyPercent <= 0)
	{
		pChar->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("둔갑에 실패 하였습니다"));
		return false;
	}
	else
	{
		if (number(1, 100) > iPolyPercent)
		{
			pChar->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("둔갑에 실패 하였습니다"));
			return false;
		}
	}

	pChar->AddAffect(AFFECT_POLYMORPH, POINT_POLYMORPH, pMob->m_table.dwVnum, AFF_POLYMORPH, dwDuration, 0, true);

	// Polymorph bonus = polymorph skill level + polymorph book level
	dwBonusPercent = bySkillLevel + pItem->GetSocket(2);

	switch (GetBonusType(pMob->m_table.dwVnum))
	{
		case POLYMORPH_ATK_BONUS:
			pChar->AddAffect(AFFECT_POLYMORPH, POINT_ATT_BONUS, dwBonusPercent, AFF_POLYMORPH, dwDuration - 1, 0, false);
			break;

		case POLYMORPH_DEF_BONUS:
			pChar->AddAffect(AFFECT_POLYMORPH, POINT_DEF_BONUS, dwBonusPercent, AFF_POLYMORPH, dwDuration - 1, 0, false);
			break;

		case POLYMORPH_SPD_BONUS:
			pChar->AddAffect(AFFECT_POLYMORPH, POINT_MOV_SPEED, dwBonusPercent, AFF_POLYMORPH, dwDuration - 1, 0, false);
			break;

		default:
		case POLYMORPH_NO_BONUS:
			break;
	}

	return true;
}

bool CPolymorphUtils::UpdateBookPracticeGrade(CHARACTER * pChar, CItem * pItem)
{
	if (pChar == nullptr || pItem == nullptr)
		return false;

	if (pItem->GetSocket(1) > 0)
		pItem->SetSocket(1, pItem->GetSocket(1) - 1);
	else
		pChar->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("둔갑서 수련을 마첬습니다. 신선에게 찾아가세요."));

	return true;
}

bool CPolymorphUtils::GiveBook(CHARACTER * pChar, uint32_t dwMobVnum, uint32_t dwPracticeCount, uint8_t BookLevel, uint8_t LevelLimit)
{
	// Socket0                Socket1     Socket2
	// Polymorph mob number   Training    Book level
	if (pChar == nullptr)
		return false;

	CItem * pItem = pChar->AutoGiveItem(POLYMORPH_BOOK_ID, 1);

	if (pItem == nullptr)
		return false;

	if (CMobManager::instance().Get(dwMobVnum) == nullptr)
	{
		metin2::log::error("Wrong Polymorph vnum passed: CPolymorphUtils::GiveBook(PID({:d}), {:d} {:d} {:d} {:d})", 
				pChar->GetPlayerID(), dwMobVnum, dwPracticeCount, BookLevel, LevelLimit);
		return false;
	}

	pItem->SetSocket(0, dwMobVnum);			// Monster number to polymorph into
	pItem->SetSocket(1, dwPracticeCount);		// Number of trainings required
	pItem->SetSocket(2, BookLevel);			// Training level
	return true;
}

bool CPolymorphUtils::BookUpgrade(CHARACTER * pChar, CItem * pItem)
{
	if (pChar == nullptr || pItem == nullptr)
		return false;

	pItem->SetSocket(1, pItem->GetSocket(2) * 50);
	pItem->SetSocket(2, pItem->GetSocket(2)+1);
	return true;
}
}
