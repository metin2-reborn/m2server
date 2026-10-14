// vim: ts=8 sw=4
#ifndef __INC_PEERBASE_H__
#define __INC_PEERBASE_H__

#include "NetBase.h"

#include <libthecore/buffer.h>
#include <libthecore/typedef.h>

#include <cstdint>

class CPeerBase : public CNetBase
{
    public:
	enum
	{ 
	    MAX_HOST_LENGTH		= 30,
	    MAX_INPUT_LEN		= 1024 * 1024 * 2,
	    DEFAULT_PACKET_BUFFER_SIZE	= 1024 * 1024 * 2
	};

    protected:
	virtual void	OnAccept() = 0;
	virtual void	OnClose() = 0;

    public:
	bool		Accept(socket_t accept_fd);
	void		Close();

    public:
	CPeerBase();
	~CPeerBase() override;

	void		Disconnect();
	void		Destroy();

	socket_t	GetFd() { return m_fd; }

	void		EncodeBYTE(uint8_t b);
	void		EncodeWORD(uint16_t w);
	void		EncodeDWORD(uint32_t dw);
	void		Encode(const void* data, uint32_t size);
	int		Send();

	int		Recv();
	void		RecvEnd(int proceed_bytes);
	int		GetRecvLength();
	const void *	GetRecvBuffer();

	int		GetSendLength();

    protected:
	char		m_host[MAX_HOST_LENGTH + 1];
	socket_t	m_fd;

    private:
	int		m_BytesRemain;
	buffer *	m_outBuffer;
	buffer *	m_inBuffer;
};

#endif
