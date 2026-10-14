/*
 *    Filename: socket.cpp
 * Description: socket-related source.
 *
 *      Author: Biyeop aka. Cronan
 */
#include <asm-generic/socket.h>
#include <libthecore/socket.h>
#include <sys/socket.h>

#include <libthecore/log.h>
#include <libthecore/typedef.h>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

namespace
{
    // errno values that mean "try again later" rather than a broken connection.
    bool would_block(int err)
    {
	return err == EINTR || err == EAGAIN || err == EWOULDBLOCK || err == EDEADLK;
    }

    template <typename T>
    bool set_option(socket_t s, int name, const T & value, const char * what)
    {
	if (setsockopt(s, SOL_SOCKET, name, &value, sizeof(value)) < 0)
	{
	    sys_err("setsockopt: %s: %s", what, std::strerror(errno));
	    return false;
	}
	return true;
    }

    template <typename T>
    bool get_option(socket_t s, int name, T & value, const char * what)
    {
	socklen_t len = sizeof(value);
	if (getsockopt(s, SOL_SOCKET, name, &value, &len) < 0)
	{
	    sys_err("getsockopt: %s: %s", what, std::strerror(errno));
	    return false;
	}
	return true;
    }

    void socket_reuse(socket_t s)
    {
	if (!set_option(s, SO_REUSEADDR, 1, "reuse"))
	    socket_close(s);
    }

    void socket_keepalive(socket_t s)
    {
	if (!set_option(s, SO_KEEPALIVE, 1, "keepalive"))
	    socket_close(s);
    }

    void set_linger(socket_t s, bool on)
    {
	const linger value{on ? 1 : 0, 0};
	if (!set_option(s, SO_LINGER, value, "linger"))
	    socket_close(s);
    }

    void set_blocking(socket_t s, bool blocking)
    {
	int flags = fcntl(s, F_GETFL, 0);
	flags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);

	if (fcntl(s, F_SETFL, flags) < 0)
	    sys_err("fcntl: nonblock: %s", std::strerror(errno));
    }

    int socket_bind(const char * ip, int port, int protocol)
    {
	const int s = socket(AF_INET, protocol, 0);
	if (s < 0)
	{
	    sys_err("socket: %s", std::strerror(errno));
	    return -1;
	}

	socket_reuse(s);
	set_linger(s, false);

	sockaddr_in sa{};
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = inet_addr(ip);
	sa.sin_port = htons(static_cast<unsigned short>(port));

	if (bind(s, reinterpret_cast<const sockaddr *>(&sa), sizeof(sa)) < 0)
	{
	    sys_err("bind: %s", std::strerror(errno));
	    socket_close(s);
	    return -1;
	}

	socket_nonblock(s);

	if (protocol == SOCK_STREAM)
	{
	    sys_log(0, "SYSTEM: BINDING TCP PORT ON [%d] (fd %d)", port, s);
	    listen(s, SOMAXCONN);
	}
	else
	    sys_log(0, "SYSTEM: BINDING UDP PORT ON [%d] (fd %d)", port, s);

	return s;
    }

    bool resolve(const char * host, in_addr & addr)
    {
	if (inet_pton(AF_INET, host, &addr) == 1)
	    return true;

	addrinfo hints{};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	addrinfo * result = nullptr;
	if (getaddrinfo(host, nullptr, &hints, &result) != 0 || !result)
	    return false;

	addr = reinterpret_cast<const sockaddr_in *>(result->ai_addr)->sin_addr;
	freeaddrinfo(result);
	return true;
    }
}

// sec : seconds, usec : microseconds
void socket_timeout(socket_t s, int32_t sec, int32_t usec)
{
	timeval rcvopt{sec, usec};
	timeval sndopt{sec, usec};

	if (!set_option(s, SO_RCVTIMEO, rcvopt, "timeout") || !get_option(s, SO_RCVTIMEO, rcvopt, "timeout") ||
	    !set_option(s, SO_SNDTIMEO, sndopt, "timeout") || !get_option(s, SO_SNDTIMEO, sndopt, "timeout"))
	{
	    socket_close(s);
	    return;
	}

	sys_log(1, "SYSTEM: %d: TIMEOUT RCV: %ld.%ld, SND: %ld.%ld", s, rcvopt.tv_sec, rcvopt.tv_usec, sndopt.tv_sec, sndopt.tv_usec);
}

