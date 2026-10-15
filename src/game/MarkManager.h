#pragma once

#include "MarkImage.h"

#include <common/singleton.h>

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace metin2::game {

class CGuildMarkManager : public singleton<CGuildMarkManager>
{
	public:
		enum
		{
			MAX_IMAGE_COUNT = 5,
			INVALID_MARK_ID = 0xffffffff,
		};

		// Symbol
		struct TGuildSymbol
		{
			uint32_t crc;
			std::vector<uint8_t> raw;
		};

		CGuildMarkManager();
		~CGuildMarkManager() override;

		const TGuildSymbol * GetGuildSymbol(uint32_t GID);
		bool LoadSymbol(const char* filename);
		void SaveSymbol(const char* filename);
		void UploadSymbol(uint32_t guildID, int iSize, const uint8_t* pbyData);

		//
		// Mark
		//
		void SetMarkPathPrefix(const char * prefix);

		bool LoadMarkIndex(); // Load mark index (server only)
		bool SaveMarkIndex(); // Save mark index

		void LoadMarkImages(); // Load all mark images
		void SaveMarkImage(uint32_t imgIdx); // Save mark image

		bool GetMarkImageFilename(uint32_t imgIdx, std::string & path) const;
		bool AddMarkIDByGuildID(uint32_t guildID, uint32_t markID);
		uint32_t GetMarkCount() const;
		uint32_t GetMarkID(uint32_t guildID);

		// SERVER
		void CopyMarkIdx(char * pcBuf) const;
		uint32_t SaveMark(uint32_t guildID, uint8_t * pbMarkImage);
		void DeleteMark(uint32_t guildID);
		void GetDiffBlocks(uint32_t imgIdx, const uint32_t * crcList, std::map<uint8_t, const SGuildMarkBlock *> & mapDiffBlocks);

		// CLIENT

	private:
		// 
		// Mark
		//
		uint32_t __AllocMarkID(uint32_t guildID);

		CGuildMarkImage * __GetImage(uint32_t imgIdx);

		std::map<uint32_t, std::unique_ptr<CGuildMarkImage>> m_mapIdx_Image; // index = image index
		std::map<uint32_t, uint32_t> m_mapGID_MarkID; // index = guild id

		std::set<uint32_t> m_setFreeMarkID;
		std::string		m_pathPrefix;

	private:
		//
		// Symbol
		//
		std::map<uint32_t, TGuildSymbol> m_mapSymbol;
};
}
