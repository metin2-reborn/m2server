#include "MarkManager.h"

#include "crc32.h"
#include "MarkImage.h"

#include <libthecore/log.h>

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

CGuildMarkManager::CGuildMarkManager()
{
	// Build the set of remaining mark ids. (server only)
	for (uint32_t i = 0; i < MAX_IMAGE_COUNT * static_cast<uint32_t>(CGuildMarkImage::MARK_TOTAL_COUNT); ++i)
		m_setFreeMarkID.insert(i);
}

CGuildMarkManager::~CGuildMarkManager()
{
}

bool CGuildMarkManager::GetMarkImageFilename(uint32_t imgIdx, std::string & path) const
{
	if (imgIdx >= MAX_IMAGE_COUNT)
		return false;

	char buf[64];
	snprintf(buf, sizeof(buf), "mark/%s_%u.tga", m_pathPrefix.c_str(), imgIdx);
	path = buf;
	return true;
}

void CGuildMarkManager::SetMarkPathPrefix(const char * prefix)
{
	m_pathPrefix = prefix;
}

// Load the mark index (used only on the server)
bool CGuildMarkManager::LoadMarkIndex()
{
	char buf[64];
	snprintf(buf, sizeof(buf), "mark/%s_index", m_pathPrefix.c_str());
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
		AddMarkIDByGuildID(guildID, markID);
	}

	LoadMarkImages();

	fclose(fp);
	return true;
}

bool CGuildMarkManager::SaveMarkIndex()
{
	char buf[64];
	snprintf(buf, sizeof(buf), "mark/%s_index", m_pathPrefix.c_str());
	FILE * fp = fopen(buf, "w");

	if (!fp)
	{
		sys_err("MarkManager::SaveMarkIndex: cannot open index file.");
		return false;
	}

	for (std::map<uint32_t, uint32_t>::iterator it = m_mapGID_MarkID.begin(); it != m_mapGID_MarkID.end(); ++it)
		fprintf(fp, "%u %u\n", it->first, it->second);

	fclose(fp);
	sys_log(0, "MarkManager::SaveMarkIndex: index count %zu", m_mapGID_MarkID.size());
	return true;
}

void CGuildMarkManager::LoadMarkImages()
{
	bool isMarkExists[MAX_IMAGE_COUNT];
	memset(isMarkExists, 0, sizeof(isMarkExists));

	for (std::map<uint32_t, uint32_t>::iterator it = m_mapGID_MarkID.begin(); it != m_mapGID_MarkID.end(); ++it)
	{
		uint32_t markID = it->second;

		if (markID < MAX_IMAGE_COUNT * static_cast<uint32_t>(CGuildMarkImage::MARK_TOTAL_COUNT))
			isMarkExists[markID / CGuildMarkImage::MARK_TOTAL_COUNT] = true;
	}

	for (uint32_t i = 0; i < MAX_IMAGE_COUNT; ++i)
		if (isMarkExists[i])
			__GetImage(i);
}

void CGuildMarkManager::SaveMarkImage(uint32_t imgIdx)
{
	std::string path;

	if (GetMarkImageFilename(imgIdx, path))
		if (!__GetImage(imgIdx)->Save(path.c_str()))
			sys_err("%s Save failed\n", path.c_str());
}

CGuildMarkImage * CGuildMarkManager::__GetImage(uint32_t imgIdx)
{
	auto it = m_mapIdx_Image.find(imgIdx);

	if (it == m_mapIdx_Image.end())
	{
		std::string imagePath;

		if (GetMarkImageFilename(imgIdx, imagePath))
		{
			CGuildMarkImage * pkImage = m_mapIdx_Image.emplace(imgIdx, std::make_unique<CGuildMarkImage>()).first->second.get();
			
			if (!pkImage->Load(imagePath.c_str()))
			{
				pkImage->Build(imagePath.c_str());
				pkImage->Load(imagePath.c_str());
			}

			return pkImage;
		}
		else
			return nullptr;
	}
	else
		return it->second.get();
}

bool CGuildMarkManager::AddMarkIDByGuildID(uint32_t guildID, uint32_t markID)
{
	if (markID >= MAX_IMAGE_COUNT * static_cast<uint32_t>(CGuildMarkImage::MARK_TOTAL_COUNT))
		return false;

	m_mapGID_MarkID.insert(std::map<uint32_t, uint32_t>::value_type(guildID, markID));
	m_setFreeMarkID.erase(markID);
	return true;
}

uint32_t CGuildMarkManager::GetMarkID(uint32_t guildID)
{
	std::map<uint32_t, uint32_t>::iterator it = m_mapGID_MarkID.find(guildID);

	if (it == m_mapGID_MarkID.end())
		return INVALID_MARK_ID;

	return it->second;
}

uint32_t CGuildMarkManager::__AllocMarkID(uint32_t guildID)
{
	std::set<uint32_t>::iterator it = m_setFreeMarkID.lower_bound(0);

	if (it == m_setFreeMarkID.end())
		return INVALID_MARK_ID;

	uint32_t markID = *it;
	
	uint32_t imgIdx = markID / CGuildMarkImage::MARK_TOTAL_COUNT;
	CGuildMarkImage * pkImage = __GetImage(imgIdx); // To create the image if it doesn't exist 

	if (pkImage && AddMarkIDByGuildID(guildID, markID))
		return markID;

	return INVALID_MARK_ID;
}

