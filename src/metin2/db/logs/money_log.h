// vim: ts=8 sw=4
#pragma once

#include <metin2/proto/length.h>
#include <metin2/common/singleton.h>

#include <cstdint>
#include <map>

namespace metin2::db::inline logs {

class money_log : public singleton<money_log>
{
    public:
	money_log();
	~money_log() override;

	void save();
	void add_log(uint8_t type, uint32_t vnum, int gold);

    private:
	std::map<uint32_t, int> money_log_container_[MONEY_LOG_TYPE_MAX_NUM];
};
}
