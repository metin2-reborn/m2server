#pragma once

#include <metin2/game/app/typedef.h>

#include <metin2/proto/length.h>
#include <metin2/common/singleton.h>

#include <cstdint>
#include <memory>

namespace metin2::game::inline characters {
    class character;
}

namespace metin2::game::inline items {

class item;

class dragon_soul_table;

class ds_manager : public singleton<ds_manager>
{
public:
	ds_manager();
	~ds_manager() override;
	bool	read_dragon_soul_table_file(const char * c_pszFileName);

	void	dragon_soul_info(uint32_t dwVnum, uint8_t& bType, uint8_t& bGrade, uint8_t& bStep, uint8_t& bRefine) const;
	// fixme : use titempos
	uint16_t	base_position(item * const pItem) const;
	bool	is_valid_cell_for_this_item(item * const pItem, const item_pos& Cell) const;
	int		duration(item * const pItem) const;
	
	// Takes a dragon soul stone and extracts a specific dragon heart
	bool	extract_dragon_heart(character * ch, item * pItem, item * pExtractor = nullptr);

	// Decides whether removing a specific dragon soul stone (pItem) from the equipment window succeeds, 
	// and gives by-products on failure. (by-products are defined in dragon_soul_table.txt)
	// If DestCell is invalid, on success the dragon soul stone is automatically placed in an empty slot.
	// On failure, the dragon soul stone (pItem) is deleted.
	// If there is an extractor item, the extraction success rate increases by pExtractor->GetValue(0)%.
	// By-products are always added automatically.
	bool	pull_out(character * ch, item_pos DestCell, item *& pItem, item * pExtractor = nullptr);

	// Dragon soul stone upgrade function
	bool	do_refine_grade(character * ch, item_pos (&aItemPoses)[DRAGON_SOUL_REFINE_GRID_SIZE]);
	bool	do_refine_step(character * ch, item_pos (&aItemPoses)[DRAGON_SOUL_REFINE_GRID_SIZE]);
	bool	do_refine_strength(character * ch, item_pos (&aItemPoses)[DRAGON_SOUL_REFINE_GRID_SIZE]);
	
	bool	dragon_soul_item_initialize(item * pItem);

	bool	is_time_left_dragon_soul(item * pItem) const;
	int		left_time(item * pItem) const;
	bool	activate_dragon_soul(item * pItem);
	bool	deactivate_dragon_soul(item * pItem, bool bSkipRefreshOwnerActiveState = false);
	bool	is_active_dragon_soul(item * pItem) const;
private:
	void	send_refine_result_packet(character * ch, uint8_t bSubHeader, const item_pos& pos);

	// Looks at the character's dragon soul deck and, if no dragon soul stone is active, turns off the character's dragon soul active state.
	void	refresh_dragon_soul_state(character * ch);

	uint32_t	make_dragon_soul_vnum(uint8_t bType, uint8_t grade, uint8_t step, uint8_t refine);
	bool	put_attributes(item * pDS);
	bool	refresh_item_attributes(item * pItem);

	std::unique_ptr<dragon_soul_table>	table_;
};
}
