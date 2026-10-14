#include "MarkImage.h"
#include "MarkManager.h"

#include <libthecore/log.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

constexpr const char OLD_MARK_INDEX_FILENAME[] = "guild_mark.idx";
constexpr const char OLD_MARK_DATA_FILENAME[] = "guild_mark.tga";
static Pixel * LoadOldGuildMarkImageFile()
{
	FILE * fp = fopen(OLD_MARK_DATA_FILENAME, "rb");

	if (!fp)
	{
		sys_err("cannot open %s", OLD_MARK_INDEX_FILENAME);
		return nullptr;
	}

	int dataSize = 512 * 512 * sizeof(Pixel);
	Pixel * dataPtr = (Pixel *) malloc(dataSize);

	fread(dataPtr, dataSize, 1, fp);

	fclose(fp);

	return dataPtr;
}

bool GuildMarkConvert(const std::vector<uint32_t> & vecGuildID)
{
	// create folder
	mkdir("mark", S_IRWXU);

	// does the index file exist? 
	if (0 != access(OLD_MARK_INDEX_FILENAME, F_OK))
		return true;

	// open index file
	FILE* fp = fopen(OLD_MARK_INDEX_FILENAME, "r");

	if (nullptr == fp)
		return false;

	// open image file
	Pixel * oldImagePtr = LoadOldGuildMarkImageFile();

	if (nullptr == oldImagePtr)
	{
		fclose(fp);
		return false;
	}

	sys_log(0, "Guild Mark Converting Start.");

	char line[256];
	uint32_t guild_id;
	uint32_t mark_id;
	Pixel mark[SGuildMark::SIZE];

	while (fgets(line, sizeof(line)-1, fp))
	{
		sscanf(line, "%u %u", &guild_id, &mark_id);

		if (find(vecGuildID.begin(), vecGuildID.end(), guild_id) == vecGuildID.end())
		{
			sys_log(0, "  skipping guild ID %u", guild_id);
			continue;
		}

		// mark id -> find position in image
		uint row = mark_id / 32;
		uint col = mark_id % 32;

		if (row >= 42)
		{
			sys_err("invalid mark_id %u", mark_id);
			continue;
		}

		uint sx = col * 16;
		uint sy = row * 12;

		Pixel * src = oldImagePtr + sy * 512 + sx;
		Pixel * dst = mark;

		// copy one mark from the old image
		for (int y = 0; y != SGuildMark::HEIGHT; ++y)
		{
			for (int x = 0; x != SGuildMark::WIDTH; ++x)
				*(dst++) = *(src+x);

			src += 512;
		}

		// put it into the new guild mark system.
		CGuildMarkManager::instance().SaveMark(guild_id, (uint8_t *) mark);
		line[0] = '\0';
	}

	free(oldImagePtr);
	fclose(fp);

	// conversion only needs to run once, so move the file.
	system("mv -f guild_mark.idx guild_mark.idx.removable");
	system("mv -f guild_mark.tga guild_mark.tga.removable");

	sys_log(0, "Guild Mark Converting Complete.");

	return true;
}

