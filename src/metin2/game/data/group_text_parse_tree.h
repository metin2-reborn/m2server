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

typedef std::map<std::string, token_vector>	token_vector_map;
typedef std::map<std::string, int> map_name_to_index;

class group_node
{
public:
	class group_node_row
	{
	public:
		group_node_row(group_node* group_node_value, token_vector& vec_values);
		virtual ~group_node_row();

		template <typename T>
		bool value(const std::string & col_key, T& value) const;
		template <typename T>
		bool value(int idx, T& value) const;

		int size() const;

	private:
		group_node*		owner_group_node_;
		token_vector	vec_values_;
	};
public:
	group_node();
	virtual ~group_node();

	bool load(const char * file_name);

	bool set_child_node(const char * key, group_node* child_group);
	group_node* child_node(const std::string & rstr_key) const;
	std::string node_name() const;

	int row_count();
	
	template <typename T>
	bool value(size_t i, const std::string & rstr_col_key, T& t_value) const;	// Returns the value of a given column in the n-th row (n is just the order in the map, unrelated to the order in the txt). 
																				// Out of place, but a convenience function.
	template <typename T>
	bool value(const std::string & rstr_row_key, const std::string & rstr_col_key, T& t_value) const;
	template <typename T>
	bool value(const std::string & rstr_row_key, int index, T& t_value) const;

	bool row(const std::string & rstr_row_key, const group_node_row ** pp_row) const;
	// Note: idx is unrelated to the order written in the txt.
	bool row(int idx, const group_node_row ** pp_row) const;
	bool group_row(const std::string& group_name, const std::string& row_value, const group_node::group_node_row ** pp_row) const;

	template <typename T>
	bool group_value(const std::string& group_name, const std::string& row, int col, T& t_value) const;
	template <typename T>
	bool group_value(const std::string& group_name, const std::string& row, const std::string& col, T& t_value) const;

	int	column_index_from_name(const std::string& name) const;

private:
	typedef std::map <std::string, group_node*> map_group;
	typedef std::map <std::string, group_node::group_node_row> map_row;
	map_group				child_nodes_;
	std::string group_name_;

	map_name_to_index			map_column_name_to_index_;
	map_row					map_rows_;
	friend class group_text_parse_tree_loader;
};

class group_text_parse_tree_loader
{
public:
	group_text_parse_tree_loader();
	virtual ~group_text_parse_tree_loader();

	bool load(const char * file_name);

	group_node*	group(const char * group_name);
private:
	bool load_group(group_node * group_node_value);

	std::unique_ptr<group_node>	root_group_node_;
	std::string					file_name_;
	uint32_t						dwcur_line_index_;

	memory_text_file_loader		file_loader_;
};

template <typename T>
bool from_string(T& t, const std::string& s)
{
	std::istringstream iss(s);
	return !(iss >> t).fail();
}

template <>
inline bool from_string <uint8_t>(uint8_t& t, const std::string& s)
{
	std::istringstream iss(s);
	int temp;
	bool b = !(iss >> temp).fail();
	t = (uint8_t)temp;
	return b;
}

template <typename T>
bool group_node::value(size_t i, const std::string & rstr_col_key, T& t_value) const
{
	if (i > map_rows_.size())
		return false;
	
	map_row::const_iterator row_it = map_rows_.begin();
	std::advance(row_it, i);
	
	auto col_idx_it = map_column_name_to_index_.find(rstr_col_key);
	if (map_column_name_to_index_.end() == col_idx_it)
	{
		return false;
	}

	int index = col_idx_it->second;
	if (row_it->second.size() <= index)
	{
		return false;
	}
	
	return row_it->second.value(index, t_value);
}

template <typename T>
bool group_node::value(const std::string & rstr_row_key, const std::string & rstr_col_key, T& t_value) const
{
	map_row::const_iterator row_it = map_rows_.find(rstr_row_key);
	if (map_rows_.end() == row_it)
	{
		return false;
	}
	auto col_idx_it = map_column_name_to_index_.find(rstr_col_key);
	if (map_column_name_to_index_.end() == col_idx_it)
	{
		return false;
	}

	int index = col_idx_it->second;
	if (row_it->second.size() <= index)
	{
		return false;
	}
	
	return row_it->second.value(index, t_value);
}

template <typename T>
bool group_node::value(const std::string & rstr_row_key, int index, T& t_value) const
{
	map_row::const_iterator row_it = map_rows_.find(rstr_row_key);
	if (map_rows_.end() == row_it)
	{
		return false;
	}

	if (row_it->second.size() <= index)
	{
		return false;
	}
	return row_it->second.value(index, t_value);
}

template <typename T>
bool group_node::group_value(const std::string& group_name, const std::string& row, int col, T& t_value) const
{
	group_node* pChildGroup = child_node(group_name);
	if (nullptr != pChildGroup)
	{
		if (pChildGroup->value(row, col, t_value))
			return true;
	}
	// Look in the default group.
	pChildGroup = child_node("default");
	if (nullptr != pChildGroup)
	{
		if (pChildGroup->value(row, col, t_value))
			return true;
	}
	return false;
}

template <typename T>
bool group_node::group_value(const std::string& group_name, const std::string& row, const std::string& col, T& t_value) const
{
	group_node* pChildGroup = child_node(group_name);
	if (nullptr != pChildGroup)
	{
		if (pChildGroup->value(row, col, t_value))
			return true;
	}
	// Look in the default group.
	pChildGroup = child_node("default");
	if (nullptr != pChildGroup)
	{
		if (pChildGroup->value(row, col, t_value))
			return true;
	}
	return false;
}

template <typename T>
bool group_node::group_node_row::value(const std::string & col_key, T& value) const
{
	int idx = owner_group_node_->column_index_from_name(col_key);
	if (idx < 0 || static_cast<size_t>(idx) >= vec_values_.size())
		return false;
	return from_string(value, vec_values_[idx]);
}

template <typename T>
bool group_node::group_node_row::value(int idx, T& value) const
{
	if (idx < 0 || static_cast<size_t>(idx) >= vec_values_.size())
		return false;
	return from_string(value, vec_values_[idx]);
}
}
