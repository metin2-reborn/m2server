#ifndef __INC_METIN_II_GAME_BUFFER_MANAGER_H__
#define __INC_METIN_II_GAME_BUFFER_MANAGER_H__

#include <libthecore/buffer.h>

class TEMP_BUFFER
{
	public:
		TEMP_BUFFER(int Size = 8192, bool ForceDelete = false );
		~TEMP_BUFFER();

		const void * 	read_peek();
		void		write(const void * data, int size);
		int		size();

		buffer *	getptr() { return buf; }

	protected:
		buffer *	buf;
		bool		forceDelete;
};

#endif
