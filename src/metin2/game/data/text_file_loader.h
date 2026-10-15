#pragma once

#include <metin2/game/data/file_loader.h>
#include <metin2/game/world/vector.h>

#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <vector>

namespace metin2::game::inline data {

typedef std::map<std::string, token_vector>	token_vector_map;

class text_file_loader
{
	public:
		struct group_node {
			std::string group_name;

			token_vector_map local_token_vector_map;

			group_node * parent_node;
			std::vector<group_node*> child_node_vector;
		};

	public:
		static void destroy_system();

	public:
		text_file_loader();
		virtual ~text_file_loader();

		bool load(const char * c_szFileName);

		void set_top();
		uint32_t child_node_count();
		bool set_child_node(const char * c_szKey);
		bool set_child_node(const std::string & c_rstrKeyHead, uint32_t dwIndex);
		bool set_child_node(uint32_t dwIndex);
		bool set_parent_node();
		bool current_node_name(std::string * pstrName);

		bool get_token_vector(const std::string & c_rstrKey, token_vector ** ppTokenVector);
		bool token_integer(const std::string & c_rstrKey, int * pData);
		bool token_double_word(const std::string & c_rstrKey, uint32_t * pData);
		bool token_float(const std::string & c_rstrKey, float * pData);

		bool token_vector3(const std::string & c_rstrKey, vector * pVector3);

		bool token_position(const std::string & c_rstrKey, vector * pVector);
		bool token_string(const std::string & c_rstrKey, std::string * pString);

	protected:
		bool load_group(group_node * pGroupNode);

	protected:
		std::string					file_name_;
		uint32_t						dwcur_line_index_;
		const void *					mc_p_data_;

		memory_text_file_loader				file_loader_;

		group_node					global_node_;
		group_node *					pcur_node_;

	private:
		// every group node of every loaded file; released by DestroySystem
		static std::deque<group_node>			group_nodes_;
};
}
