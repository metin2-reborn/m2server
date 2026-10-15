#pragma once

#include "typedef.h"

#include <common/singleton.h>
#include <common/tables.h>

#include <cstdint>
#include <iterator>

namespace metin2::game {

class CMonarch : public singleton<CMonarch>
{
	public:
		CMonarch();
		~CMonarch() override;

		bool Initialize();

		int HealMyEmpire(CHARACTER * ch, uint32_t price);
		void SetMonarchInfo(MonarchInfo * pInfo);

		bool IsMonarch(uint32_t pid, uint8_t bEmpire);
		bool IsMoneyOk(int price, uint8_t bEmpire);
		bool SendtoDBAddMoney(int Money, uint8_t bEmpire, CHARACTER * ch);
		bool SendtoDBDecMoney(int Money, uint8_t bEmpire, CHARACTER * ch);

		bool AddMoney(int Money, uint8_t bEmpire);
		bool DecMoney(int Money, uint8_t bEmpire);
		int GetMoney(uint8_t bEmpire);

		MonarchInfo* GetMonarch();

		uint32_t GetMonarchPID(uint8_t Empire);

		bool IsPowerUp(uint8_t Empire);
		bool IsDefenceUp(uint8_t Empire);

		int	GetPowerUpCT(uint8_t Empire)
		{
			return Empire < std::size(m_PowerUpCT) ? m_PowerUpCT[Empire] : 0;
		}

		bool CheckPowerUpCT(uint8_t Empire);
		bool CheckDefenseUpCT(uint8_t Empire);

		int	GetDefenseUpCT(uint8_t Empire)
		{
			return Empire < std::size(m_DefenseUpCT) ? m_DefenseUpCT[Empire] : 0;
		}

		void PowerUp(uint8_t Empire, bool On);
		void DefenseUp(uint8_t Empire, bool On);

	private:
		MonarchInfo m_MonarchInfo;	

		int m_PowerUp[4]; ///< Monarch's Lion's Roar
		int m_DefenseUp[4]; ///< Monarch's Diamond Fist

		int m_PowerUpCT[4]; ///< Monarch's Lion's Roar
		int m_DefenseUpCT[4]; ///< Monarch's Diamond Fist
};

bool IsMonarchWarpZone (int map_idx);
}
