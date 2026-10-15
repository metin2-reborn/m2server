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
	bool	read_dragon_soul_table_file(const char * file_name);

	void	dragon_soul_info(uint32_t vnum, uint8_t& type, uint8_t& grade, uint8_t& step, uint8_t& strength) const;
	uint16_t	base_position(item * const item_value) const;
	bool	is_valid_cell_for_this_item(item * const item_value, const item_pos& cell_value) const;
	int		duration(item * const item_value) const;
	
	bool	extract_dragon_heart(character * ch, item * item_value, item * extractor = nullptr);

	bool	pull_out(character * ch, item_pos dest_cell, item *& item_value, item * extractor = nullptr);

	bool	do_refine_grade(character * ch, item_pos (&item_poses)[DRAGON_SOUL_REFINE_GRID_SIZE]);
	bool	do_refine_step(character * ch, item_pos (&item_poses)[DRAGON_SOUL_REFINE_GRID_SIZE]);
	bool	do_refine_strength(character * ch, item_pos (&item_poses)[DRAGON_SOUL_REFINE_GRID_SIZE]);
	
	bool	dragon_soul_item_initialize(item * item_value);

	bool	is_time_left_dragon_soul(item * item_value) const;
	int		left_time(item * item_value) const;
	bool	activate_dragon_soul(item * item_value);
	bool	deactivate_dragon_soul(item * item_value, bool skip_refresh_owner_active_state = false);
	bool	is_active_dragon_soul(item * item_value) const;
private:
	void	send_refine_result_packet(character * ch, uint8_t sub_header, const item_pos& pos);

	void	refresh_dragon_soul_state(character * ch);

	uint32_t	make_dragon_soul_vnum(uint8_t type, uint8_t grade, uint8_t step, uint8_t refine);
	bool	put_attributes(item * ds);
	bool	refresh_item_attributes(item * ds);

	std::unique_ptr<dragon_soul_table>	table_;
};
}
