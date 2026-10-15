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
#include <memory>
#include <string>
#include <utility>

namespace metin2::game::inline data {

std::deque<text_file_loader::group_node> text_file_loader::group_nodes_;

void text_file_loader::destroy_system()
{
	group_nodes_.clear();
}

text_file_loader::text_file_loader()
	: dwcur_line_index_(0), mc_p_data_(nullptr)
{
	set_top();

	global_node_.group_name = "global";
	global_node_.parent_node = nullptr;
}

text_file_loader::~text_file_loader()
{	
}

bool text_file_loader::load(const char * file_name)
{
	file_name_ = file_name;

	dwcur_line_index_ = 0;

	FILE* fp = fopen(file_name, "rb");

	if (nullptr == fp)
		return false;

	fseek(fp, 0L, SEEK_END);
	const size_t fileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	auto pData = std::make_unique_for_overwrite<char[]>(fileSize);
	fread(pData.get(), fileSize, 1, fp);
	fclose(fp);

	file_loader_.bind(fileSize, pData.get());

	load_group(&global_node_);
	return true;
}

bool text_file_loader::load_group(group_node * group_node_value)
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

			group_node * pNewNode = &group_nodes_.emplace_back();
			pNewNode->parent_node = group_node_value;
			pNewNode->group_name = stTokenVector[1];
			to_lower(pNewNode->group_name);
			group_node_value->child_node_vector.push_back(pNewNode);

			++dwcur_line_index_;

			load_group(pNewNode);
		}
		else if (0 == stTokenVector[0].compare("list"))
		{
			if (2 != stTokenVector.size())
			{
				assert(!"There is no list name!");
				continue;
			}

			token_vector stSubTokenVector;

			to_lower(stTokenVector[1]);
			std::string key = stTokenVector[1];
			stTokenVector.clear();

			++dwcur_line_index_;
			for (; dwcur_line_index_ < file_loader_.line_count(); ++dwcur_line_index_)
			{
				if (!file_loader_.split_line(dwcur_line_index_, &stSubTokenVector))
					continue;

				if ('{' == stSubTokenVector[0][0])
					continue;

				if ('}' == stSubTokenVector[0][0])
					break;

				for (uint32_t j = 0; j < stSubTokenVector.size(); ++j)
				{
					stTokenVector.push_back(stSubTokenVector[j]);
				}
			}

			group_node_value->local_token_vector_map.insert(std::make_pair(key, stTokenVector));
		}
		else
		{
			std::string key = stTokenVector[0];

			if (1 == stTokenVector.size())
			{
				metin2::log::error("CTextFileLoader::LoadGroup : must have a value (filename: {} line: {:d} key: {})",
						file_name_.c_str(),
						dwcur_line_index_,
						key.c_str());
				break;
			}

			stTokenVector.erase(stTokenVector.begin());
			group_node_value->local_token_vector_map.insert(std::make_pair(key, stTokenVector));
		}
	}

	return true;
}

void text_file_loader::set_top()
{
	pcur_node_ = &global_node_;
}

uint32_t text_file_loader::child_node_count()
{
	if (!pcur_node_)
	{
		assert(!"Node to access has not set!");
		return 0;
	}

	return pcur_node_->child_node_vector.size();
}

bool text_file_loader::set_child_node(const char * key)
{
	if (!pcur_node_)
	{
		assert(!"Node to access has not set!");
		return false;
	}

	for (uint32_t i = 0; i < pcur_node_->child_node_vector.size(); ++i)
	{
		group_node * pGroupNode = pcur_node_->child_node_vector[i];
		if (0 == pGroupNode->group_name.compare(key))
		{
			pcur_node_ = pGroupNode;
			return true;
		}
	}

	return false;
}

bool text_file_loader::set_child_node(const std::string & rstr_key_head, uint32_t index)
{
	char szKey[32];
	snprintf(szKey, sizeof(szKey), "%s%02u", rstr_key_head.c_str(), (unsigned int) index);
	return set_child_node(szKey);
}

