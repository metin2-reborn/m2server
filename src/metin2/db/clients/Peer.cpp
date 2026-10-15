#include <metin2/db/clients/Peer.h>

#include <metin2/db/clients/ClientManager.h>
#include <metin2/db/items/ItemIDRangeManager.h>

#include <metin2/proto/tables.h>
#include <metin2/net/connection.h>
#include <metin2/log/log.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <system_error>
#include <utility>

namespace metin2::db::inline clients {

peer::peer(std::shared_ptr<net::connection> connection) : connection_(std::move(connection))
{
	input_consumed_ = 0;
	state_ = STATE_PLAYING;
	channel_ = 0;
	listen_port_ = 0;
	p2_p_port_ = 0;

	memset(maps_, 0, sizeof(maps_));

	item_range_.min = item_range_.max = item_range_.usable_item_id_min = 0;
	item_spare_range_.min = item_spare_range_.max = item_spare_range_.usable_item_id_min = 0;

	static uint32_t current_handle = 0;
	handle_ = ++current_handle;

	metin2::log::info( "Connection accepted. (host: {} handle: {:d})", host(), handle_);
	connection_->start(*this);
}

peer::~peer()
{
	connection_->close();

	metin2::log::info( "Connection closed. (host: {})", host());
}

void peer::return_item_id_range()
{
	metin2::log::info( "ItemIDRange: returned. {:d} ~ {:d}", item_range_.min, item_range_.max);

	item_id_range_manager::instance().update_range(item_range_.min, item_range_.max);

	item_range_.min = 0;
	item_range_.max = 0;
	item_range_.usable_item_id_min = 0;
}

auto peer::on_receive(std::span<std::uint8_t> input, std::size_t /*received*/) -> std::size_t
{
	input_ = input;
	input_consumed_ = 0;
	client_manager::instance().process_packets(this);
	input_ = {};
	return input_consumed_;
}

void peer::on_close(const std::error_code & error)
{
	if (error)
		metin2::log::info( "Connection lost. (host: {}: {})", host(), error.message().c_str());

	state_ = STATE_CLOSE;
}

void peer::encode(const void * data, uint32_t size)
{
	if (!connection_->send(data, size))
		metin2::log::error("cannot send {:d} bytes to {}", size, host());
}

uint32_t peer::handle()
{
	return handle_;
}

void peer::set_user_count(uint32_t dwCount)
{
}

bool peer::peek_packet(int & iBytesProceed, uint8_t & header, uint32_t & dwHandle, uint32_t & dwLength, const char ** data)
{
	const int iLength = (int) input_.size();

	if (iLength < iBytesProceed + 9)
		return false;

	const char * buf = (const char *) input_.data();
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

void peer::encode_header(uint8_t header, uint32_t dwHandle, uint32_t dwSize)
{
	header_kind h;

	metin2::log::debug( "EncodeHeader {:d} handle {:d} size {:d}", header, dwHandle, dwSize);

	h.header = header;
	h.handle = dwHandle;
	h.size = dwSize;

	encode(&h, sizeof(header_kind));
}

void peer::encode_return(uint8_t header, uint32_t dwHandle)
{
	encode_header(header, dwHandle, 0);
}

void peer::set_p2_p_port(uint16_t wPort)
{
	p2_p_port_ = wPort;
}

void peer::set_maps(int32_t * pl)
{
	std::memcpy(maps_, pl, sizeof(maps_));
}

void peer::send_spare_item_id_range()
{
	if (item_spare_range_.min == 0 || item_spare_range_.max == 0 || item_spare_range_.usable_item_id_min == 0)
	{
		encode_header(HEADER_DG_ACK_SPARE_ITEM_ID_RANGE, 0, sizeof(item_id_range_table));
		encode(&item_spare_range_, sizeof(item_id_range_table));
	}
	else
	{
		set_item_id_range(item_spare_range_);

		if (set_spare_item_id_range(item_id_range_manager::instance().range()) == false)
		{
			metin2::log::info( "ItemIDRange: spare range set error");
			item_spare_range_.min = item_spare_range_.max = item_spare_range_.usable_item_id_min = 0;
		}

		encode_header(HEADER_DG_ACK_SPARE_ITEM_ID_RANGE, 0, sizeof(item_id_range_table));
		encode(&item_spare_range_, sizeof(item_id_range_table));
	}
}

bool peer::set_item_id_range(item_id_range_table itemRange)
{
	if (itemRange.min == 0 || itemRange.max == 0 || itemRange.usable_item_id_min == 0) return false;

	item_range_ = itemRange;
	metin2::log::info( "ItemIDRange: SET {} {:d} ~ {:d} start: {:d}", public_ip(), item_range_.min, item_range_.max, item_range_.usable_item_id_min);

	return true;
}

bool peer::set_spare_item_id_range(item_id_range_table itemRange)
{
	if (itemRange.min == 0 || itemRange.max == 0 || itemRange.usable_item_id_min == 0) return false;

	item_spare_range_ = itemRange;
	metin2::log::info( "ItemIDRange: SPARE SET {} {:d} ~ {:d} start: {:d}", public_ip(), item_spare_range_.min, item_spare_range_.max,
			item_spare_range_.usable_item_id_min);

	return true;
}

bool peer::check_item_id_range_collision(item_id_range_table itemRange)
{
	if (item_range_.min < itemRange.max && item_range_.max > itemRange.min)
	{
		metin2::log::error("ItemIDRange: Collision!! this {:d} ~ {:d} check {:d} ~ {:d}",
				item_range_.min, item_range_.max, itemRange.min, itemRange.max);
		return false;
	}

	if (item_spare_range_.min < itemRange.max && item_spare_range_.max > itemRange.min)
	{
		metin2::log::error("ItemIDRange: Collision with spare range this {:d} ~ {:d} check {:d} ~ {:d}",
				item_spare_range_.min, item_spare_range_.max, itemRange.min, itemRange.max);
		return false;
	}
	
	return true;
}
}