uint32_t CGuildMarkManager::GetMarkCount() const
{
	return m_mapGID_MarkID.size();
}

// SERVER
void CGuildMarkManager::CopyMarkIdx(char * pcBuf) const
{
	uint16_t * pwBuf = (uint16_t *) pcBuf;

	for (std::map<uint32_t, uint32_t>::const_iterator it = m_mapGID_MarkID.begin(); it != m_mapGID_MarkID.end(); ++it)
	{
		*(pwBuf++) = it->first; // guild id
		*(pwBuf++) = it->second; // mark id
	}
}

// SERVER
uint32_t CGuildMarkManager::SaveMark(uint32_t guildID, uint8_t * pbMarkImage)
{
	uint32_t idMark;

	if ((idMark = GetMarkID(guildID)) == INVALID_MARK_ID)
	{
		if ((idMark = __AllocMarkID(guildID)) == INVALID_MARK_ID)
		{
			sys_err("CGuildMarkManager: cannot alloc mark id %u", guildID);
			return false;
		}
		else
			sys_log(0, "SaveMark: mark id alloc %u", idMark);
	}
	else
		sys_log(0, "SaveMark: mark id found %u", idMark);

	uint32_t imgIdx = (idMark / CGuildMarkImage::MARK_TOTAL_COUNT);
	CGuildMarkImage * pkImage = __GetImage(imgIdx);

	if (pkImage)
	{
		pkImage->SaveMark(idMark % CGuildMarkImage::MARK_TOTAL_COUNT, pbMarkImage);

		SaveMarkImage(imgIdx);
		SaveMarkIndex();
	}

	return idMark;
}

// SERVER
void CGuildMarkManager::DeleteMark(uint32_t guildID)
{
	std::map<uint32_t, uint32_t>::iterator it = m_mapGID_MarkID.find(guildID);

	if (it == m_mapGID_MarkID.end())
		return;

	CGuildMarkImage * pkImage;

	if ((pkImage = __GetImage(it->second / CGuildMarkImage::MARK_TOTAL_COUNT)) != nullptr)
		pkImage->DeleteMark(it->second % CGuildMarkImage::MARK_TOTAL_COUNT);

	m_setFreeMarkID.insert(it->second);
	m_mapGID_MarkID.erase(it);

	SaveMarkIndex();
}

// SERVER
void CGuildMarkManager::GetDiffBlocks(uint32_t imgIdx, const uint32_t * crcList, std::map<uint8_t, const SGuildMarkBlock *> & mapDiffBlocks)
{
	mapDiffBlocks.clear();

	// The client cannot request an image the server doesn't have.
	if (m_mapIdx_Image.end() == m_mapIdx_Image.find(imgIdx))
	{
		sys_err("invalid idx %u", imgIdx);
		return;
	}

	CGuildMarkImage * p = __GetImage(imgIdx);

	if (p)
		p->GetDiffBlocks(crcList, mapDiffBlocks);
}

///////////////////////////////////////////////////////////////////////////////////////
// Symbol
///////////////////////////////////////////////////////////////////////////////////////
const CGuildMarkManager::TGuildSymbol * CGuildMarkManager::GetGuildSymbol(uint32_t guildID)
{
	std::map<uint32_t, TGuildSymbol>::iterator it = m_mapSymbol.find(guildID);

	if (it == m_mapSymbol.end())
		return nullptr;

	return &it->second;
}

bool CGuildMarkManager::LoadSymbol(const char* filename)
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

			TGuildSymbol gs;
			gs.raw.resize(dwSize);
			fread(&gs.raw[0], 1, dwSize, fp);
			gs.crc = GetCRC32(reinterpret_cast<const char*>(&gs.raw[0]), dwSize);
			m_mapSymbol.insert(std::make_pair(guildID, gs));
		}
	}

	fclose(fp);
	return true;
}

void CGuildMarkManager::SaveSymbol(const char* filename)
{
	FILE* fp = fopen(filename, "wb");
	if (!fp)
	{
		sys_err("Cannot open Symbol file (name: %s)", filename);
		return;
	}

	uint32_t symbolCount = m_mapSymbol.size();
	fwrite(&symbolCount, 4, 1, fp);

	for (std::map<uint32_t, TGuildSymbol>::iterator it = m_mapSymbol.begin(); it != m_mapSymbol.end(); ++it)
	{
		uint32_t guildID = it->first;
		uint32_t dwSize = it->second.raw.size();
		fwrite(&guildID, 4, 1, fp);
		fwrite(&dwSize, 4, 1, fp);
		fwrite(&it->second.raw[0], 1, dwSize, fp);
	}

	fclose(fp);
}

void CGuildMarkManager::UploadSymbol(uint32_t guildID, int iSize, const uint8_t* pbyData)
{
	sys_log(0, "GuildSymbolUpload guildID %u Size %d", guildID, iSize);
	
	if (m_mapSymbol.find(guildID) == m_mapSymbol.end())
		m_mapSymbol.insert(std::make_pair(guildID, TGuildSymbol()));

	TGuildSymbol& rSymbol = m_mapSymbol[guildID];
	rSymbol.raw.clear();

	if (iSize > 0)
	{
		rSymbol.raw.reserve(iSize);
		std::copy(pbyData, (pbyData + iSize), std::back_inserter(rSymbol.raw));
		rSymbol.crc = GetCRC32(reinterpret_cast<const char*>(pbyData), iSize);
	}
}

