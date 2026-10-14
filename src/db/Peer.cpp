#include "Peer.h"

#include "ClientManager.h"
#include "ItemIDRangeManager.h"

#include <common/tables.h>
#include <libnet/connection.h>
#include <libthecore/log.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <system_error>
#include <utility>

CPeer::CPeer(std::shared_ptr<net::connection> connection) : m_connection(std::move(connection))
{
	m_inputConsumed = 0;
	m_state = STATE_PLAYING;
	m_bChannel = 0;
	m_wListenPort = 0;
	m_wP2PPort = 0;

	memset(m_alMaps, 0, sizeof(m_alMaps));

	m_itemRange.dwMin = m_itemRange.dwMax = m_itemRange.dwUsableItemIDMin = 0;
	m_itemSpareRange.dwMin = m_itemSpareRange.dwMax = m_itemSpareRange.dwUsableItemIDMin = 0;

	static uint32_t current_handle = 0;
	m_dwHandle = ++current_handle;

	sys_log(0, "Connection accepted. (host: %s handle: %u)", GetHost(), m_dwHandle);
	m_connection->start(*this);
}

CPeer::~CPeer()
{
	m_connection->close();

	sys_log(0, "Connection closed. (host: %s)", GetHost());
}

void CPeer::ReturnItemIDRange()
{
	sys_log(0, "ItemIDRange: returned. %u ~ %u", m_itemRange.dwMin, m_itemRange.dwMax);

	CItemIDRangeManager::instance().UpdateRange(m_itemRange.dwMin, m_itemRange.dwMax);

	m_itemRange.dwMin = 0;
	m_itemRange.dwMax = 0;
	m_itemRange.dwUsableItemIDMin = 0;
}

auto CPeer::on_receive(std::span<std::uint8_t> input, std::size_t /*received*/) -> std::size_t
{
	m_input = input;
	m_inputConsumed = 0;
	CClientManager::instance().ProcessPackets(this);
	m_input = {};
	return m_inputConsumed;
}

void CPeer::on_close(const std::error_code & error)
{
	if (error)
		sys_log(0, "Connection lost. (host: %s: %s)", GetHost(), error.message().c_str());

	m_state = STATE_CLOSE;
}

void CPeer::Encode(const void * data, uint32_t size)
{
	if (!m_connection->send(data, size))
		sys_err("cannot send %u bytes to %s", size, GetHost());
}

uint32_t CPeer::GetHandle()
{
	return m_dwHandle;
}

void CPeer::SetUserCount(uint32_t dwCount)
{
}

bool CPeer::PeekPacket(int & iBytesProceed, uint8_t & header, uint32_t & dwHandle, uint32_t & dwLength, const char ** data)
{
	const int iLength = (int) m_input.size();

	if (iLength < iBytesProceed + 9)
		return false;

	const char * buf = (const char *) m_input.data();
	buf += iBytesProceed;

	header	= *(buf++);

	dwHandle	= *((uint32_t *) buf);
	buf		+= sizeof(uint32_t);

	dwLength	= *((uint32_t *) buf);
	buf		+= sizeof(uint32_t);

	if (iBytesProceed + dwLength + 9 > (uint32_t) iLength)
	{
		sys_log(0, "PeekPacket: not enough buffer size: len %u, recv %d", 
				9+dwLength, iLength-iBytesProceed);
		return false;
	}

	*data = buf;
	iBytesProceed += dwLength + 9;
	return true;
}

void CPeer::EncodeHeader(uint8_t header, uint32_t dwHandle, uint32_t dwSize)
{
	HEADER h;

	sys_log(1, "EncodeHeader %u handle %u size %u", header, dwHandle, dwSize);

	h.bHeader = header;
	h.dwHandle = dwHandle;
	h.dwSize = dwSize;

	Encode(&h, sizeof(HEADER));
}

void CPeer::EncodeReturn(uint8_t header, uint32_t dwHandle)
{
	EncodeHeader(header, dwHandle, 0);
}

void CPeer::SetP2PPort(uint16_t wPort)
{
	m_wP2PPort = wPort;
}

void CPeer::SetMaps(int32_t * pl)
{
	std::memcpy(m_alMaps, pl, sizeof(m_alMaps));
}

void CPeer::SendSpareItemIDRange()
{
	if (m_itemSpareRange.dwMin == 0 || m_itemSpareRange.dwMax == 0 || m_itemSpareRange.dwUsableItemIDMin == 0)
	{
		EncodeHeader(HEADER_DG_ACK_SPARE_ITEM_ID_RANGE, 0, sizeof(TItemIDRangeTable));
		Encode(&m_itemSpareRange, sizeof(TItemIDRangeTable));
	}
	else
	{
		SetItemIDRange(m_itemSpareRange);

		if (SetSpareItemIDRange(CItemIDRangeManager::instance().GetRange()) == false)
		{
			sys_log(0, "ItemIDRange: spare range set error");
			m_itemSpareRange.dwMin = m_itemSpareRange.dwMax = m_itemSpareRange.dwUsableItemIDMin = 0;
		}

		EncodeHeader(HEADER_DG_ACK_SPARE_ITEM_ID_RANGE, 0, sizeof(TItemIDRangeTable));
		Encode(&m_itemSpareRange, sizeof(TItemIDRangeTable));
	}
}

bool CPeer::SetItemIDRange(TItemIDRangeTable itemRange)
{
	if (itemRange.dwMin == 0 || itemRange.dwMax == 0 || itemRange.dwUsableItemIDMin == 0) return false;

	m_itemRange = itemRange;
	sys_log(0, "ItemIDRange: SET %s %u ~ %u start: %u", GetPublicIP(), m_itemRange.dwMin, m_itemRange.dwMax, m_itemRange.dwUsableItemIDMin);

	return true;
}

bool CPeer::SetSpareItemIDRange(TItemIDRangeTable itemRange)
{
	if (itemRange.dwMin == 0 || itemRange.dwMax == 0 || itemRange.dwUsableItemIDMin == 0) return false;

	m_itemSpareRange = itemRange;
	sys_log(0, "ItemIDRange: SPARE SET %s %u ~ %u start: %u", GetPublicIP(), m_itemSpareRange.dwMin, m_itemSpareRange.dwMax,
			m_itemSpareRange.dwUsableItemIDMin);

	return true;
}

bool CPeer::CheckItemIDRangeCollision(TItemIDRangeTable itemRange)
{
	if (m_itemRange.dwMin < itemRange.dwMax && m_itemRange.dwMax > itemRange.dwMin)
	{
		sys_err("ItemIDRange: Collision!! this %u ~ %u check %u ~ %u",
				m_itemRange.dwMin, m_itemRange.dwMax, itemRange.dwMin, itemRange.dwMax);
		return false;
	}

	if (m_itemSpareRange.dwMin < itemRange.dwMax && m_itemSpareRange.dwMax > itemRange.dwMin)
	{
		sys_err("ItemIDRange: Collision with spare range this %u ~ %u check %u ~ %u",
				m_itemSpareRange.dwMin, m_itemSpareRange.dwMax, itemRange.dwMin, itemRange.dwMax);
		return false;
	}
	
	return true;
}


