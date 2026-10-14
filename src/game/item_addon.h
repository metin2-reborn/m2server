#ifndef __ITEM_ADDON_H
#define __ITEM_ADDON_H

#include "typedef.h"

#include <common/singleton.h>

class CItemAddonManager : public singleton<CItemAddonManager>
{
	public:
		CItemAddonManager();
		~CItemAddonManager() override;

		void ApplyAddonTo(int iAddonType, CItem * pItem);
};

#endif
