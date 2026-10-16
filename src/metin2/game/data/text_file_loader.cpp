#include <metin2/game/data/text_file_loader.h>

#include <metin2/game/data/file_loader.h>
#include <metin2/game/world/vector.h>

#include <metin2/common/utils.h>
#include <metin2/log/log.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <format>
#include <memory>
#include <string>
#include <utility>

namespace metin2::game::inline data {

    std::deque<text_file_loader::group_node> text_file_loader::group_nodes_;

    void text_file_loader::destroy_system() {
        group_nodes_.clear();
    }

    text_file_loader::text_file_loader()
        : dwcur_line_index_(0), data_(nullptr) {
        set_top();

        global_node_.group_name = "global";
        global_node_.parent_node = nullptr;
    }

    text_file_loader::~text_file_loader() = default;

    auto text_file_loader::load(const char *file_name) -> bool {
        file_name_ = file_name;

        dwcur_line_index_ = 0;

        FILE *fp = fopen(file_name, "rb");

        if (nullptr == fp)
            return false;

        fseek(fp, 0L, SEEK_END);
        const size_t file_size = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        auto data = std::make_unique_for_overwrite<char[]>(file_size);
        fread(data.get(), file_size, 1, fp);
        fclose(fp);

        file_loader_.bind(file_size, data.get());

        load_group(&global_node_);
        return true;
    }

    auto text_file_loader::load_group(group_node *group_node_value) -> bool {
        token_vector token_vector_value;
        for (; dwcur_line_index_ < file_loader_.line_count(); ++dwcur_line_index_) {
            if (!file_loader_.split_line(dwcur_line_index_, &token_vector_value))
                continue;

            to_lower(token_vector_value[0]);

            if ('{' == token_vector_value[0][0])
                continue;

            if ('}' == token_vector_value[0][0])
                break;

            if (0 == token_vector_value[0].compare("group")) {
                if (2 != token_vector_value.size()) {
                    metin2::log::error("{} line {:d}: group line has {:d} tokens instead of 2 (group names must not contain spaces)", file_name_, dwcur_line_index_, token_vector_value.size());
                    for (unsigned int i = 0; i < token_vector_value.size(); ++i)
                        metin2::log::error("Token {:d}: {}", i, token_vector_value[i]);
                    exit(1);
                    continue;
                }

                group_node *new_node = &group_nodes_.emplace_back();
                new_node->parent_node = group_node_value;
                new_node->group_name = token_vector_value[1];
                to_lower(new_node->group_name);
                group_node_value->child_node_vector.push_back(new_node);

                ++dwcur_line_index_;

                load_group(new_node);
            } else if (0 == token_vector_value[0].compare("list")) {
                if (2 != token_vector_value.size()) {
                    assert(!"There is no list name!");
                    continue;
                }

                token_vector sub_token_vector;

                to_lower(token_vector_value[1]);
                std::string key = token_vector_value[1];
                token_vector_value.clear();

                ++dwcur_line_index_;
                for (; dwcur_line_index_ < file_loader_.line_count(); ++dwcur_line_index_) {
                    if (!file_loader_.split_line(dwcur_line_index_, &sub_token_vector))
                        continue;

                    if ('{' == sub_token_vector[0][0])
                        continue;

                    if ('}' == sub_token_vector[0][0])
                        break;

                    for (uint32_t j = 0; j < sub_token_vector.size(); ++j) {
                        token_vector_value.push_back(sub_token_vector[j]);
                    }
                }

                group_node_value->local_token_vector_map.insert(std::make_pair(key, token_vector_value));
            } else {
                std::string key = token_vector_value[0];

                if (1 == token_vector_value.size()) {
                    metin2::log::error("{} line {:d}: key {} has no value",
                        file_name_,
                        dwcur_line_index_,
                        key);
                    break;
                }

                token_vector_value.erase(token_vector_value.begin());
                group_node_value->local_token_vector_map.insert(std::make_pair(key, token_vector_value));
            }
        }

        return true;
    }

    void text_file_loader::set_top() {
        pcur_node_ = &global_node_;
    }

    auto text_file_loader::child_node_count() -> uint32_t {
        if (!pcur_node_) {
            assert(!"Node to access has not set!");
            return 0;
        }

        return pcur_node_->child_node_vector.size();
    }

