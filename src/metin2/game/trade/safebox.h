#pragma once
#include <metin2/game/app/typedef.h>

#include <metin2/proto/tables.h>
#include <metin2/common/grid.h>

#include <cstdint>
#include <memory>

namespace metin2::game::inline characters {
    class CHARACTER;
}
namespace metin2::game::inline items {
    class CItem;
}

namespace metin2::game::inline trade {


class CSafebox
{
	public:
		CSafebox(CHARACTER * pkChrOwner, int iSize, uint32_t dwGold);
		~CSafebox();

		bool		Add(uint32_t dwPos, CItem * pkItem);
		CItem *		Get(uint32_t dwPos);
		CItem *		Remove(uint32_t dwPos);
		void		ChangeSize(int iSize);

		bool		MoveItem(uint8_t bCell, uint8_t bDestCell, uint8_t count);
		CItem *		GetItem(uint8_t bCell);

		void		Save();

		bool		IsEmpty(uint32_t dwPos, uint8_t bSize);
		// Tells the owner's client what is in a slot.
		void		SendItemSet(uint32_t dwPos);
		bool		IsValidPosition(uint32_t dwPos);

		void		SetWindowMode(uint8_t bWindowMode);

	protected:
		void		__Destroy();

		CHARACTER *	m_pkChrOwner;
		CItem *		m_pkItems[SAFEBOX_MAX_NUM];
		std::unique_ptr<CGrid>	m_pkGrid;
		int		m_iSize;
		int32_t		m_lGold;

		uint8_t		m_bWindowMode;
};
}
