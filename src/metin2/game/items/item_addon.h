#pragma once

#include <metin2/game/app/typedef.h>

namespace metin2::game::inline items {

    class item_addon_manager {
      public:
        item_addon_manager();
        ~item_addon_manager();

        void apply_addon_to(int addon_type, item *item_value);
    };

    inline auto get_item_addon_manager() -> item_addon_manager & {
        static item_addon_manager instance;
        return instance;
    }
}
