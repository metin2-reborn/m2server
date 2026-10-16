#pragma once

#include <metin2/game/data/file_loader.h>

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <memory>
#include <sstream>
#include <string>

namespace metin2::game::inline data {

    using token_vector_map = std::map<std::string, token_vector>;
    using map_name_to_index = std::map<std::string, int>;

    class group_node {
      public:
        class group_node_row {
          public:
            group_node_row(group_node *group_node_value, token_vector &values);
            virtual ~group_node_row();

            template <typename T>
            auto value(const std::string &col_key, T &value) const -> bool;
            template <typename T>
            auto value(int idx, T &value) const -> bool;

            auto size() const -> int;

          private:
            group_node *owner_group_node_;
            token_vector values_;
        };

      public:
        group_node();
        virtual ~group_node();

        auto load(const char *file_name) -> bool;

        auto set_child_node(const char *key, std::unique_ptr<group_node> child_group) -> bool;
        auto child_node(const std::string &rstr_key) const -> group_node *;
        auto node_name() const -> std::string;

        auto row_count() -> int;

        template <typename T>
        auto value(size_t i, const std::string &rstr_col_key, T &t_value) const -> bool;
        template <typename T>
        auto value(const std::string &rstr_row_key, const std::string &rstr_col_key, T &t_value) const -> bool;
        template <typename T>
        auto value(const std::string &rstr_row_key, int index, T &t_value) const -> bool;

        auto row(const std::string &rstr_row_key, const group_node_row **row) const -> bool;
        auto row(int idx, const group_node_row **row) const -> bool;
        auto group_row(const std::string &group_name, const std::string &row_value, const group_node::group_node_row **row) const -> bool;

        template <typename T>
        auto group_value(const std::string &group_name, const std::string &row, int col, T &t_value) const -> bool;
        template <typename T>
        auto group_value(const std::string &group_name, const std::string &row, const std::string &col, T &t_value) const -> bool;

        auto column_index_from_name(const std::string &name) const -> int;

      private:
        using map_group = std::map<std::string, std::unique_ptr<group_node>>;
        using map_row = std::map<std::string, group_node::group_node_row>;
        map_group child_nodes_;
        std::string group_name_;

        map_name_to_index column_name_to_index_;
        map_row rows_;
        friend class group_text_parse_tree_loader;
    };

    class group_text_parse_tree_loader {
      public:
        group_text_parse_tree_loader();
        virtual ~group_text_parse_tree_loader();

        auto load(const char *file_name) -> bool;

        auto group(const char *group_name) -> group_node *;

      private:
        auto load_group(group_node *group_node_value) -> bool;

        std::unique_ptr<group_node> root_group_node_;
        std::string file_name_;
        uint32_t dwcur_line_index_;

        memory_text_file_loader file_loader_;
    };

    template <typename T>
    auto from_string(T &t, const std::string &s) -> bool {
        std::istringstream iss(s);
        return !(iss >> t).fail();
    }

    template <>
    inline auto from_string<uint8_t>(uint8_t &t, const std::string &s) -> bool {
        std::istringstream iss(s);
        int temp;
        bool b = !(iss >> temp).fail();
        t = (uint8_t)temp;
        return b;
    }

    template <typename T>
    auto group_node::value(size_t i, const std::string &rstr_col_key, T &t_value) const -> bool {
        if (i >= rows_.size())
            return false;

        auto row_it = rows_.begin();
        std::advance(row_it, i);

        auto col_idx_it = column_name_to_index_.find(rstr_col_key);
        if (column_name_to_index_.end() == col_idx_it) {
            return false;
        }

        int index = col_idx_it->second;
        if (row_it->second.size() <= index) {
            return false;
        }

        return row_it->second.value(index, t_value);
    }

    template <typename T>
    auto group_node::value(const std::string &rstr_row_key, const std::string &rstr_col_key, T &t_value) const -> bool {
        auto row_it = rows_.find(rstr_row_key);
        if (rows_.end() == row_it) {
            return false;
        }
        auto col_idx_it = column_name_to_index_.find(rstr_col_key);
        if (column_name_to_index_.end() == col_idx_it) {
            return false;
        }

        int index = col_idx_it->second;
        if (row_it->second.size() <= index) {
            return false;
        }

        return row_it->second.value(index, t_value);
    }

    template <typename T>
    auto group_node::value(const std::string &rstr_row_key, int index, T &t_value) const -> bool {
        auto row_it = rows_.find(rstr_row_key);
        if (rows_.end() == row_it) {
            return false;
        }

        if (row_it->second.size() <= index) {
            return false;
        }
        return row_it->second.value(index, t_value);
    }

    template <typename T>
    auto group_node::group_value(const std::string &group_name, const std::string &row, int col, T &t_value) const -> bool {
        group_node *child_group = child_node(group_name);
        if (nullptr != child_group) {
            if (child_group->value(row, col, t_value))
                return true;
        }
        child_group = child_node("default");
        if (nullptr != child_group) {
            if (child_group->value(row, col, t_value))
                return true;
        }
        return false;
    }

    template <typename T>
    auto group_node::group_value(const std::string &group_name, const std::string &row, const std::string &col, T &t_value) const -> bool {
        group_node *child_group = child_node(group_name);
        if (nullptr != child_group) {
            if (child_group->value(row, col, t_value))
                return true;
        }
        child_group = child_node("default");
        if (nullptr != child_group) {
            if (child_group->value(row, col, t_value))
                return true;
        }
        return false;
    }

    template <typename T>
    auto group_node::group_node_row::value(const std::string &col_key, T &value) const -> bool {
        int idx = owner_group_node_->column_index_from_name(col_key);
        if (idx < 0 || static_cast<size_t>(idx) >= values_.size())
            return false;
        return from_string(value, values_[idx]);
    }

    template <typename T>
    auto group_node::group_node_row::value(int idx, T &value) const -> bool {
        if (idx < 0 || static_cast<size_t>(idx) >= values_.size())
            return false;
        return from_string(value, values_[idx]);
    }
}
