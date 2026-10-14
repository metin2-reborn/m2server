/*
 *    Filename: buffer.cpp
 * Description: Buffer handling module
 *
 *      Author: Kim Han-ju (aka. Biyeop, Cronan)
 */
#include <libthecore/buffer.h>

#include <libthecore/log.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <vector>

struct buffer
{
	std::unique_ptr<char[]>	data;
	int			capacity = 0;
	int			read_pos = 0;	// next byte to read
	int			write_pos = 0;	// next byte to write; read_pos..write_pos holds the content
};

namespace
{
	// Released buffers whose capacity is a power of two are kept for reuse, one list per size.
	constexpr int POOL_COUNT = 32;
	std::array<std::vector<buffer *>, POOL_COUNT> g_pool;

	// Smallest power-of-two index that fits size, or -1 when too big to pool.
	int pool_index_for(int size)
	{
		for (int i = 0; i < POOL_COUNT; ++i)
			if ((1 << i) >= size)
				return i;
		return -1;
	}

	// Index of a capacity that is exactly a power of two, or -1.
	int pool_index_of(int capacity)
	{
		for (int i = 0; i < POOL_COUNT; ++i)
			if ((1 << i) == capacity)
				return i;
		return -1;
	}

	void pool_clear()
	{
		for (auto & list : g_pool)
		{
			for (buffer * b : list)
				delete b;
			list.clear();
		}
	}

	std::unique_ptr<char[]> allocate(int size)
	{
		try
		{
			return std::make_unique<char[]>(size);
		}
		catch (const std::bad_alloc &)
		{
			// Pooled buffers may be holding the memory: release them and try once more.
			pool_clear();
			sys_err("buffer allocation of %d bytes failed; released the buffer pool", size);
			return std::make_unique<char[]>(size);
		}
	}

	void buffer_realloc(buffer * & buf, int capacity)
	{
		assert(capacity >= 0 && "buffer_realloc: length is lower than zero");

		if (buf->capacity >= capacity)
			return;

		buffer * grown = buffer_new(capacity);
		sys_log(0, "reallocating buffer to %d, current %d", grown->capacity, buf->capacity);
		std::memcpy(grown->data.get(), buf->data.get(), buf->capacity);
		grown->read_pos = buf->read_pos;
		grown->write_pos = buf->write_pos;

		buffer_delete(buf);
		buf = grown;
	}
}

buffer * buffer_new(int size)
{
	if (size < 0)
		return nullptr;

	const int index = pool_index_for(size);
	if (index >= 0)
	{
		size = 1 << index;
		if (!g_pool[index].empty())
		{
			buffer * b = g_pool[index].back();
			g_pool[index].pop_back();
			buffer_reset(b);
			return b;
		}
	}

	auto b = new buffer;
	b->data = allocate(size);
	b->capacity = size;
	return b;
}

void buffer_delete(buffer * buf)
{
	if (buf == nullptr)
		return;

	buffer_reset(buf);

	const int index = pool_index_of(buf->capacity);
	if (index >= 0)
		g_pool[index].push_back(buf);
	else
		delete buf;
}

uint32_t buffer_size(buffer * buf)
{
	return buf->write_pos - buf->read_pos;
}

void buffer_reset(buffer * buf)
{
	buf->read_pos = 0;
	buf->write_pos = 0;
}

void buffer_write(buffer * & buf, const void * src, int length)
{
	if (buf->write_pos + length >= buf->capacity)
		buffer_realloc(buf, buf->capacity + length + std::min(10240, length));

	std::memcpy(buf->data.get() + buf->write_pos, src, length);
	buffer_write_proceed(buf, length);
}

const void * buffer_read_peek(buffer * buf)
{
	return buf->data.get() + buf->read_pos;
}

void buffer_read_proceed(buffer * buf, int length)
{
	if (length == 0)
		return;

	const int content = buf->write_pos - buf->read_pos;

	if (length < 0)
		sys_err("buffer_proceed: length argument lower than zero (length: %d)", length);
	else if (length > content)
	{
		sys_err("buffer_proceed: length argument bigger than buffer (length: %d, buffer: %d)", length, content);
		length = content;
	}

	// If the processed length is less than the buffer length, the buffer must be kept.
	if (length < content)
	{
		if (buf->read_pos + length > buf->capacity)
		{
			sys_err("buffer_read_proceed: buffer overflow! length %d read_point %d", length, buf->read_pos);
			std::abort();
		}

		buf->read_pos += length;
	}
	else
	{
		buffer_reset(buf);
	}
}

void * buffer_write_peek(buffer * buf)
{
	return buf->data.get() + buf->write_pos;
}

void buffer_write_proceed(buffer * buf, int length)
{
	buf->write_pos += length;
}

int buffer_has_space(buffer * buf)
{
	return buf->capacity - buf->write_pos;
}

void buffer_adjust_size(buffer * & buf, int add_size)
{
	if (buf->capacity >= buf->write_pos + add_size)
		return;

	sys_log(0, "buffer_adjust %d current %d/%d", add_size, buf->write_pos - buf->read_pos, buf->capacity);
	buffer_realloc(buf, buf->capacity + add_size);
}
