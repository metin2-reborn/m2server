// vim: ts=8 sw=4
#pragma once

#include <common/tables.h>
#include <libnet/connection.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <system_error>

// A game or auth server connected to the db. Its packets are handed to
// CClientManager::ProcessPackets as they arrive.
class CPeer : public net::connection_handler
{
    public:
#pragma pack(1)
	typedef struct _header
	{   
	    uint8_t    bHeader;
	    uint32_t   dwHandle;
	    uint32_t   dwSize;
	} HEADER;
#pragma pack()
	enum EState
	{
	    STATE_CLOSE = 0,
	    STATE_PLAYING = 1
	};

	explicit CPeer(std::shared_ptr<net::connection> connection);
	~CPeer() override;

	auto	on_receive(std::span<std::uint8_t> input, std::size_t received) -> std::size_t override;
	void	on_close(const std::error_code & error) override;

	// The connection ended; CClientManager removes the peer on its next pass.
	bool	IsClosed() const	{ return m_state == STATE_CLOSE; }
	const char *	GetHost() const	{ return m_connection->remote_address().c_str(); }

	void	EncodeBYTE(uint8_t b)		{ Encode(&b, sizeof(b)); }
	void	EncodeWORD(uint16_t w)		{ Encode(&w, sizeof(w)); }
	void	EncodeDWORD(uint32_t dw)	{ Encode(&dw, sizeof(dw)); }
	void	Encode(const void * data, uint32_t size);

	void	EncodeHeader(uint8_t header, uint32_t dwHandle, uint32_t dwSize);
	bool 	PeekPacket(int & iBytesProceed, uint8_t & header, uint32_t & dwHandle, uint32_t & dwLength, const char ** data);
	void	EncodeReturn(uint8_t header, uint32_t dwHandle);
	void	RecvEnd(int iBytesProceed)	{ m_inputConsumed = iBytesProceed; }

	uint32_t	GetHandle();
	void	SetUserCount(uint32_t dwCount);

	void	SetPublicIP(const char * ip)	{ m_stPublicIP = ip; }
	const char * GetPublicIP()		{ return m_stPublicIP.c_str(); }

	void	SetChannel(uint8_t bChannel)	{ m_bChannel = bChannel; }
	uint8_t	GetChannel()			{ return m_bChannel; }

	void	SetListenPort(uint16_t wPort) { m_wListenPort = wPort; }
	uint16_t	GetListenPort() { return m_wListenPort; }

	void	SetP2PPort(uint16_t wPort);
	uint16_t	GetP2PPort() { return m_wP2PPort; }

	void	SetMaps(int32_t* pl);
	int32_t *	GetMaps() { return &m_alMaps[0]; }

	bool	SetItemIDRange(TItemIDRangeTable itemRange);
	bool	SetSpareItemIDRange(TItemIDRangeTable itemRange);
	bool	CheckItemIDRangeCollision(TItemIDRangeTable itemRange);
	void	SendSpareItemIDRange();
	void	ReturnItemIDRange();	// gives the peer's item ID range back for reuse

    private:
	std::shared_ptr<net::connection> m_connection;
	std::span<const std::uint8_t> m_input;	// the input being processed
	std::size_t	m_inputConsumed;

	int	m_state;

	uint8_t	m_bChannel;
	uint32_t	m_dwHandle;
	uint16_t	m_wListenPort;	// Port the game server listens on for clients
	uint16_t	m_wP2PPort;	// Port the game server listens on for game server P2P connections
	int32_t	m_alMaps[MAX_MAPS_PER_SERVER];	// Which maps does it manage?

	TItemIDRangeTable m_itemRange;
	TItemIDRangeTable m_itemSpareRange;

	std::string m_stPublicIP;
};
