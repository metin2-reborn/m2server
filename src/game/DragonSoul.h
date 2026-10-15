#pragma once

#include "typedef.h"

#include <common/length.h>
#include <common/singleton.h>

#include <cstdint>


class CHARACTER;
class CItem;

class DragonSoulTable;

class DSManager : public singleton<DSManager>
{
public:
	DSManager();
	~DSManager() override;
	bool	ReadDragonSoulTableFile(const char * c_pszFileName);

	void	GetDragonSoulInfo(uint32_t dwVnum, uint8_t& bType, uint8_t& bGrade, uint8_t& bStep, uint8_t& bRefine) const;
	// fixme : use titempos
	uint16_t	GetBasePosition(CItem * const pItem) const;
	bool	IsValidCellForThisItem(CItem * const pItem, const TItemPos& Cell) const;
	int		GetDuration(CItem * const pItem) const;
	
	// Takes a dragon soul stone and extracts a specific dragon heart
	bool	ExtractDragonHeart(CHARACTER * ch, CItem * pItem, CItem * pExtractor = nullptr);

	// Decides whether removing a specific dragon soul stone (pItem) from the equipment window succeeds, 
	// and gives by-products on failure. (by-products are defined in dragon_soul_table.txt)
	// If DestCell is invalid, on success the dragon soul stone is automatically placed in an empty slot.
	// On failure, the dragon soul stone (pItem) is deleted.
	// If there is an extractor item, the extraction success rate increases by pExtractor->GetValue(0)%.
	// By-products are always added automatically.
	bool	PullOut(CHARACTER * ch, TItemPos DestCell, CItem *& pItem, CItem * pExtractor = nullptr);

	// Dragon soul stone upgrade function
	bool	DoRefineGrade(CHARACTER * ch, TItemPos (&aItemPoses)[DRAGON_SOUL_REFINE_GRID_SIZE]);
	bool	DoRefineStep(CHARACTER * ch, TItemPos (&aItemPoses)[DRAGON_SOUL_REFINE_GRID_SIZE]);
	bool	DoRefineStrength(CHARACTER * ch, TItemPos (&aItemPoses)[DRAGON_SOUL_REFINE_GRID_SIZE]);
	
	bool	DragonSoulItemInitialize(CItem * pItem);

	bool	IsTimeLeftDragonSoul(CItem * pItem) const;
	int		LeftTime(CItem * pItem) const;
	bool	ActivateDragonSoul(CItem * pItem);
	bool	DeactivateDragonSoul(CItem * pItem, bool bSkipRefreshOwnerActiveState = false);
	bool	IsActiveDragonSoul(CItem * pItem) const;
private:
	void	SendRefineResultPacket(CHARACTER * ch, uint8_t bSubHeader, const TItemPos& pos);

	// Looks at the character's dragon soul deck and, if no dragon soul stone is active, turns off the character's dragon soul active state.
	void	RefreshDragonSoulState(CHARACTER * ch);

	uint32_t	MakeDragonSoulVnum(uint8_t bType, uint8_t grade, uint8_t step, uint8_t refine);
	bool	PutAttributes(CItem * pDS);
	bool	RefreshItemAttributes(CItem * pItem);

	DragonSoulTable*	m_pTable;
};
