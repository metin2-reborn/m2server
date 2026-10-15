#pragma once

#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>

#include <metin2/common/singleton.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

namespace metin2::game::inline social {

namespace marriage
{
	const uint32_t WEDDING_MAP_INDEX = 81;
	typedef character_set charset_t;

	class wedding_map
	{
		public:
			wedding_map(uint32_t map_index, uint32_t pid1, uint32_t pid2);
			~wedding_map();

			uint32_t map_index() { return map_index_; }

			void warp_all();
			void destroy_all();
			void notice(const char* psz);
			void set_ended();

			void inc_member(character * ch);
			void dec_member(character * ch);
			bool is_member(character * ch );

			void set_dark(bool set);
			void set_snow(bool set);
			void set_music(bool set, const char* music_file_name);

			bool is_playing_music();

			void send_local_event(character * ch);

			void shout_in_map(uint8_t type, const char* msg);
		private:

			const char* build_command_play_music(char* command, size_t cmd_len, uint8_t set, const char* music_file_name); 

		private:
			uint32_t map_index_;
			event_ptr end_event_;
			charset_t set_pk_chr_;

			bool is_dark_;
			bool is_snow_;
			bool is_music_;

			uint32_t pi_d1_;
			uint32_t pi_d2_;

			std::string music_file_name_;
	};

	class wedding_manager : public singleton<wedding_manager>
	{
		public:
			wedding_manager();
			~wedding_manager() override;

			bool is_wedding_map(uint32_t map_index);

			void request(uint32_t pid1, uint32_t pid2);
			bool end(uint32_t map_index);

			void destroy_wedding_map(wedding_map* map);

			wedding_map* find(uint32_t map_index);

		private:
			uint32_t create_wedding_map(uint32_t pid1, uint32_t pid2);

		private:

			std::map<uint32_t, wedding_map*> wedding_;
	};
}
}
