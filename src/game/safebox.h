#ifndef __INC_METIN_II_GAME_SAFEBOX_H__
#define __INC_METIN_II_GAME_SAFEBOX_H__
#include "typedef.h"

#include <common/tables.h>

#include <cstdint>

class CHARACTER;
class CItem;
class CGrid;

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
		bool		IsValidPosition(uint32_t dwPos);

		void		SetWindowMode(uint8_t bWindowMode);

	protected:
		void		__Destroy();

		CHARACTER *	m_pkChrOwner;
		CItem *		m_pkItems[SAFEBOX_MAX_NUM];
		CGrid *		m_pkGrid;
		int		m_iSize;
		int32_t		m_lGold;

		uint8_t		m_bWindowMode;
};

#endif
