#include <metin2/game/data/group_text_parse_tree.h>

#include <metin2/game/data/file_loader.h>

#include <metin2/common/utils.h>
#include <metin2/log/log.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

namespace metin2::game::inline data {

    group_text_parse_tree_loader::group_text_parse_tree_loader()
        : dwcur_line_index_(0) {
    }

    group_text_parse_tree_loader::~group_text_parse_tree_loader() = default;

    auto group_text_parse_tree_loader::load(const char *file_name) -> bool {
        file_name_ = file_name;

        dwcur_line_index_ = 0;

        FILE *fp = fopen(file_name, "rb");

        if (nullptr == fp)
            return false;

        fseek(fp, 0L, SEEK_END);
        const size_t fileSize = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        auto pData = std::make_unique<char[]>(fileSize);
        fread(pData.get(), fileSize, 1, fp);
        fclose(fp);

        file_loader_.bind(fileSize, pData.get());
        pData.reset();

        root_group_node_ = std::make_unique<group_node>();
        if (!load_group(root_group_node_.get())) {
            root_group_node_.reset();
            return false;
        }
        return true;
    }

    auto group_text_parse_tree_loader::load_group(group_node *group_node_value) -> bool {
        token_vector stTokenVector;
        for (; dwcur_line_index_ < file_loader_.line_count(); ++dwcur_line_index_) {
            if (!file_loader_.split_line(dwcur_line_index_, &stTokenVector))
                continue;

            to_lower(stTokenVector[0]);

            if ('{' == stTokenVector[0][0])
                continue;

            if ('}' == stTokenVector[0][0])
                break;

            if (0 == stTokenVector[0].compare("group")) {
                if (2 != stTokenVector.size()) {
                    metin2::log::error("{} line {:d}: group line has {:d} tokens instead of 2 (group names must not contain spaces)", file_name_, dwcur_line_index_, stTokenVector.size());
                    for (unsigned int i = 0; i < stTokenVector.size(); ++i)
                        metin2::log::error("Token {:d}: {}", i, stTokenVector[i]);
                    exit(1);
                    continue;
                }

                auto pNewNode = std::make_unique<group_node>();
                group_node *pNode = pNewNode.get();

                pNode->group_name_ = stTokenVector[1];
                to_lower(pNode->group_name_);

                ++dwcur_line_index_;

                load_group(pNode);

                group_node_value->set_child_node(pNode->group_name_.c_str(), std::move(pNewNode));
            } else if (0 == stTokenVector[0].compare("#--#")) {
                for (size_t i = 1; i < stTokenVector.size(); i++) {
                    to_lower(stTokenVector[i]);
                    group_node_value->map_column_name_to_index_.insert(map_name_to_index::value_type(stTokenVector[i], i - 1));
                }
            } else {
                std::string key = stTokenVector[0];

                if (1 == stTokenVector.size()) {
                    metin2::log::error("{} line {:d}: key {} has no value",
                        file_name_,
                        dwcur_line_index_,
                        key);
                    break;
                }

                stTokenVector.erase(stTokenVector.begin());
                group_node_value->map_rows_.insert(std::make_pair(key, group_node::group_node_row(group_node_value, stTokenVector)));
            }
        }

        return true;
    }

    auto group_text_parse_tree_loader::group(const char *group_name) -> group_node * {
        if (nullptr == root_group_node_)
            return nullptr;
        return root_group_node_->child_node(group_name);
    }

    group_node::group_node() = default;

    group_node::~group_node() = default;

    auto group_node::set_child_node(const char *key, std::unique_ptr<group_node> child_group) -> bool {
        if (nullptr == child_group) {
            child_nodes_.erase(key);
            return true;
        }
        map_group::iterator it = child_nodes_.find(key);
        if (it != child_nodes_.end())
            return false;

        child_nodes_.emplace(key, std::move(child_group));

        return true;
    }

    auto group_node::child_node(const std::string &rstr_key) const -> group_node * {
        map_group::const_iterator it = child_nodes_.find(rstr_key);
        if (it != child_nodes_.end())
            return it->second.get();
        else
            return nullptr;
    }

    auto group_node::node_name() const -> std::string {
        return group_name_;
    }

    auto group_node::row_count() -> int {
        return map_rows_.size();
    }

    auto group_node::row(const std::string &rstr_row_key, const group_node::group_node_row **pp_row) const -> bool {
        map_row::const_iterator row_it = map_rows_.find(rstr_row_key);
        if (map_rows_.end() == row_it) {
            return false;
        }

        *pp_row = &row_it->second;

        return true;
    }

    auto group_node::row(int idx, const group_node::group_node_row **pp_row) const -> bool {
        if (idx < 0 || static_cast<size_t>(idx) >= map_rows_.size())
            return false;

        map_row::const_iterator row_it = map_rows_.begin();

        std::advance(row_it, idx);

        *pp_row = &row_it->second;

        return true;
    }

    auto group_node::group_row(const std::string &group_name, const std::string &row_value, const group_node::group_node_row **pp_row) const -> bool {
        group_node *pChildGroup = child_node(group_name);
        if (nullptr != pChildGroup) {
            if (pChildGroup->row(row_value, pp_row))
                return true;
        }
        pChildGroup = child_node("default");
        if (nullptr != pChildGroup) {
            if (pChildGroup->row(row_value, pp_row))
                return true;
        }
        return false;
    }

    auto group_node::column_index_from_name(const std::string &name) const -> int {
        map_name_to_index::const_iterator it = map_column_name_to_index_.find(name);
        if (map_column_name_to_index_.end() == it)
            return -1;
        else
            return it->second;
    }

    group_node::group_node_row::group_node_row(group_node *group_node_value, token_vector &vec_values) {
        owner_group_node_ = group_node_value;
        vec_values_.clear();
        std::copy(vec_values.begin(), vec_values.end(), std::back_inserter(vec_values_));
    }

    group_node::group_node_row::~group_node_row() = default;

    auto group_node::group_node_row::size() const -> int {
        return vec_values_.size();
    }
}
