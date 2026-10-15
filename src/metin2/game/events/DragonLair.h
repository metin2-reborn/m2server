#pragma once

#include <metin2/game/app/typedef.h>

#include <metin2/common/singleton.h>

#include <cstdint>
#include <unordered_map>

namespace metin2::game::inline events {

class CDragonLair
{
	public:
		explicit CDragonLair (uint32_t dwGuildID);
		virtual ~CDragonLair ();

		uint32_t GetEstimatedTime () const;

		void OnDragonDead (CHARACTER * pDragon);

	private:
		uint32_t StartTime_;
		uint32_t GuildID_;
};

class CDragonLairManager : public singleton<CDragonLairManager>
{
	public:
		CDragonLairManager ();
		~CDragonLairManager () override;

		bool Start (int32_t MapIndexFrom, int32_t BaseMapIndex, uint32_t GuildID);
		void OnDragonDead (CHARACTER * pDragon, uint32_t KillerGuildID);

	private:
		std::unordered_map<uint32_t, CDragonLair*> LairMap_;
};
}
