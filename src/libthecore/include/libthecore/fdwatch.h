#ifndef __INC_LIBTHECORE_FDWATCH_H__
#define __INC_LIBTHECORE_FDWATCH_H__

#include <libthecore/typedef.h>

#include <sys/time.h>


// Readiness notification for sockets (epoll). Opaque; see fdwatch.cpp.
struct fdwatch;

enum EFdwatch { FDW_READ = 1, FDW_WRITE = 2, FDW_WRITE_ONESHOT = 4, FDW_EOF = 8 };

fdwatch * fdwatch_new(int nfiles);
void fdwatch_delete(fdwatch * fdw);
int fdwatch_check_event(fdwatch * fdw, socket_t fd, unsigned int event_idx);
void fdwatch_clear_event(fdwatch * fdw, socket_t fd, unsigned int event_idx);
void fdwatch_add_fd(fdwatch * fdw, socket_t fd, void *client_data, int rw, bool oneshot);
int fdwatch_wait(fdwatch * fdw, struct timeval *timeout);
void *fdwatch_get_client_data(fdwatch * fdw, unsigned int event_idx);
void fdwatch_del_fd(fdwatch * fdw, socket_t fd);
int fdwatch_get_buffer_size(fdwatch * fdw, socket_t fd);
int fdwatch_get_ident(fdwatch * fdw, unsigned int event_idx);

#endif
