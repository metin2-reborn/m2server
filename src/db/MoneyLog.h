// vim: ts=8 sw=4
#pragma once

#include <common/length.h>
#include <common/singleton.h>

#include <cstdint>
#include <map>

class CMoneyLog : public singleton<CMoneyLog>
{
    public:
	CMoneyLog();
	~CMoneyLog() override;

	void Save();
	void AddLog(uint8_t bType, uint32_t dwVnum, int iGold);

    private:
	std::map<uint32_t, int> m_MoneyLogContainer[MONEY_LOG_TYPE_MAX_NUM];
};
