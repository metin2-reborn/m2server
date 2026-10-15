
#pragma once

#include <metin2/common/singleton.h>
#include <metin2/proto/tables.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace metin2::game::inline text {

class banword_manager : public singleton<banword_manager>
{
	public:
		banword_manager();
		~banword_manager() override;

		bool initialize(banword_table * p, uint16_t wSize);
		bool check_string(const char * c_pszString, size_t _len);
		void convert_string(char * c_pszString, size_t _len);

	protected:
		typedef std::unordered_map<std::string, bool> banword_hashmap;
		banword_hashmap hashmap_words_;
};
}
