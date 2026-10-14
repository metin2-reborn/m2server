#include <libthecore/fdwatch.h>

#include <libthecore/log.h>
#include <libthecore/typedef.h>

#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstring>
#include <new>
#include <vector>

#include <sys/epoll.h>
#include <sys/time.h>
#include <unistd.h>

namespace
{
	struct watched_fd
	{
		void *		data = nullptr;
		unsigned	generation = 0;	// bumped on removal, so stale events are dropped
		int		interests = 0;
		bool		registered = false;
	};

	struct ready_fd
	{
		int		fd;
		int		flags;
		unsigned	generation;
	};
}

struct fdwatch
{
	int				epfd = -1;
	std::vector<watched_fd>		fds;
	std::vector<epoll_event>	events;
	std::vector<ready_fd>		ready;

	~fdwatch()
	{
		if (epfd >= 0)
			close(epfd);
	}

	bool valid(int fd) const { return fd >= 0 && fd < static_cast<int>(fds.size()); }

	// Bring the epoll registration of fd in line with its interests.
	bool update(int fd)
	{
		watched_fd & f = fds[fd];

		if (!(f.interests & (FDW_READ | FDW_WRITE)))
		{
			if (f.registered && epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr) < 0 && errno != ENOENT && errno != EBADF)
				return false;
			f.registered = false;
			return true;
		}

		epoll_event e{};
		if (f.interests & FDW_READ)
			e.events |= EPOLLIN | EPOLLRDHUP;
		if (f.interests & FDW_WRITE)
			e.events |= EPOLLOUT;
		e.data.u64 = (static_cast<uint64_t>(f.generation) << 32) | static_cast<uint32_t>(fd);

		if (epoll_ctl(epfd, f.registered ? EPOLL_CTL_MOD : EPOLL_CTL_ADD, fd, &e) < 0)
			return false;
		f.registered = true;
		return true;
	}

	// The ready entry at idx, when it still refers to the descriptor that produced it.
	ready_fd * ready_at(unsigned idx)
	{
		if (idx >= ready.size())
			return nullptr;
		ready_fd & r = ready[idx];
		if (!valid(r.fd) || r.generation != fds[r.fd].generation)
			return nullptr;
		return &r;
	}
};

fdwatch * fdwatch_new(int nfiles)
{
	if (nfiles <= 0 || nfiles > INT_MAX / 2)
	{
		errno = EINVAL;
		return nullptr;
	}

	auto w = new (std::nothrow) struct fdwatch;
	if (!w)
		return nullptr;

	w->epfd = epoll_create1(EPOLL_CLOEXEC);
	if (w->epfd < 0)
	{
		delete w;
		return nullptr;
	}

	w->fds.resize(nfiles);
	w->events.resize(nfiles);
	w->ready.reserve(static_cast<size_t>(nfiles) * 2);
	return w;
}

void fdwatch_delete(fdwatch * w)
{
	delete w;
}

void fdwatch_add_fd(fdwatch * w, socket_t fd, void * data, int rw, bool oneshot)
{
	if (!w || !w->valid(fd))
	{
		sys_err("fdwatch: descriptor %d exceeds capacity", fd);
		return;
	}

	watched_fd & f = w->fds[fd];
	const int previous = f.interests;

	f.data = data;
	f.interests |= rw & (FDW_READ | FDW_WRITE);
	if (rw & FDW_WRITE)
	{
		if (oneshot)
			f.interests |= FDW_WRITE_ONESHOT;
		else
			f.interests &= ~FDW_WRITE_ONESHOT;
	}

	if (!w->update(fd))
	{
		f.interests = previous;
		sys_err("epoll_ctl(%d): %s", fd, std::strerror(errno));
	}
}

void fdwatch_del_fd(fdwatch * w, socket_t fd)
{
	if (!w || !w->valid(fd))
		return;

	watched_fd & f = w->fds[fd];
	f.interests = 0;
	w->update(fd);
	f.data = nullptr;
	++f.generation;
}

int fdwatch_wait(fdwatch * w, struct timeval * timeout)
{
	int ms = 0;
	if (timeout)
	{
		const int64_t total = static_cast<int64_t>(timeout->tv_sec) * 1000 + (timeout->tv_usec + 999) / 1000;
		ms = total > INT_MAX ? INT_MAX : (total < 0 ? 0 : static_cast<int>(total));
	}

	w->ready.clear();

	const int n = epoll_wait(w->epfd, w->events.data(), static_cast<int>(w->events.size()), ms);
	if (n < 0)
		return errno == EINTR ? 0 : -1;

	for (int i = 0; i < n; ++i)
	{
		const epoll_event & e = w->events[i];
		const int fd = static_cast<int>(static_cast<uint32_t>(e.data.u64));
		const unsigned gen = static_cast<unsigned>(e.data.u64 >> 32);

		if (!w->valid(fd) || gen != w->fds[fd].generation)
			continue;

		int flags = 0;
		if (e.events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
			flags = FDW_EOF;
		else
		{
			if ((e.events & EPOLLIN) && (w->fds[fd].interests & FDW_READ))
				flags |= FDW_READ;
			if ((e.events & EPOLLOUT) && (w->fds[fd].interests & FDW_WRITE))
				flags |= FDW_WRITE;
		}

		for (int bit = FDW_READ; bit <= FDW_EOF; bit <<= 1)
		{
			if (!(flags & bit) || bit == FDW_WRITE_ONESHOT)
				continue;
			w->ready.push_back({fd, bit, gen});
		}
	}

	return static_cast<int>(w->ready.size());
}

int fdwatch_check_event(fdwatch * w, socket_t fd, unsigned idx)
{
	ready_fd * r = w ? w->ready_at(idx) : nullptr;
	if (!r || r->fd != fd)
		return 0;

	if (r->flags == FDW_WRITE && (w->fds[fd].interests & FDW_WRITE_ONESHOT))
	{
		w->fds[fd].interests &= ~(FDW_WRITE | FDW_WRITE_ONESHOT);
		w->update(fd);
	}
	return r->flags;
}

void fdwatch_clear_event(fdwatch * w, socket_t fd, unsigned idx)
{
	ready_fd * r = w ? w->ready_at(idx) : nullptr;
	if (r && r->fd == fd)
		r->flags = 0;
}

void * fdwatch_get_client_data(fdwatch * w, unsigned idx)
{
	ready_fd * r = w ? w->ready_at(idx) : nullptr;
	return r ? w->fds[r->fd].data : nullptr;
}

int fdwatch_get_ident(fdwatch * w, unsigned idx)
{
	ready_fd * r = w ? w->ready_at(idx) : nullptr;
	return r ? r->fd : -1;
}

int fdwatch_get_buffer_size(fdwatch * w, socket_t fd)
{
	if (!w || !w->valid(fd))
		return 0;
	// Linux does not expose kqueue's exact writable-byte count. Use an advisory budget;
	// callers advance only by the bytes send() accepted.
	return 65536;
}
