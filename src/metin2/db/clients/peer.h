// vim: ts=8 sw=4
#pragma once

#include <metin2/proto/tables.h>
#include <metin2/net/connection.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <system_error>

namespace metin2::db::inline clients {

// A game or auth server connected to the db. Its packets are handed to
// CClientManager::ProcessPackets as they arrive.
class peer : public net::connection_handler
{
    public:
#pragma pack(1)
	struct header_kind {   
	    uint8_t    header;
	    uint32_t   handle;
	    uint32_t   size;
	};
#pragma pack()
	enum state
	{
	    STATE_CLOSE = 0,
	    STATE_PLAYING = 1
	};

	explicit peer(std::shared_ptr<net::connection> connection);
	~peer() override;

	auto	on_receive(std::span<std::uint8_t> input, std::size_t received) -> std::size_t override;
	void	on_close(const std::error_code & error) override;

	// The connection ended; CClientManager removes the peer on its next pass.
	bool	is_closed() const	{ return state_ == STATE_CLOSE; }
	const char *	host() const	{ return connection_->remote_address().c_str(); }

	void	encode_byte(uint8_t b)		{ encode(&b, sizeof(b)); }
	void	encode_word(uint16_t w)		{ encode(&w, sizeof(w)); }
	void	encode_dword(uint32_t dw)	{ encode(&dw, sizeof(dw)); }
	void	encode(const void * data, uint32_t size);

	void	encode_header(uint8_t header, uint32_t handle_value, uint32_t size_value);
	bool 	peek_packet(int & bytes_proceed, uint8_t & header, uint32_t & handle, uint32_t & length, const char ** data);
	void	encode_return(uint8_t header, uint32_t handle);
	void	recv_end(int bytes_proceed)	{ input_consumed_ = bytes_proceed; }

	uint32_t	handle();
	void	set_user_count(uint32_t count);

	void	set_public_ip(const char * ip)	{ public_ip_ = ip; }
	const char * public_ip()		{ return public_ip_.c_str(); }

	void	set_channel(uint8_t channel)	{ channel_ = channel; }
	uint8_t	channel()			{ return channel_; }

	void	set_listen_port(uint16_t port) { listen_port_ = port; }
	uint16_t	listen_port() { return listen_port_; }

	void	set_p2_p_port(uint16_t port);
	uint16_t	p2_p_port() { return p2_p_port_; }

	void	set_maps(int32_t* pl);
	int32_t *	maps() { return &maps_[0]; }

	bool	set_item_id_range(item_id_range_table item_range);
	bool	set_spare_item_id_range(item_id_range_table item_range);
	bool	check_item_id_range_collision(item_id_range_table item_range);
	void	send_spare_item_id_range();
	void	return_item_id_range();	// gives the peer's item ID range back for reuse

    private:
	std::shared_ptr<net::connection> connection_;
	std::span<const std::uint8_t> input_;	// the input being processed
	std::size_t	input_consumed_;

	int	state_;

	uint8_t	channel_;
	uint32_t	handle_;
	uint16_t	listen_port_;	// Port the game server listens on for clients
	uint16_t	p2_p_port_;	// Port the game server listens on for game server P2P connections
	int32_t	maps_[MAX_MAPS_PER_SERVER];	// Which maps does it manage?

	item_id_range_table item_range_;
	item_id_range_table item_spare_range_;

	std::string public_ip_;
};
}
