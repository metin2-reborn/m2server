/*
 *    Filename: socket.h
 * Description: socket-related function header.
 *
 *      Author: Biyeop aka. Cronan
 */
#ifndef __INC_LIBTHECORE_SOCKET_H__
#define __INC_LIBTHECORE_SOCKET_H__

#include <libthecore/typedef.h>

#include <cstddef>
#include <cstdint>

#include <netinet/in.h>

constexpr socket_t INVALID_SOCKET = -1;

int		socket_write_tcp(socket_t desc, const char *data, int length);
int		socket_read(socket_t desc, char* read_point, size_t space_left);
int		socket_write(socket_t desc, const char *data, size_t length);

int		socket_tcp_bind(const char * ip, int port);
socket_t	socket_accept(socket_t s, struct sockaddr_in *peer);
void		socket_close(socket_t s);
socket_t	socket_connect(const char* host, uint16_t port);

void		socket_nonblock(socket_t s);
void		socket_block(socket_t s);
void		socket_sndbuf(socket_t s, unsigned int opt);
void		socket_rcvbuf(socket_t s, unsigned int opt);
void		socket_timeout(socket_t s, int32_t sec, int32_t usec);

#endif
