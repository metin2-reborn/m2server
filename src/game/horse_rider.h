#pragma once

#include "cmd.h"
#include "event.h"

#include <common/tables.h>

#include <cstdint>

const int HORSE_MAX_LEVEL = 30;

struct THorseStat
{
	int iMinLevel;      // Minimum level to ride
	int iNPCRace;
	int iMaxHealth;     // Horse max HP
	int iMaxStamina;    // Horse max stamina
	int iST;
	int iDX;
	int iHT;
	int iIQ;
	int iDamMean;
	int iDamMin;
	int iDamMax;
	int iArmor;
};

extern THorseStat c_aHorseStat[HORSE_MAX_LEVEL+1];

class CHorseRider
{
	public:
		CHorseRider();
		virtual ~CHorseRider();
		
		uint8_t		GetHorseLevel() const { return m_Horse.bLevel; }
		uint8_t		GetHorseGrade();
		short		GetHorseHealth() const	{ return m_Horse.sHealth; }
		short		GetHorseStamina() const	{ return m_Horse.sStamina; }
		short		GetHorseMaxHealth();
		short		GetHorseMaxStamina();

		int		GetHorseST()		{ return c_aHorseStat[GetHorseLevel()].iST; }
		int		GetHorseDX()		{ return c_aHorseStat[GetHorseLevel()].iDX; }
		int		GetHorseHT()		{ return c_aHorseStat[GetHorseLevel()].iHT; }
		int		GetHorseIQ()		{ return c_aHorseStat[GetHorseLevel()].iIQ; }
		int		GetHorseArmor()		{ return c_aHorseStat[GetHorseLevel()].iArmor; }

		virtual bool ReviveHorse();
		void FeedHorse();
		virtual void HorseDie();

		bool IsHorseRiding() const		{ return m_Horse.bRiding; }

		void ResetHorseHealthDropTime();

		virtual void SetHorseLevel(int iLevel);

		void EnterHorse();

		virtual void SendHorseInfo() {}
		virtual void ClearHorseInfo() {}

		virtual void UpdateRideTime(int interval) {}

	protected:
		void SetHorseData(const THorseInfo& crInfo);
		const THorseInfo& GetHorseData() const { return m_Horse; }

		void UpdateHorseDataByLogoff(uint32_t dwLogoffTime);

		virtual bool StartRiding();
		virtual bool StopRiding();

		virtual	uint32_t GetMyHorseVnum() const { return 20030; }

	private:
		void UpdateHorseStamina(int iStamina, bool bSend = true);

		void StartStaminaConsumeEvent();
		void StartStaminaRegenEvent();

		void UpdateHorseHealth(int iHealth, bool bSend = true);
		void CheckHorseHealthDropTime(bool bSend = true);

		void Initialize();
		void Destroy();

		THorseInfo m_Horse;

		LPEVENT	m_eventStaminaRegen;
		LPEVENT	m_eventStaminaConsume;

		friend EVENTFUNC(horse_stamina_regen_event);
		friend EVENTFUNC(horse_stamina_consume_event);
		friend ACMD(do_horse_set_stat);
};
