#include <metin2/game/social/wedding.h>

#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/app/config.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/protocol/desc_manager.h>
#include <metin2/game/app/event.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/text/locale_service.h>
#include <metin2/game/world/regen.h>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/app/typedef.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/log/log.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>

namespace metin2::game::inline social {

namespace marriage
{
	EVENTINFO(wedding_map_info)
	{
		game::social::marriage::wedding_map * wedding_map;
		int step;

		wedding_map_info()
		: wedding_map( nullptr )
		, step( 0 )
		{
		}
	};

	EVENTFUNC(wedding_end_event)
	{
		wedding_map_info* info = dynamic_cast<wedding_map_info*>( event->info );

		if ( info == nullptr )
		{
			metin2::log::error("Wedding end event fired without event info");
			return 0;
		}
		
		wedding_map* pMap = info->wedding_map;

		if (info->step == 0)
		{
			++info->step;
			pMap->warp_all(); 
			return passes_per_sec(15);
		}
		wedding_manager::instance().destroy_wedding_map(pMap);
		return 0;
	}

	wedding_map::wedding_map(uint32_t map_index, uint32_t pid1, uint32_t pid2) :
		map_index_(map_index),
		end_event_(nullptr),
		is_dark_(false),
		is_snow_(false),
		is_music_(false),
		pi_d1_(pid1),
		pi_d2_(pid2)
	{
	}

	wedding_map::~wedding_map()
	{
		event_cancel(&end_event_);
	}

	void wedding_map::set_ended()
	{
		if (end_event_)
		{
			metin2::log::error("Wedding map already has an end event ({})", static_cast<const void *>(static_cast<void*>(end_event_.get())));
			return;
		}

		wedding_map_info* info = alloc_event_info<wedding_map_info>();

		info->wedding_map = this;

		end_event_ = event_create(wedding_end_event, info, passes_per_sec(5));

		notice(lc_text("결혼식이 종료됩니다."));
		notice(lc_text("자동으로 나가게됩니다."));

		for (auto it = set_pk_chr_.begin(); it != set_pk_chr_.end(); ++it)
		{
			character * ch = *it;
			if (ch->player_id() == pi_d1_ || ch->player_id() == pi_d2_)
				continue;

			if (ch->level() < 10)
				continue;

			ch->auto_give_item(27002, 5);
		}
	}

	struct f_notice
	{
		f_notice(const char * psz_value) :
			psz(psz_value)
		{
		}

		void operator() (character * ch)
		{
			ch->chat_packet(CHAT_TYPE_NOTICE, "%s", psz);
		}

		const char * psz;
	};

	void wedding_map::notice(const char * psz)
	{
		f_notice f(psz);
		std::for_each(set_pk_chr_.begin(), set_pk_chr_.end(), f);
	}

	struct f_warp_everyone
	{
		void operator() (character * ch)
		{
			if (ch->is_pc())
			{
				ch->exit_to_saved_location();
			}
		}
	};

	void wedding_map::warp_all()
	{
		f_warp_everyone f;
		std::for_each(set_pk_chr_.begin(), set_pk_chr_.end(), f);
	}

	struct f_destroy_everyone
	{
		void operator() (character * ch)
		{
			metin2::log::debug("Wedding map: removing {}", ch->name());

			if (ch->get_desc())
				desc_manager::instance().destroy_desc(ch->get_desc());
			else
				character_manager::instance().destroy_character(ch);
		}
	};

	void wedding_map::destroy_all()
	{
		metin2::log::info("Wedding map: removing {:d} characters", set_pk_chr_.size());
		
		f_destroy_everyone f;

		for (charset_t::iterator it = set_pk_chr_.begin(); it != set_pk_chr_.end(); it = set_pk_chr_.begin())
			f(*it);
	}

	void wedding_map::inc_member(character * ch)
	{
		if (is_member(ch) == true)
			return;

		set_pk_chr_.insert(ch);

		send_local_event(ch);

		if (ch->level() < 10)
		{
			ch->set_observer_mode(true);
		}
	}

	void wedding_map::dec_member(character * ch)
	{
		if (is_member(ch) == false)
			return;

		set_pk_chr_.erase(ch);

		if (ch->level() < 10)
		{
			ch->set_observer_mode(false);
		}
	}

	auto wedding_map::is_member(character * ch) -> bool
	{
		if (set_pk_chr_.size() <= 0)
			return false;

		return set_pk_chr_.find(ch) != set_pk_chr_.end();
	}

	void wedding_map::shout_in_map(uint8_t type, const char* msg)
	{
		for (auto it = set_pk_chr_.begin(); it != set_pk_chr_.end(); ++it)
		{
			character * ch = *it;
			ch->chat_packet(CHAT_TYPE_COMMAND, msg);
		}
	}

	void wedding_map::set_music(bool set, const char* music_file_name)
	{
		if (is_music_ != set)
		{
			is_music_ = set;
			music_file_name_ = music_file_name;

			char szCommand[256];
			if (is_music_)
			{
				shout_in_map(CHAT_TYPE_COMMAND, build_command_play_music(szCommand, sizeof(szCommand), 1, music_file_name_.c_str()));
			}
			else
			{
				shout_in_map(CHAT_TYPE_COMMAND, build_command_play_music(szCommand, sizeof(szCommand), 0, "default"));
			}
		} 
	}

