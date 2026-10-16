#pragma once

#include <metin2/common/singleton.h>

#include <lzo/lzo1x.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace metin2::game::inline protocol {

    class lzo_manager : public singleton<lzo_manager> {
      public:
        lzo_manager();
        ~lzo_manager() override;

        auto compress(const uint8_t *src, size_t srcsize, uint8_t *dest, lzo_uint *pui_dest_size) -> bool;
        auto decompress(const uint8_t *src, size_t srcsize, uint8_t *dest, lzo_uint *pui_dest_size) -> bool;
        auto max_compressed_size(size_t original) -> size_t;

        auto work_memory() -> uint8_t * { return workmem_.data(); }

      private:
        std::vector<uint8_t> workmem_;
    };
}
