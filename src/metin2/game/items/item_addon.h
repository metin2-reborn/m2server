#pragma once

#include <metin2/game/app/typedef.h>

#include <metin2/common/singleton.h>

namespace metin2::game::inline items {

class item_addon_manager : public singleton<item_addon_manager>
{
	public:
		item_addon_manager();
		~item_addon_manager() override;

		void apply_addon_to(int addon_type, item * item_value);
};
}