    auto text_file_loader::set_child_node(const char *key) -> bool {
        if (!pcur_node_) {
            assert(!"Node to access has not set!");
            return false;
        }

        for (uint32_t i = 0; i < pcur_node_->child_node_vector.size(); ++i) {
            group_node *group_node = pcur_node_->child_node_vector[i];
            if (0 == group_node->group_name.compare(key)) {
                pcur_node_ = group_node;
                return true;
            }
        }

        return false;
    }

    auto text_file_loader::set_child_node(const std::string &rstr_key_head, uint32_t index) -> bool {
        const std::string key = std::format("{}{:02}", rstr_key_head, (unsigned int)index);
        return set_child_node(key.c_str());
    }

    auto text_file_loader::set_child_node(uint32_t index) -> bool {
        if (!pcur_node_) {
            assert(!"Node to access has not set!");
            return false;
        }

        if (index >= pcur_node_->child_node_vector.size()) {
            assert(!"Node index to set is too large to access!");
            return false;
        }

        pcur_node_ = pcur_node_->child_node_vector[index];

        return true;
    }

    auto text_file_loader::set_parent_node() -> bool {
        if (!pcur_node_) {
            assert(!"Node to access has not set!");
            return false;
        }

        if (nullptr == pcur_node_->parent_node) {
            assert(!"Current group node is already top!");
            return false;
        }

        pcur_node_ = pcur_node_->parent_node;

        return true;
    }

    auto text_file_loader::current_node_name(std::string *pstr_name) -> bool {
        if (!pcur_node_)
            return false;
        if (nullptr == pcur_node_->parent_node)
            return false;

        *pstr_name = pcur_node_->group_name;

        return true;
    }

    auto text_file_loader::get_token_vector(const std::string &rstr_key, token_vector **pp_token_vector) -> bool {
        if (!pcur_node_) {
            assert(!"Node to access has not set!");
            return false;
        }

        auto it = pcur_node_->local_token_vector_map.find(rstr_key);
        if (pcur_node_->local_token_vector_map.end() == it) {
            metin2::log::debug("{}: group {} has no key {}", file_name_, pcur_node_->group_name, rstr_key);
            return false;
        }

        *pp_token_vector = &it->second;

        return true;
    }

    auto text_file_loader::token_integer(const std::string &rstr_key, int *data) -> bool {
        token_vector *token_vector;
        if (!get_token_vector(rstr_key, &token_vector))
            return false;

        if (token_vector->empty()) {
            metin2::log::debug("{}: group {} has no integer value for {}", file_name_, pcur_node_->group_name, rstr_key);
            return false;
        }

        int out = 0;
        str_to_number(out, token_vector->at(0).c_str());
        *data = out;

        return true;
    }

    auto text_file_loader::token_double_word(const std::string &rstr_key, uint32_t *data) -> bool {
        return token_integer(rstr_key, (int *)data);
    }

    auto text_file_loader::token_float(const std::string &rstr_key, float *data) -> bool {
        token_vector *token_vector;
        if (!get_token_vector(rstr_key, &token_vector))
            return false;

        if (token_vector->empty()) {
            metin2::log::debug("{}: group {} has no float value for {}", file_name_, pcur_node_->group_name, rstr_key);
            return false;
        }

        *data = atof(token_vector->at(0).c_str());

        return true;
    }

    auto text_file_loader::token_vector3(const std::string &rstr_key, vector *vector3) -> bool {
        token_vector *token_vector;
        if (!get_token_vector(rstr_key, &token_vector))
            return false;

        if (token_vector->size() != 3) {
            metin2::log::debug("{}: group {} key {} needs 3 values", file_name_, pcur_node_->group_name, rstr_key);
            return false;
        }

        vector3->x = atof(token_vector->at(0).c_str());
        vector3->y = atof(token_vector->at(1).c_str());
        vector3->z = atof(token_vector->at(2).c_str());

        return true;
    }

    auto text_file_loader::token_position(const std::string &rstr_key, vector *vector_value) -> bool {
        return token_vector3(rstr_key, vector_value);
    }

    auto text_file_loader::token_string(const std::string &rstr_key, std::string *string_value) -> bool {
        token_vector *token_vector;
        if (!get_token_vector(rstr_key, &token_vector))
            return false;

        if (token_vector->empty()) {
            metin2::log::debug("{}: group {} has no string value for {}", file_name_, pcur_node_->group_name, rstr_key);
            return false;
        }

        *string_value = token_vector->at(0);

        return true;
    }
}
