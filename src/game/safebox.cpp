#include "safebox.h"

#include "char.h"
#include "desc_client.h"
#include "item.h"
#include "item_manager.h"
#include "packet.h"
#include "typedef.h"
#include "utils.h"

#include <common/length.h>
#include <common/tables.h>
#include <libgame/grid.h>
#include <metin2/log/log.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>

namespace metin2::game {

CSafebox::CSafebox(CHARACTER * pkChrOwner, int iSize, uint32_t dwGold) : m_pkChrOwner(pkChrOwner), m_iSize(iSize), m_lGold(dwGold)
{
	assert(m_pkChrOwner != nullptr);
	memset(m_pkItems, 0, sizeof(m_pkItems));

	if (m_iSize)
		m_pkGrid = std::make_unique<CGrid>(5, m_iSize);

	m_bWindowMode = SAFEBOX;
}

CSafebox::~CSafebox()
{
	__Destroy();
}

void CSafebox::SetWindowMode(uint8_t bMode)
{
	m_bWindowMode = bMode;
}

void CSafebox::__Destroy()
{
	for (int i = 0; i < SAFEBOX_MAX_NUM; ++i)
	{
		if (m_pkItems[i])
		{
			m_pkItems[i]->SetSkipSave(true);
			ITEM_MANAGER::instance().FlushDelayedSave(m_pkItems[i]);

			ITEM_MANAGER::instance().DestroyItem(m_pkItems[i]->RemoveFromCharacter());
			m_pkItems[i] = nullptr;
		}
	}

	m_pkGrid.reset();
}

bool CSafebox::Add(uint32_t dwPos, CItem * pkItem)
{
	if (!IsValidPosition(dwPos))
	{
		metin2::log::error("SAFEBOX: item on wrong position at {:d} (size of grid = {:d})", dwPos, m_pkGrid->GetSize());
		return false;
	}

	pkItem->SetWindow(m_bWindowMode);
	pkItem->SetCell(m_pkChrOwner, dwPos);
	pkItem->Save(); // Save must be called forcibly.
	ITEM_MANAGER::instance().FlushDelayedSave(pkItem);

	m_pkGrid->Put(dwPos, 1, pkItem->GetSize());
	m_pkItems[dwPos] = pkItem;

	SendItemSet(dwPos);
	metin2::log::debug( "SAFEBOX: ADD {} {} count {:d}", m_pkChrOwner->GetName(), pkItem->GetName(), pkItem->GetCount());
	return true;
}

void CSafebox::SendItemSet(uint32_t dwPos)
{
	CItem * pkItem = m_pkItems[dwPos];

	if (!pkItem || !m_pkChrOwner->GetDesc())
		return;

	TPacketGCItemSet pack;

	pack.header	= m_bWindowMode == SAFEBOX ? HEADER_GC_SAFEBOX_SET : HEADER_GC_MALL_SET;
	pack.Cell	= TItemPos(m_bWindowMode, dwPos);
	pack.vnum	= pkItem->GetVnum();
	pack.count	= pkItem->GetCount();
	pack.flags	= pkItem->GetFlag();
	pack.anti_flags	= pkItem->GetAntiFlag();
	std::memcpy(pack.alSockets, pkItem->GetSockets(), sizeof(pack.alSockets));
	std::memcpy(pack.aAttr, pkItem->GetAttributes(), sizeof(pack.aAttr));

	m_pkChrOwner->GetDesc()->Packet(&pack, sizeof(pack));
}

CItem * CSafebox::Get(uint32_t dwPos)
{
	if (!m_pkGrid || dwPos >= m_pkGrid->GetSize())
		return nullptr;

	return m_pkItems[dwPos];
}

CItem * CSafebox::Remove(uint32_t dwPos)
{
	CItem * pkItem = Get(dwPos);

	if (!pkItem)
		return nullptr;

	if (!m_pkGrid)
		metin2::log::error("Safebox::Remove : nil grid");
	else
		m_pkGrid->Get(dwPos, 1, pkItem->GetSize());

	pkItem->RemoveFromCharacter();

	m_pkItems[dwPos] = nullptr;

	TPacketGCItemDel pack;

	pack.header	= m_bWindowMode == SAFEBOX ? HEADER_GC_SAFEBOX_DEL : HEADER_GC_MALL_DEL;
	pack.pos	= dwPos;

	m_pkChrOwner->GetDesc()->Packet(&pack, sizeof(pack));
	metin2::log::debug( "SAFEBOX: REMOVE {} {} count {:d}", m_pkChrOwner->GetName(), pkItem->GetName(), pkItem->GetCount());
	return pkItem;
}

void CSafebox::Save()
{
	TSafeboxTable t;

	memset(&t, 0, sizeof(TSafeboxTable));

	t.dwID = m_pkChrOwner->GetDesc()->GetAccountTable().id;
	t.dwGold = m_lGold;

	db_clientdesc->DBPacket(HEADER_GD_SAFEBOX_SAVE, 0, &t, sizeof(TSafeboxTable));
	metin2::log::debug( "SAFEBOX: SAVE {}", m_pkChrOwner->GetName());
}

bool CSafebox::IsEmpty(uint32_t dwPos, uint8_t bSize)
{
	if (!m_pkGrid)
		return false;

	return m_pkGrid->IsEmpty(dwPos, 1, bSize);
}

void CSafebox::ChangeSize(int iSize)
{
	// if the current size is larger than the argument, leave the size as is.
	if (m_iSize >= iSize)
		return;

	m_iSize = iSize;

	if (m_pkGrid)
		m_pkGrid = std::make_unique<CGrid>(*m_pkGrid, 5, m_iSize);
	else
		m_pkGrid = std::make_unique<CGrid>(5, m_iSize);
}

CItem * CSafebox::GetItem(uint8_t bCell)
{
	if (bCell >= 5 * m_iSize)
	{
		metin2::log::error("CHARACTER::GetItem: invalid item cell {:d}", bCell);
		return nullptr;
	}

	return m_pkItems[bCell];
}

bool CSafebox::MoveItem(uint8_t bCell, uint8_t bDestCell, uint8_t count)
{
	CItem * item;

	int max_position = 5 * m_iSize;

	if (bCell >= max_position || bDestCell >= max_position)
		return false;

	if (!(item = GetItem(bCell)))
		return false;

	if (item->IsExchanging())
		return false;

	if (item->GetCount() < count)
		return false;

	{
		CItem * item2;

		if ((item2 = GetItem(bDestCell)) && item != item2 && item2->IsStackable() &&
				!IS_SET(item2->GetAntiFlag(), ITEM_ANTIFLAG_STACK) &&
				item2->GetVnum() == item->GetVnum()) // for stackable items
		{
			for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
				if (item2->GetSocket(i) != item->GetSocket(i))
					return false;

			if (count == 0)
				count = item->GetCount();

			count = std::min<int>(ITEM_MAX_COUNT - item2->GetCount(), count);

			// The destination stack is full: nothing moves.
			if (count == 0)
				return false;

			item2->SetCount(item2->GetCount() + count);
			SendItemSet(bDestCell);

			// The whole source stack merged: it goes away; otherwise the rest stays where it was.
			if (count == item->GetCount())
				ITEM_MANAGER::instance().RemoveItem(item, "SAFEBOX_STACK");
			else
			{
				item->SetCount(item->GetCount() - count);
				SendItemSet(bCell);
			}

			metin2::log::debug( "SAFEBOX: STACK {} {:d} -> {:d} {} count {:d}", m_pkChrOwner->GetName(), bCell, bDestCell, item2->GetName(), item2->GetCount());
			return true;
		}

		if (!IsEmpty(bDestCell, item->GetSize()))
			return false;

		m_pkGrid->Get(bCell, 1, item->GetSize());

		if (!m_pkGrid->Put(bDestCell, 1, item->GetSize()))
		{
			m_pkGrid->Put(bCell, 1, item->GetSize());
			return false;
		}
		else
		{
			m_pkGrid->Get(bDestCell, 1, item->GetSize());
			m_pkGrid->Put(bCell, 1, item->GetSize());
		}

		metin2::log::debug( "SAFEBOX: MOVE {} {:d} -> {:d} {} count {:d}", m_pkChrOwner->GetName(), bCell, bDestCell, item->GetName(), item->GetCount());

		Remove(bCell);
		Add(bDestCell, item);
	}

	return true;
}

bool CSafebox::IsValidPosition(uint32_t dwPos)
{
	if (!m_pkGrid)
		return false;

	if (dwPos >= m_pkGrid->GetSize())
		return false;

	return true;
}
}
