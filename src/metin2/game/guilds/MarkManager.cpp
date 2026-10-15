#include <metin2/game/guilds/MarkManager.h>

#include <metin2/game/app/crc32.h>
#include <metin2/game/guilds/MarkImage.h>

#include <metin2/log/log.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

namespace metin2::game::inline guilds {

guild_mark_manager::guild_mark_manager()
{
	// Build the set of remaining mark ids. (server only)
	for (uint32_t i = 0; i < MAX_IMAGE_COUNT * static_cast<uint32_t>(guild_mark_image::MARK_TOTAL_COUNT); ++i)
		free_mark_id_.insert(i);
}

guild_mark_manager::~guild_mark_manager()
{
}

bool guild_mark_manager::mark_image_filename(uint32_t imgIdx, std::string & path) const
{
	if (imgIdx >= MAX_IMAGE_COUNT)
		return false;

	char buf[64];
	snprintf(buf, sizeof(buf), "mark/%s_%u.tga", path_prefix_.c_str(), imgIdx);
	path = buf;
	return true;
}

void guild_mark_manager::set_mark_path_prefix(const char * prefix)
{
	path_prefix_ = prefix;
}

// Load the mark index (used only on the server)
bool guild_mark_manager::load_mark_index()
{
	char buf[64];
	snprintf(buf, sizeof(buf), "mark/%s_index", path_prefix_.c_str());
	FILE * fp = fopen(buf, "r");

	if (!fp)
		return false;

	uint32_t guildID;
	uint32_t markID;

	char line[256];

	while (fgets(line, sizeof(line)-1, fp))
	{
		sscanf(line, "%u %u", &guildID, &markID);
		line[0] = '\0';
		add_mark_id_by_guild_id(guildID, markID);
	}

	load_mark_images();

	fclose(fp);
	return true;
}

bool guild_mark_manager::save_mark_index()
{
	char buf[64];
	snprintf(buf, sizeof(buf), "mark/%s_index", path_prefix_.c_str());
	FILE * fp = fopen(buf, "w");

	if (!fp)
	{
		metin2::log::error("MarkManager::SaveMarkIndex: cannot open index file.");
		return false;
	}

	for (std::map<uint32_t, uint32_t>::iterator it = gid_mark_id_.begin(); it != gid_mark_id_.end(); ++it)
		fprintf(fp, "%u %u\n", it->first, it->second);

	fclose(fp);
	metin2::log::info( "MarkManager::SaveMarkIndex: index count {:d}", gid_mark_id_.size());
	return true;
}

void guild_mark_manager::load_mark_images()
{
	bool isMarkExists[MAX_IMAGE_COUNT];
	memset(isMarkExists, 0, sizeof(isMarkExists));

	for (std::map<uint32_t, uint32_t>::iterator it = gid_mark_id_.begin(); it != gid_mark_id_.end(); ++it)
	{
		uint32_t markID = it->second;

		if (markID < MAX_IMAGE_COUNT * static_cast<uint32_t>(guild_mark_image::MARK_TOTAL_COUNT))
			isMarkExists[markID / guild_mark_image::MARK_TOTAL_COUNT] = true;
	}

	for (uint32_t i = 0; i < MAX_IMAGE_COUNT; ++i)
		if (isMarkExists[i])
			image(i);
}

void guild_mark_manager::save_mark_image(uint32_t imgIdx)
{
	std::string path;

	if (mark_image_filename(imgIdx, path))
		if (!image(imgIdx)->save(path.c_str()))
			metin2::log::error("{} Save failed", path.c_str());
}

guild_mark_image * guild_mark_manager::image(uint32_t imgIdx)
{
	auto it = idx_image_.find(imgIdx);

	if (it == idx_image_.end())
	{
		std::string imagePath;

		if (mark_image_filename(imgIdx, imagePath))
		{
			guild_mark_image * pkImage = idx_image_.emplace(imgIdx, std::make_unique<guild_mark_image>()).first->second.get();
			
			if (!pkImage->load(imagePath.c_str()))
			{
				pkImage->build(imagePath.c_str());
				pkImage->load(imagePath.c_str());
			}

			return pkImage;
		}
		else
			return nullptr;
	}
	else
		return it->second.get();
}

bool guild_mark_manager::add_mark_id_by_guild_id(uint32_t guildID, uint32_t markID)
{
	if (markID >= MAX_IMAGE_COUNT * static_cast<uint32_t>(guild_mark_image::MARK_TOTAL_COUNT))
		return false;

	gid_mark_id_.insert(std::map<uint32_t, uint32_t>::value_type(guildID, markID));
	free_mark_id_.erase(markID);
	return true;
}

uint32_t guild_mark_manager::mark_id(uint32_t guildID)
{
	std::map<uint32_t, uint32_t>::iterator it = gid_mark_id_.find(guildID);

	if (it == gid_mark_id_.end())
		return INVALID_MARK_ID;

	return it->second;
}

uint32_t guild_mark_manager::alloc_mark_id(uint32_t guildID)
{
	std::set<uint32_t>::iterator it = free_mark_id_.lower_bound(0);

	if (it == free_mark_id_.end())
		return INVALID_MARK_ID;

	uint32_t markID = *it;
	
	uint32_t imgIdx = markID / guild_mark_image::MARK_TOTAL_COUNT;
	guild_mark_image * pkImage = image(imgIdx); // To create the image if it doesn't exist 

	if (pkImage && add_mark_id_by_guild_id(guildID, markID))
		return markID;

	return INVALID_MARK_ID;
}

uint32_t guild_mark_manager::mark_count() const
{
	return gid_mark_id_.size();
}

// SERVER
void guild_mark_manager::copy_mark_idx(char * pcBuf) const
{
	uint16_t * pwBuf = (uint16_t *) pcBuf;

	for (std::map<uint32_t, uint32_t>::const_iterator it = gid_mark_id_.begin(); it != gid_mark_id_.end(); ++it)
	{
		*(pwBuf++) = it->first; // guild id
		*(pwBuf++) = it->second; // mark id
	}
}

// SERVER
uint32_t guild_mark_manager::save_mark(uint32_t guildID, uint8_t * pbMarkImage)
{
	uint32_t idMark;

	if ((idMark = mark_id(guildID)) == INVALID_MARK_ID)
	{
		if ((idMark = alloc_mark_id(guildID)) == INVALID_MARK_ID)
		{
			metin2::log::error("CGuildMarkManager: cannot alloc mark id {:d}", guildID);
			return false;
		}
		else
			metin2::log::info( "SaveMark: mark id alloc {:d}", idMark);
	}
	else
		metin2::log::info( "SaveMark: mark id found {:d}", idMark);

	uint32_t imgIdx = (idMark / guild_mark_image::MARK_TOTAL_COUNT);
	guild_mark_image * pkImage = image(imgIdx);

	if (pkImage)
	{
		pkImage->save_mark(idMark % guild_mark_image::MARK_TOTAL_COUNT, pbMarkImage);

		save_mark_image(imgIdx);
		save_mark_index();
	}

	return idMark;
}

// SERVER
void guild_mark_manager::delete_mark(uint32_t guildID)
{
	std::map<uint32_t, uint32_t>::iterator it = gid_mark_id_.find(guildID);

	if (it == gid_mark_id_.end())
		return;

	guild_mark_image * pkImage;

	if ((pkImage = image(it->second / guild_mark_image::MARK_TOTAL_COUNT)) != nullptr)
		pkImage->delete_mark(it->second % guild_mark_image::MARK_TOTAL_COUNT);

	free_mark_id_.insert(it->second);
	gid_mark_id_.erase(it);

	save_mark_index();
}

// SERVER
void guild_mark_manager::diff_blocks(uint32_t imgIdx, const uint32_t * crcList, std::map<uint8_t, const guild_mark_block *> & mapDiffBlocks)
{
	mapDiffBlocks.clear();

	// The client cannot request an image the server doesn't have.
	if (idx_image_.end() == idx_image_.find(imgIdx))
	{
		metin2::log::error("invalid idx {:d}", imgIdx);
		return;
	}

	guild_mark_image * p = image(imgIdx);

	if (p)
		p->diff_blocks(crcList, mapDiffBlocks);
}

///////////////////////////////////////////////////////////////////////////////////////
// Symbol
///////////////////////////////////////////////////////////////////////////////////////
const guild_mark_manager::guild_symbol * guild_mark_manager::get_guild_symbol(uint32_t guildID)
{
	std::map<uint32_t, guild_symbol>::iterator it = symbol_.find(guildID);

	if (it == symbol_.end())
		return nullptr;

	return &it->second;
}

bool guild_mark_manager::load_symbol(const char* filename)
{
	FILE* fp = fopen(filename, "rb");

	if (!fp)
		return true;
	else
	{
		uint32_t symbolCount;
		fread(&symbolCount, 4, 1, fp);

		for (uint32_t i = 0; i < symbolCount; i++)
		{
			uint32_t guildID;
			uint32_t dwSize;
			fread(&guildID, 4, 1, fp);
			fread(&dwSize, 4, 1, fp);

			guild_symbol gs;
			gs.raw.resize(dwSize);
			fread(&gs.raw[0], 1, dwSize, fp);
			gs.crc = get_cr_c32(reinterpret_cast<const char*>(&gs.raw[0]), dwSize);
			symbol_.insert(std::make_pair(guildID, gs));
		}
	}

	fclose(fp);
	return true;
}

void guild_mark_manager::save_symbol(const char* filename)
{
	FILE* fp = fopen(filename, "wb");
	if (!fp)
	{
		metin2::log::error("Cannot open Symbol file (name: {})", filename);
		return;
	}

	uint32_t symbolCount = symbol_.size();
	fwrite(&symbolCount, 4, 1, fp);

	for (std::map<uint32_t, guild_symbol>::iterator it = symbol_.begin(); it != symbol_.end(); ++it)
	{
		uint32_t guildID = it->first;
		uint32_t dwSize = it->second.raw.size();
		fwrite(&guildID, 4, 1, fp);
		fwrite(&dwSize, 4, 1, fp);
		fwrite(&it->second.raw[0], 1, dwSize, fp);
	}

	fclose(fp);
}

void guild_mark_manager::upload_symbol(uint32_t guildID, int iSize, const uint8_t* pbyData)
{
	metin2::log::info( "GuildSymbolUpload guildID {:d} Size {:d}", guildID, iSize);
	
	if (symbol_.find(guildID) == symbol_.end())
		symbol_.insert(std::make_pair(guildID, guild_symbol()));

	guild_symbol& rSymbol = symbol_[guildID];
	rSymbol.raw.clear();

	if (iSize > 0)
	{
		rSymbol.raw.reserve(iSize);
		std::copy(pbyData, (pbyData + iSize), std::back_inserter(rSymbol.raw));
		rSymbol.crc = get_cr_c32(reinterpret_cast<const char*>(pbyData), iSize);
	}
}
}
