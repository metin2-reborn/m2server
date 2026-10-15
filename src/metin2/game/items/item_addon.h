#pragma once

#include <metin2/game/app/typedef.h>

#include <metin2/common/singleton.h>

namespace metin2::game::inline items {

class CItemAddonManager : public singleton<CItemAddonManager>
{
	public:
		CItemAddonManager();
		~CItemAddonManager() override;

		void ApplyAddonTo(int iAddonType, CItem * pItem);
};
}