bool text_file_loader::set_child_node(uint32_t index)
{
	if (!pcur_node_)
	{
		assert(!"Node to access has not set!");
		return false;
	}

	if (index >= pcur_node_->child_node_vector.size())
	{
		assert(!"Node index to set is too large to access!");
		return false;
	}

	pcur_node_ = pcur_node_->child_node_vector[index];

	return true;
}

bool text_file_loader::set_parent_node()
{
	if (!pcur_node_)
	{
		assert(!"Node to access has not set!");
		return false;
	}

	if (nullptr == pcur_node_->parent_node)
	{
		assert(!"Current group node is already top!");
		return false;
	}

	pcur_node_ = pcur_node_->parent_node;

	return true;
}

bool text_file_loader::current_node_name(std::string * pstr_name)
{
	if (!pcur_node_)
		return false;
	if (nullptr == pcur_node_->parent_node)
		return false;

	*pstr_name = pcur_node_->group_name;

	return true;
}

bool text_file_loader::get_token_vector(const std::string & rstr_key, token_vector ** pp_token_vector)
{
	if (!pcur_node_)
	{
		assert(!"Node to access has not set!");
		return false;
	}

	token_vector_map::iterator it = pcur_node_->local_token_vector_map.find(rstr_key);
	if (pcur_node_->local_token_vector_map.end() == it)
	{
		metin2::log::debug( " CTextFileLoader::GetTokenVector - Failed to find the key {} [{} :: {}]", file_name_.c_str(), pcur_node_->group_name.c_str(), rstr_key.c_str());
		return false;
	}

	*pp_token_vector = &it->second;

	return true;
}

bool text_file_loader::token_integer(const std::string & rstr_key, int * data)
{
	token_vector * pTokenVector;
	if (!get_token_vector(rstr_key, &pTokenVector))
		return false;

	if (pTokenVector->empty())
	{
		metin2::log::debug( " CTextFileLoader::GetTokenInteger - Failed to find the value {} [{} : {}]", file_name_.c_str(), pcur_node_->group_name.c_str(), rstr_key.c_str());
		return false;
	}

	int out = 0;
	str_to_number(out, pTokenVector->at(0).c_str());
	*data = out;

	return true;
}

bool text_file_loader::token_double_word(const std::string & rstr_key, uint32_t * data)
{
	return token_integer(rstr_key, (int *) data);
}

bool text_file_loader::token_float(const std::string & rstr_key, float * data)
{
	token_vector * pTokenVector;
	if (!get_token_vector(rstr_key, &pTokenVector))
		return false;

	if (pTokenVector->empty())
	{
		metin2::log::debug( " CTextFileLoader::GetTokenFloat - Failed to find the value {} [{} : {}]", file_name_.c_str(), pcur_node_->group_name.c_str(), rstr_key.c_str());
		return false;
	}

	*data = atof(pTokenVector->at(0).c_str());

	return true;
}

bool text_file_loader::token_vector3(const std::string & rstr_key, vector * vector3)
{
	token_vector * pTokenVector;
	if (!get_token_vector(rstr_key, &pTokenVector))
		return false;

	if (pTokenVector->size() != 3)
	{
		metin2::log::debug( " CTextFileLoader::GetTokenVector3 - This key should have 3 values {} [{} : {}]", file_name_.c_str(), pcur_node_->group_name.c_str(), rstr_key.c_str());
		return false;
	}

	vector3->x = atof(pTokenVector->at(0).c_str());
	vector3->y = atof(pTokenVector->at(1).c_str());
	vector3->z = atof(pTokenVector->at(2).c_str());

	return true;
}


bool text_file_loader::token_position(const std::string & rstr_key, vector * vector_value)
{
	return token_vector3(rstr_key, vector_value);
}

bool text_file_loader::token_string(const std::string & rstr_key, std::string * string_value)
{
	token_vector * pTokenVector;
	if (!get_token_vector(rstr_key, &pTokenVector))
		return false;

	if (pTokenVector->empty())
	{
		metin2::log::debug( " CTextFileLoader::GetTokenString - Failed to find the value {} [{} : {}]", file_name_.c_str(), pcur_node_->group_name.c_str(), rstr_key.c_str());
		return false;
	}

	*string_value = pTokenVector->at(0);

	return true;
}
}
