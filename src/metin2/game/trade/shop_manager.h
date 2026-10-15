#pragma once
#include <metin2/game/app/typedef.h>

#include <metin2/common/singleton.h>
#include <metin2/proto/tables.h>

#include <cstdint>
#include <map>

namespace metin2::game::inline trade {

class shop;

class shop_manager : public singleton<shop_manager>
{
public:
	typedef std::map<uint32_t, shop *> shop_map;

public:
	shop_manager();
	~shop_manager() override;

	bool	initialize(shop_table * table, int size);
	void	destroy();

	shop *	get(uint32_t vnum);
	shop *	by_npc_vnum(uint32_t vnum);

	bool	start_shopping(character * chr, character * chr_shop_keeper, int shop_vnum = 0);
	void	stop_shopping(character * ch);

	void	buy(character * ch, uint8_t pos);
	void	sell(character * ch, uint8_t cell, uint8_t count_value=0);

	shop *	create_pc_shop(character * ch, shop_item_table * table, uint8_t item_count);
	shop *	find_pc_shop(uint32_t vid);
	void	destroy_pc_shop(character * ch);

private:
	shop_map	map_pk_shop_;
	shop_map	map_pk_shop_by_npc_vnum_;
	shop_map	map_pk_shop_by_pc_;

	bool	read_shop_table_ex(const char* file_name);
};
}
