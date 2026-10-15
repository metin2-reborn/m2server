#pragma once

#include <metin2/game/app/typedef.h>

#include <metin2/common/singleton.h>

#include <cstdint>
#include <unordered_map>

namespace metin2::game::inline events {

class dragon_lair
{
	public:
		explicit dragon_lair (uint32_t guild_id);
		virtual ~dragon_lair ();

		uint32_t estimated_time () const;

		void on_dragon_dead (character * dragon);

	private:
		uint32_t start_time_;
		uint32_t guild_id_;
};

class dragon_lair_manager : public singleton<dragon_lair_manager>
{
	public:
		dragon_lair_manager ();
		~dragon_lair_manager () override;

		bool start (int32_t map_index_from, int32_t base_map_index, uint32_t guild_id);
		void on_dragon_dead (character * dragon, uint32_t killer_guild_id);

	private:
		std::unordered_map<uint32_t, dragon_lair*> lair_map_;
};
}
