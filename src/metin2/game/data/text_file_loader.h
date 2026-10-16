#pragma once

#include <metin2/game/data/file_loader.h>
#include <metin2/game/world/vector.h>

#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <vector>

namespace metin2::game::inline data {

    typedef std::map<std::string, token_vector> token_vector_map;

    class text_file_loader {
      public:
        struct group_node {
            std::string group_name;

            token_vector_map local_token_vector_map;

            group_node *parent_node;
            std::vector<group_node *> child_node_vector;
        };

      public:
        static void destroy_system();

      public:
        text_file_loader();
        virtual ~text_file_loader();

        auto load(const char *file_name) -> bool;

        void set_top();
        auto child_node_count() -> uint32_t;
        auto set_child_node(const char *key) -> bool;
        auto set_child_node(const std::string &rstr_key_head, uint32_t index) -> bool;
        auto set_child_node(uint32_t index) -> bool;
        auto set_parent_node() -> bool;
        auto current_node_name(std::string *pstr_name) -> bool;

        auto get_token_vector(const std::string &rstr_key, token_vector **pp_token_vector) -> bool;
        auto token_integer(const std::string &rstr_key, int *data) -> bool;
        auto token_double_word(const std::string &rstr_key, uint32_t *data) -> bool;
        auto token_float(const std::string &rstr_key, float *data) -> bool;

        auto token_vector3(const std::string &rstr_key, vector *vector3) -> bool;

        auto token_position(const std::string &rstr_key, vector *vector_value) -> bool;
        auto token_string(const std::string &rstr_key, std::string *string_value) -> bool;

      protected:
        auto load_group(group_node *group_node_value) -> bool;

      protected:
        std::string file_name_;
        uint32_t dwcur_line_index_;
        const void *mc_p_data_;

        memory_text_file_loader file_loader_;

        group_node global_node_;
        group_node *pcur_node_;

      private:
        static std::deque<group_node> group_nodes_;
    };
}