	void wedding_map::set_dark(bool set)
	{
		if (is_dark_ != set)
		{
			is_dark_ = set;

			if (is_dark_)
				shout_in_map(CHAT_TYPE_COMMAND, "DayMode dark");
			else
				shout_in_map(CHAT_TYPE_COMMAND, "DayMode light");
		}
	}

	void wedding_map::set_snow(bool set)
	{
		if (is_snow_ != set)
		{
			is_snow_ = set;

			if (is_snow_)
				shout_in_map(CHAT_TYPE_COMMAND, "xmas_snow 1");
			else
				shout_in_map(CHAT_TYPE_COMMAND, "xmas_snow 0");
		}
	}

	auto wedding_map::is_playing_music() -> bool
	{
		return is_music_;
	}

	void wedding_map::send_local_event(character * ch)
	{
		char szCommand[256];

		if (is_dark_)
			ch->chat_packet(CHAT_TYPE_COMMAND, "DayMode dark");
		if (is_snow_)
			ch->chat_packet(CHAT_TYPE_COMMAND, "xmas_snow 1");
		if (is_music_)
			ch->chat_packet(CHAT_TYPE_COMMAND, build_command_play_music(szCommand, sizeof(szCommand), 1, music_file_name_.c_str()));	
	}

	auto wedding_map::build_command_play_music(char* command, size_t cmd_len, uint8_t set, const char* music_file_name) -> const char*
	{
		if (cmd_len < 1)
		{
			command[0] = '\0';
			return "PlayMusic 0 CommandLengthError";
		}

		snprintf(command, cmd_len, "PlayMusic %d %s", set, music_file_name);
		return command;
	}

	wedding_manager::wedding_manager()
	{
	}

	wedding_manager::~wedding_manager()
	{
	}

	auto wedding_manager::is_wedding_map(uint32_t map_index) -> bool
	{
		return (map_index == WEDDING_MAP_INDEX || map_index / 10000 == WEDDING_MAP_INDEX);
	}

	auto wedding_manager::find(uint32_t map_index) -> wedding_map*
	{
		auto it = wedding_.find(map_index);

		if (it == wedding_.end())
			return nullptr;

		return it->second;
	}

	auto wedding_manager::create_wedding_map(uint32_t pid1, uint32_t pid2) -> uint32_t
	{
		sectree_manager& rkSecTreeMgr = sectree_manager::instance();

		uint32_t dwMapIndex = rkSecTreeMgr.create_private_map(WEDDING_MAP_INDEX);

		if (!dwMapIndex)
		{
			metin2::log::error("Cannot create private copy of map {:d} for the wedding of {:d} and {:d}", WEDDING_MAP_INDEX, pid1, pid2);
			return 0;
		}

		wedding_.insert(std::make_pair(dwMapIndex, new wedding_map(dwMapIndex, pid1, pid2)));


		sectree_map * pkSectreeMap = rkSecTreeMgr.map(dwMapIndex);
		if (pkSectreeMap == nullptr) {
			return 0;
		}
		std::string st_weddingMapRegenFileName;
		st_weddingMapRegenFileName.reserve(64);
		st_weddingMapRegenFileName  = locale_service_get_map_path();
		st_weddingMapRegenFileName += "/metin2_map_wedding_01/npc.txt";

		if (!regen_do(st_weddingMapRegenFileName.c_str(), dwMapIndex, pkSectreeMap->setting.base_x, pkSectreeMap->setting.base_y, nullptr, true))
		{
			metin2::log::error("Cannot spawn wedding of {:d} and {:d}: regen {} failed on map {:d} (base {:d}, {:d})", pid1, pid2, st_weddingMapRegenFileName.c_str(), dwMapIndex, pkSectreeMap->setting.base_x, pkSectreeMap->setting.base_y);
		}
		else
		{
			metin2::log::info("Wedding map created for {:d} and {:d}: regen {} on map {:d} (base {:d}, {:d})", pid1, pid2, st_weddingMapRegenFileName.c_str(), dwMapIndex, pkSectreeMap->setting.base_x, pkSectreeMap->setting.base_y);
		}

		return dwMapIndex;
	}
	
	void wedding_manager::destroy_wedding_map(wedding_map* map)
	{
		metin2::log::info("Wedding map {:d} destroyed", map->map_index());
		map->destroy_all();
		wedding_.erase(map->map_index());
		sectree_manager::instance().destroy_private_map(map->map_index());
		delete map;
	}

	auto wedding_manager::end(uint32_t map_index) -> bool
	{
		auto it = wedding_.find(map_index);

		if (it == wedding_.end())
			return false;

		it->second->set_ended();
		return true;
	}

	void wedding_manager::request(uint32_t pid1, uint32_t pid2)
	{
		if (map_allow_find(WEDDING_MAP_INDEX))
		{
			uint32_t dwMapIndex = create_wedding_map(pid1, pid2);

			if (!dwMapIndex)
			{
				metin2::log::error("Cannot create wedding map for {:d} and {:d}", pid1, pid2);
				return;
			}

			packet_wedding_ready p;
			p.pi_d1 = pid1;
			p.pi_d2 = pid2;
			p.map_index = dwMapIndex;

			db_clientdesc->db_packet(HEADER_GD_WEDDING_READY, 0, &p, sizeof(p));
		}
	}

}
}
