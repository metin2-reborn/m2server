/*
 *    Filename: buffer.h
 * Description: Buffer handling module
 *
 *      Author: Kim Han-ju (aka. Biyeop, Cronan), Song Young-jin (aka. myevan, Bitjaru)
 */
#ifndef __INC_LIBTHECORE_BUFFER_H__
#define __INC_LIBTHECORE_BUFFER_H__

#include <cstdint>

// A growable byte queue: written at the end, read from the front. Opaque; see buffer.cpp.
struct buffer;

buffer *	buffer_new(int size);				// Create a new buffer
void		buffer_delete(buffer * buffer);			// Delete the buffer (nullptr is ignored)
void		buffer_reset(buffer * buffer);			// Reset the buffer lengths
uint32_t		buffer_size(buffer * buffer);			// Length remaining in the buffer
int		buffer_has_space(buffer * buffer);		// Returns the writable length

void		buffer_write(buffer * & buffer, const void * src, int length);	// Write to the buffer.

const void *	buffer_read_peek(buffer * buffer);		// Returns the read position
void		buffer_read_proceed(buffer * buffer, int length);	// length bytes have been processed

void *		buffer_write_peek(buffer * buffer);		// Returns the write position
void		buffer_write_proceed(buffer * buffer, int length);	// Only increases length.

void		buffer_adjust_size(buffer * & buffer, int add_size);	// Reserve add_size additional bytes

#endif
