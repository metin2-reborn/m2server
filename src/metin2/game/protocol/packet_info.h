#pragma once

#include <cstdint>
#include <map>
#include <string>

namespace metin2::game::inline protocol {

    struct packet_element {
        int size;
        std::string name;
        int called;
        uint32_t load;
        bool sequence_packet;
    };

    class packet_info {
      public:
        packet_info();
        virtual ~packet_info();

        void set(int header, int size_value, const char *name_value, bool seq = false);
        auto get(int header, int *size, const char **ppsz_name) -> bool;

        void start();
        void end();

        void log(const char *file_name);

        auto is_sequence(int header) -> bool;
        void set_sequence(int header, bool seq);

      private:
        auto element(int header) -> packet_element *;

      protected:
        std::map<int, packet_element> packet_map_;
        packet_element *current_packet_;
        uint32_t start_time_;
    };

    class packet_info_cg : public packet_info {
      public:
        packet_info_cg();
        ~packet_info_cg() override;
    };

    class packet_info_gg : public packet_info {
      public:
        packet_info_gg();
        ~packet_info_gg() override;
    };

}
