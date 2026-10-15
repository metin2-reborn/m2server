#pragma once

#include <metin2/game/guilds/mark_image.h>

#include <metin2/common/singleton.h>

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace metin2::game::inline guilds {

class guild_mark_manager : public singleton<guild_mark_manager>
{
	public:
		enum
		{
			MAX_IMAGE_COUNT = 5,
			INVALID_MARK_ID = 0xffffffff,
		};

		// Symbol
		struct guild_symbol
		{
			uint32_t crc;
			std::vector<uint8_t> raw;
		};

		guild_mark_manager();
		~guild_mark_manager() override;

		const guild_symbol * get_guild_symbol(uint32_t guild_id);
		bool load_symbol(const char* filename);
		void save_symbol(const char* filename);
		void upload_symbol(uint32_t guild_id, int size, const uint8_t* pby_data);

		//
		// Mark
		//
		void set_mark_path_prefix(const char * prefix);

		bool load_mark_index(); // Load mark index (server only)
		bool save_mark_index(); // Save mark index

		void load_mark_images(); // Load all mark images
		void save_mark_image(uint32_t img_idx); // Save mark image

		bool mark_image_filename(uint32_t img_idx, std::string & path) const;
		bool add_mark_id_by_guild_id(uint32_t guild_id, uint32_t mark_id);
		uint32_t mark_count() const;
		uint32_t mark_id(uint32_t guild_id);

		// SERVER
		void copy_mark_idx(char * pc_buf) const;
		uint32_t save_mark(uint32_t guild_id, uint8_t * mark_image);
		void delete_mark(uint32_t guild_id);
		void diff_blocks(uint32_t img_idx, const uint32_t * crc_list, std::map<uint8_t, const guild_mark_block *> & diff_blocks_value);

		// CLIENT

	private:
		// 
		// Mark
		//
		uint32_t alloc_mark_id(uint32_t guild_id);

		guild_mark_image * image(uint32_t img_idx);

		std::map<uint32_t, std::unique_ptr<guild_mark_image>> idx_image_; // index = image index
		std::map<uint32_t, uint32_t> gid_mark_id_; // index = guild id

		std::set<uint32_t> free_mark_id_;
		std::string		path_prefix_;

	private:
		//
		// Symbol
		//
		std::map<uint32_t, guild_symbol> symbol_;
};
}
