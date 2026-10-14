#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include <libthecore/buffer.h>

#include <cassert>
#include <cstdint>

inline const char *encode_byte(char ind)
{
	static char a[8];
	*((char *) a) = ind;
	return (a);
}

inline const char *encode_4bytes(int ind)
{
	static char a[8];
	*((int *) a) = ind;
	return (a);
}

inline uint8_t decode_byte(const void * a)
{
	return (*(uint8_t *) a);
}

inline uint16_t decode_2bytes(const void * a)
{
	return (*((uint16_t *) a));
}

inline int32_t decode_4bytes(const void *a)
{
	return (*((int32_t *) a));
}

inline constexpr int DEFAULT_PACKET_BUFFER_SIZE = 65536;
inline bool packet_encode(buffer * pbuf, const void * data, int length)
{
	assert(pbuf != nullptr);
	assert(data != nullptr);

	if (buffer_has_space(pbuf) < length)
	{
		return false;
	}

	buffer_write(pbuf, data, length);
	return true;
}

#endif
