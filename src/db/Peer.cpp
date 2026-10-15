#include "Peer.h"

#include "ClientManager.h"
#include "ItemIDRangeManager.h"

#include <common/tables.h>
#include <libnet/connection.h>
#include <metin2/log/log.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <system_error>
#include <utility>

namespace metin2::db {

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

	metin2::log::info( "Connection accepted. (host: {} handle: {:d})", GetHost(), m_dwHandle);
	m_connection->start(*this);
}

CPeer::~CPeer()
{
	m_connection->close();

	metin2::log::info( "Connection closed. (host: {})", GetHost());
}

void CPeer::ReturnItemIDRange()
{
	metin2::log::info( "ItemIDRange: returned. {:d} ~ {:d}", m_itemRange.dwMin, m_itemRange.dwMax);

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
		metin2::log::info( "Connection lost. (host: {}: {})", GetHost(), error.message().c_str());

	m_state = STATE_CLOSE;
}

void CPeer::Encode(const void * data, uint32_t size)
{
	if (!m_connection->send(data, size))
		metin2::log::error("cannot send {:d} bytes to {}", size, GetHost());
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
		metin2::log::info( "PeekPacket: not enough buffer size: len {:d}, recv {:d}", 
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

	metin2::log::debug( "EncodeHeader {:d} handle {:d} size {:d}", header, dwHandle, dwSize);

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
			metin2::log::info( "ItemIDRange: spare range set error");
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
	metin2::log::info( "ItemIDRange: SET {} {:d} ~ {:d} start: {:d}", GetPublicIP(), m_itemRange.dwMin, m_itemRange.dwMax, m_itemRange.dwUsableItemIDMin);

	return true;
}

bool CPeer::SetSpareItemIDRange(TItemIDRangeTable itemRange)
{
	if (itemRange.dwMin == 0 || itemRange.dwMax == 0 || itemRange.dwUsableItemIDMin == 0) return false;

	m_itemSpareRange = itemRange;
	metin2::log::info( "ItemIDRange: SPARE SET {} {:d} ~ {:d} start: {:d}", GetPublicIP(), m_itemSpareRange.dwMin, m_itemSpareRange.dwMax,
			m_itemSpareRange.dwUsableItemIDMin);

	return true;
}

bool CPeer::CheckItemIDRangeCollision(TItemIDRangeTable itemRange)
{
	if (m_itemRange.dwMin < itemRange.dwMax && m_itemRange.dwMax > itemRange.dwMin)
	{
		metin2::log::error("ItemIDRange: Collision!! this {:d} ~ {:d} check {:d} ~ {:d}",
				m_itemRange.dwMin, m_itemRange.dwMax, itemRange.dwMin, itemRange.dwMax);
		return false;
	}

	if (m_itemSpareRange.dwMin < itemRange.dwMax && m_itemSpareRange.dwMax > itemRange.dwMin)
	{
		metin2::log::error("ItemIDRange: Collision with spare range this {:d} ~ {:d} check {:d} ~ {:d}",
				m_itemSpareRange.dwMin, m_itemSpareRange.dwMax, itemRange.dwMin, itemRange.dwMax);
		return false;
	}
	
	return true;
}
}
