
#pragma once

#include <metin2/common/singleton.h>
#include <metin2/proto/tables.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace metin2::game::inline text {

    class banword_manager : public singleton<banword_manager> {
      public:
        banword_manager();
        ~banword_manager() override;

        auto initialize(banword_table *p, uint16_t size_value) -> bool;
        auto check_string(const char *string_value, size_t len_value) -> bool;
        void convert_string(char *string_value, size_t len_value);

      protected:
        using banword_hashmap = std::unordered_map<std::string, bool>;
        banword_hashmap hashmap_words_;
    };
}
