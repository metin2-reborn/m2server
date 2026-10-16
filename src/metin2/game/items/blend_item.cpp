

#include <algorithm>
#include <metin2/game/items/blend_item.h>

#include <metin2/game/app/constants.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/items/item.h>
#include <metin2/game/text/locale_service.h>

#include <metin2/common/utils.h>
#include <metin2/core/utils.h>
#include <metin2/log/log.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <format>
#include <string>
#include <vector>

namespace metin2::game::inline items {

    struct blend_item_info {
        uint32_t item_vnum;
        int apply_type;
        int apply_value[MAX_BLEND_ITEM_VALUE];
        int apply_duration[MAX_BLEND_ITEM_VALUE];
    };

    std::vector<blend_item_info> blend_info;

    auto blend_item_init() -> bool {
        const std::string file_name = std::format("{}/blend.txt", locale_service_get_base_path());

        metin2::log::info("Blend: loading {}", file_name);

        blend_info.clear();

        if (false == blend_item_load(file_name.c_str())) {
            metin2::log::error("Cannot load blend items from {}", file_name);
            return false;
        }
        return true;
    }

    auto blend_item_load(const char *file) -> bool {
        FILE *fp;
        char one_line[256];
        const char *delim = " \t\r\n";
        char *v;

        blend_item_info blend_item_info{};

        if (nullptr == file || 0 == file[0])
            return false;

        if ((fp = fopen(file, "r")) == nullptr)
            return false;

        while (fgets(one_line, 256, fp)) {
            if (one_line[0] == '#')
                continue;

            const char *token_string = strtok(one_line, delim);

            if (nullptr == token_string)
                continue;

            if (!strcasecmp(token_string, "section")) {
                blend_item_info = {};
            } else if (!strcasecmp(token_string, "item_vnum")) {
                v = strtok(nullptr, delim);

                if (nullptr == v) {
                    fclose(fp);
                    return false;
                }

                str_to_number(blend_item_info.item_vnum, v);
            } else if (!strcasecmp(token_string, "apply_type")) {
                v = strtok(nullptr, delim);

                if (nullptr == v) {
                    fclose(fp);
                    return false;
                }

                if (0 == (blend_item_info.apply_type = fn_get_apply_type(v))) {
                    metin2::log::error("Unknown blend apply type {}", v);
                    fclose(fp);
                    return false;
                }
            } else if (!strcasecmp(token_string, "apply_value")) {
                for (int i = 0; i < MAX_BLEND_ITEM_VALUE; ++i) {
                    v = strtok(nullptr, delim);

                    if (nullptr == v) {
                        fclose(fp);
                        return false;
                    }

                    str_to_number(blend_item_info.apply_value[i], v);
                }
            } else if (!strcasecmp(token_string, "apply_duration")) {
                for (int i = 0; i < MAX_BLEND_ITEM_VALUE; ++i) {
                    v = strtok(nullptr, delim);

                    if (nullptr == v) {
                        fclose(fp);
                        return false;
                    }

                    str_to_number(blend_item_info.apply_duration[i], v);
                }
            } else if (!strcasecmp(token_string, "end")) {
                blend_info.push_back(blend_item_info);
            }
        }

        fclose(fp);

        return true;
    }

    static auto fn_random_index() -> int {
        int percent = number(1, 100);

        if (percent <= 10)
            return 0;
        else if (percent <= 30)
            return 1;
        else if (percent <= 70)
            return 2;
        else if (percent <= 90)
            return 3;
        else
            return 4;

        return 0;
    }

    static auto fn_ecs_random_index() -> int {
        int percent = number(1, 100);

        if (percent <= 5)
            return 0;
        else if (percent <= 15)
            return 1;
        else if (percent <= 60)
            return 2;
        else if (percent <= 85)
            return 3;
        else
            return 4;

        return 0;
    }

    auto blend_item_set_value(item *item) -> bool {
        for (blend_item_info &info : blend_info) {
            if (info.item_vnum == item->vnum()) {
                int apply_type;
                int apply_value;
                int apply_duration;

                if (item->vnum() == 51002) {
                    apply_type = info.apply_type;
                    apply_value = info.apply_value[fn_ecs_random_index()];
                    apply_duration = info.apply_duration[fn_ecs_random_index()];
                } else {
                    apply_type = info.apply_type;
                    apply_value = info.apply_value[fn_random_index()];
                    apply_duration = info.apply_duration[fn_random_index()];
                }
                metin2::log::debug("Blend: apply type {:d}, value {:d}, duration {:d}", apply_type, apply_value, apply_duration);
                item->set_socket(0, apply_type);
                item->set_socket(1, apply_value);
                item->set_socket(2, apply_duration);
                return true;
            }
        }
        return false;
    }

    auto blend_item_find(uint32_t item_vnum) -> bool {
        return std::ranges::any_of(blend_info, [item_vnum](const blend_item_info &info) -> bool { return info.item_vnum == item_vnum; });
    }
}
