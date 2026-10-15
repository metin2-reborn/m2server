#pragma once

#include <metin2/game/app/typedef.h>

#include <metin2/common/singleton.h>

#include <cstdint>
#include <unordered_map>

namespace metin2::game::inline events {

class dragon_lair
{
	public:
		explicit dragon_lair (uint32_t dwGuildID);
		virtual ~dragon_lair ();

		uint32_t estimated_time () const;

		void on_dragon_dead (character * pDragon);

	private:
		uint32_t start_time_;
		uint32_t guild_id_;
};

class dragon_lair_manager : public singleton<dragon_lair_manager>
{
	public:
		dragon_lair_manager ();
		~dragon_lair_manager () override;

		bool start (int32_t MapIndexFrom, int32_t BaseMapIndex, uint32_t GuildID);
		void on_dragon_dead (character * pDragon, uint32_t KillerGuildID);

	private:
		std::unordered_map<uint32_t, dragon_lair*> lair_map_;
};
}
