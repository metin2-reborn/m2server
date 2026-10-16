#pragma once

#include <metin2/game/app/event.h>

#include <metin2/common/singleton.h>

#include <cstdint>
#include <cstring>
#include <list>
#include <map>

namespace metin2::game {

    enum target_types {
        TARGET_TYPE_POS = (1 << 0),
        TARGET_TYPE_VID = (1 << 1),
    };

    EVENTINFO(target_info) {

        int id;

        uint32_t pid;
        uint32_t quest_index;

        char target_name[32 + 1];
        char target_desc[32 + 1];

        int type;
        int arg1;
        int arg2;

        int map_index;
        int old_x;
        int old_y;

        bool send_to_client;

        target_info()
            : id(0), pid(0), quest_index(0), type(0), arg1(0), arg2(0), map_index(0), old_x(0), old_y(0), send_to_client(false) {
            ::memset(target_name, 0, 32 + 1);
            ::memset(target_desc, 0, 32 + 1);
        }
    };

    class target_manager : public singleton<target_manager> {
      public:
        target_manager();
        ~target_manager() override;

        void create_target(uint32_t pid_value, uint32_t quest_index_value, const char *target_name_value, int type_value, int arg1_value, int arg2_value, int map_index_value, const char *target_desc_value = nullptr, int send_flag = 1);
        void delete_target(uint32_t pid_value, uint32_t quest_index_value, const char *target_name_value);

        void logout(uint32_t pid);
        auto get_target_info(uint32_t pid, int type_value, int arg1_value) -> target_info *;

        auto target_event(uint32_t pid, uint32_t quest_index_value, const char *target_name_value) -> event_ptr;

      protected:
        std::map<uint32_t, std::list<event_ptr>> map_k_list_event_;
        int id_;
    };
}
