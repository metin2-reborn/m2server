#pragma once

#include "typedef.h"

#include <common/singleton.h>

class CItemAddonManager : public singleton<CItemAddonManager>
{
	public:
		CItemAddonManager();
		~CItemAddonManager() override;

		void ApplyAddonTo(int iAddonType, CItem * pItem);
};