int socket_read(socket_t desc, char* read_point, size_t space_left)
{
    const ssize_t ret = recv(desc, read_point, space_left, 0);

    if (ret > 0)
	return static_cast<int>(ret);

    if (ret == 0)	// Disconnected normally
	return -1;

    if (would_block(errno))
	return 0;

    sys_err("about to lose connection");
    return -1;
}

int socket_write_tcp(socket_t desc, const char *txt, int length)
{
    const ssize_t bytes_written = send(desc, txt, length, MSG_NOSIGNAL);

    // Success
    if (bytes_written > 0)
	return static_cast<int>(bytes_written);

    if (bytes_written == 0)
	return -1;

    if (would_block(errno))
	return 0;

    // Looks like the error was fatal.  Too bad.
    return -1;
}

int socket_write(socket_t desc, const char *data, size_t length)
{
    size_t total = length;

    do
    {
	const int bytes_written = socket_write_tcp(desc, data, static_cast<int>(total));
	if (bytes_written <= 0)
	{
	    if (errno == EWOULDBLOCK || errno == EAGAIN)
		sys_err("socket write would block, about to close!");
	    else
		sys_err("write to desc error");   // 'Usually' means the connection was closed by the other side.

	    return -1;
	}

	data += bytes_written;
	total -= bytes_written;
    }
    while (total > 0);

    return 0;
}

int socket_tcp_bind(const char * ip, int port)
{
    return socket_bind(ip, port, SOCK_STREAM);
}

void socket_close(socket_t s)
{
    close(s);
}

socket_t socket_accept(socket_t s, struct sockaddr_in *peer)
{
    socklen_t len = sizeof(*peer);

    const socket_t desc = accept(s, reinterpret_cast<sockaddr *>(peer), &len);
    if (desc == -1)
    {
	sys_err("accept: %s (fd %d)", std::strerror(errno), s);
	return -1;
    }

    if (desc >= 65500)
    {
	sys_err("SOCKET FD 65500 LIMIT! %d", desc);
	socket_close(desc);
	return -1;
    }

    socket_nonblock(desc);
    set_linger(desc, false);
    return desc;
}

socket_t socket_connect(const char* host, uint16_t port)
{
    sockaddr_in server_addr{};

    if (!resolve(host, server_addr.sin_addr))
    {
	sys_err("socket_connect(): can not connect to %s:%d", host, port);
	return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    const socket_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
    {
	std::perror("socket");
	return -1;
    }

    socket_keepalive(s);
    socket_sndbuf(s, 233016);
    socket_rcvbuf(s, 233016);
    socket_timeout(s, 10, 0);
    set_linger(s, true);

    // Connection request
    if (connect(s, reinterpret_cast<const sockaddr *>(&server_addr), sizeof(server_addr)) < 0)
    {
	const int err = errno;
	socket_close(s);

	switch (err)
	{
	    case EINTR:
	    case ETIMEDOUT:
		sys_err("HOST %s:%d connection timeout.", host, port);
		break;
	    case ECONNREFUSED:
		sys_err("HOST %s:%d port is not opened. connection refused.", host, port);
		break;
	    case ENETUNREACH:
		sys_err("HOST %s:%d is not reachable from this host.", host, port);
		break;
	    default:
		sys_err("HOST %s:%d, could not connect: %s", host, port, std::strerror(err));
		break;
	}
	return -1;
    }

    return s;
}

void socket_nonblock(socket_t s)
{
    set_blocking(s, false);
}

void socket_block(socket_t s)
{
    set_blocking(s, true);
}

void socket_rcvbuf(socket_t s, unsigned int opt)
{
    if (!set_option(s, SO_RCVBUF, opt, "rcvbuf"))
    {
	socket_close(s);
	return;
    }

    opt = 0;
    if (!get_option(s, SO_RCVBUF, opt, "rcvbuf"))
    {
	socket_close(s);
	return;
    }

    sys_log(1, "SYSTEM: %d: receive buffer changed to %u", s, opt);
}

void socket_sndbuf(socket_t s, unsigned int opt)
{
    if (!set_option(s, SO_SNDBUF, opt, "sndbuf"))
	return;

    opt = 0;
    if (!get_option(s, SO_SNDBUF, opt, "sndbuf"))
	return;

    sys_log(1, "SYSTEM: %d: send buffer changed to %u", s, opt);
}
