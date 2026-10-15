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
	: dwcur_line_index_(0)
{
}

group_text_parse_tree_loader::~group_text_parse_tree_loader()
{
}

bool group_text_parse_tree_loader::load(const char * c_szFileName)
{
	file_name_ = c_szFileName;

	dwcur_line_index_ = 0;

	FILE* fp = fopen(c_szFileName, "rb");

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
	if (!load_group(root_group_node_.get()))
	{
		root_group_node_.reset();
		return false;
	}
	return true;
}

bool group_text_parse_tree_loader::load_group(group_node * pGroupNode)
{
	token_vector stTokenVector;
	for (; dwcur_line_index_ < file_loader_.line_count(); ++dwcur_line_index_)
	{
		if (!file_loader_.split_line(dwcur_line_index_, &stTokenVector))
			continue;

		to_lower(stTokenVector[0]);

		if ('{' == stTokenVector[0][0])
			continue;

		if ('}' == stTokenVector[0][0])
			break;

		// Group
		if (0 == stTokenVector[0].compare("group"))
		{
			if (2 != stTokenVector.size())
			{
				metin2::log::error("Invalid group syntax token size: {:d} != 2 (DO NOT SPACE IN NAME)", stTokenVector.size());
				for (unsigned int i = 0; i < stTokenVector.size(); ++i)
					metin2::log::error("  {:d} {}", i, stTokenVector[i].c_str());
				exit(1);
				continue;
			}

			group_node * pNewNode = new group_node();

			pNewNode->group_name_ = stTokenVector[1];
			to_lower(pNewNode->group_name_);
			
			pGroupNode->set_child_node(pNewNode->group_name_.c_str(), pNewNode);

			++dwcur_line_index_;

			load_group(pNewNode);
		}
		// Column Names
		else if (0 == stTokenVector[0].compare("#--#"))
		{
			for (size_t i = 1; i < stTokenVector.size(); i++)
			{
				to_lower(stTokenVector[i]);
				pGroupNode->map_column_name_to_index_.insert(map_name_to_index::value_type (stTokenVector[i], i - 1));
			}
		}
		else
		{
			std::string key = stTokenVector[0];

			if (1 == stTokenVector.size())
			{
				metin2::log::error("CGroupTextParseTreeLoader::LoadGroup : must have a value (filename: {} line: {:d} key: {})",
						file_name_.c_str(),
						dwcur_line_index_,
						key.c_str());
				break;
			}

			stTokenVector.erase(stTokenVector.begin());
			pGroupNode->map_rows_.insert(std::make_pair(key, group_node::group_node_row(pGroupNode, stTokenVector)));
		}
	}

	return true;
}

group_node* group_text_parse_tree_loader::group(const char * c_szGroupName)
{
	if (nullptr == root_group_node_)
		return nullptr;
	return root_group_node_->child_node(c_szGroupName);
}

group_node::group_node()
{
}

group_node::~group_node()
{
	for (map_group::iterator it = child_nodes_.begin(); it != child_nodes_.end(); it++)
	{
		delete it->second;
	}
}

bool group_node::set_child_node(const char * c_szKey, group_node* pChildGroup)
{
	if (nullptr == pChildGroup)
	{
		child_nodes_.erase(c_szKey);
		return true;
	}
	map_group::iterator it = child_nodes_.find(c_szKey);
	if (it != child_nodes_.end())
		return false;

	child_nodes_.insert (map_group::value_type (c_szKey, pChildGroup));
	
	return true;
}

group_node* group_node::child_node(const std::string & c_rstrKey) const
{
	map_group::const_iterator it = child_nodes_.find(c_rstrKey);
	if (it != child_nodes_.end())
		return it->second;
	else
		return nullptr;
}

std::string group_node::node_name() const
{
	return group_name_;
}

int group_node::row_count()
{
	return map_rows_.size();
}

bool group_node::row(const std::string & c_rstrRowKey, const group_node::group_node_row ** ppRow) const
{
	map_row::const_iterator row_it = map_rows_.find(c_rstrRowKey);
	if (map_rows_.end() == row_it)
	{
		return false;
	}

	*ppRow = &row_it->second;

	return true;
}

// Note: idx is unrelated to the order written in the txt.
bool group_node::row(int idx, const group_node::group_node_row ** ppRow) const
{
	if (idx < 0 || static_cast<size_t>(idx) >= map_rows_.size())
		return false;

	map_row::const_iterator row_it = map_rows_.begin();

	std::advance(row_it, idx);

	*ppRow = &row_it->second;

	return true;
}

bool group_node::group_row(const std::string& stGroupName, const std::string& stRow, const group_node::group_node_row ** ppRow) const
{
	group_node* pChildGroup = child_node(stGroupName);
	if (nullptr != pChildGroup)
	{
		if (pChildGroup->row(stRow, ppRow))
			return true;
	}
	// Look in the default group.
	pChildGroup = child_node("default");
	if (nullptr != pChildGroup)
	{
		if (pChildGroup->row(stRow, ppRow))
			return true;
	}
	return false;
}

int	group_node::column_index_from_name(const std::string& stName) const
{
	map_name_to_index::const_iterator it = map_column_name_to_index_.find(stName);
	if (map_column_name_to_index_.end() == it)
		return -1;
	else
		return it->second;
}

group_node::group_node_row::group_node_row(group_node* pGroupNode, token_vector& vec_values)
{
	owner_group_node_ = pGroupNode;
	vec_values_.clear();
	std::copy(vec_values.begin(), vec_values.end(), std::back_inserter(vec_values_));
}

group_node::group_node_row::~group_node_row()
{
}

int group_node::group_node_row::size() const
{
	return vec_values_.size();
}
}
