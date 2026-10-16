#include <metin2/proto/item_types.h>
#include <metin2/proto/mob_types.h>

#include <cstdint>
#include <vector>

#pragma once

namespace metin2::game::inline items {

    class item_drop_info {
      public:
        item_drop_info(int level_start_value, int level_end_value, int percent_value, uint32_t vnum_value) : level_start(level_start_value), level_end(level_end_value), percent(percent_value), vnum(vnum_value) {
        }

        int level_start;
        int level_end;
        int percent;
        uint32_t vnum;

        friend auto operator<(const item_drop_info &l, const item_drop_info &r) -> bool {
            return l.level_end < r.level_end;
        }
    };

    extern std::vector<item_drop_info> vec_common_drop_item[MOB_RANK_MAX_NUM];

    struct drop_item {
        int lv_start;
        int lv_end;
        float percent;
        char item_name[PROTO_NAME_MAX_LEN + 1];
        int count;
    };
}
