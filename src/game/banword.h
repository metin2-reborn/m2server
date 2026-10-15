
#pragma once

#include <common/singleton.h>
#include <common/tables.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace metin2::game {

class CBanwordManager : public singleton<CBanwordManager>
{
	public:
		CBanwordManager();
		~CBanwordManager() override;

		bool Initialize(TBanwordTable * p, uint16_t wSize);
		bool CheckString(const char * c_pszString, size_t _len);
		void ConvertString(char * c_pszString, size_t _len);

	protected:
		typedef std::unordered_map<std::string, bool> TBanwordHashmap;
		TBanwordHashmap m_hashmap_words;
};
}
