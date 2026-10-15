#pragma once

#include <metin2/game/world/sectree.h>
#include <metin2/game/app/typedef.h>

#include <metin2/common/singleton.h>
#include <metin2/log/log.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace metin2::game::inline world {

struct map_region {
	int			index;
	int			sx, sy, ex, ey;
	pixel_position	pos_spawn;

	bool		empire_spawn_different;
	pixel_position	pos_empire[3];

	std::string		map_name;
};

struct area_info
{
	int sx, sy, ex, ey, dir;
	area_info(int sx, int sy, int ex, int ey, int dir)
		: sx(sx), sy(sy), ex(ex), ey(ey), dir(dir)
		{}
};

struct npc_info
{
	uint8_t type;
	const char* name;
	int32_t x, y;
	npc_info(uint8_t bType, const char* name, int32_t x, int32_t y)
		: type(bType), name(name), x(x), y(y)
		{}
};

typedef std::map<std::string, area_info> area_map;

struct map_setting {
	int			index;
	int			cell_scale;
	int			base_x;
	int			base_y;
	int			width;
	int			height;

	pixel_position	pos_spawn;
};

class sectree_map
{
	public:
		typedef std::map<uint32_t, sectree *> map_type;

		sectree_map();
		sectree_map(sectree_map & r);
		virtual ~sectree_map();

		bool add(uint32_t key, sectree * sectree) {
			return map_.insert(map_type::value_type(key, sectree)).second;
		}

		sectree *	find(uint32_t dwPackage);
		sectree *	find(uint32_t x, uint32_t y);
		void		build();

		map_setting	setting;

		template< typename Func >
		void for_each( Func & rfunc )
		{
			// <Factor> Using snapshot copy to avoid side-effects
			f_collect_entity collector;
			std::map<uint32_t, sectree *>::iterator it = map_.begin();
			for ( ; it != map_.end(); ++it)
			{
				sectree * sectree = it->second;
				sectree->for_each_entity(collector);
			}
			collector.for_each(rfunc);
		}

		void dump_all_to_sys_err() {
			sectree_map::map_type::iterator i;
			for (i = map_.begin(); i != map_.end(); ++i)
			{
				metin2::log::error("SECTREE {:x}({:d}, {:d})", i->first, i->first & 0xffff, i->first >> 16);
			}
		}

	private:
		map_type map_;
};

enum attr_region_mode
{
	ATTR_REGION_MODE_SET,
	ATTR_REGION_MODE_REMOVE,
	ATTR_REGION_MODE_CHECK,
};

class sectree_manager : public singleton<sectree_manager>
{
	public:
		sectree_manager();
		~sectree_manager() override;

		sectree_map * map(int32_t lMapIndex);
		sectree * 	get(uint32_t dwIndex, uint32_t package);
		sectree * 	get(uint32_t dwIndex, uint32_t x, uint32_t y);

		template< typename Func >
		void for_each( int iMapIndex, Func & rfunc )
		{
			sectree_map * pSecMap = sectree_manager::instance().map( iMapIndex );
			if ( pSecMap )
			{
				pSecMap->for_each( rfunc );
			}
		}
		
		int		load_setting_file(int32_t lIndex, const char * c_pszSettingFileName, map_setting & r_setting);
		bool		load_map_region(const char * c_pszFileName, map_setting & r_Setting, const char * c_pszMapName);
		int		build(const char * c_pszListFileName, const char* c_pszBasePath);
		sectree_map * build_sectree_from_setting(map_setting & r_setting);
		bool		load_attribute(sectree_map * pkMapSectree, const char * c_pszFileName, map_setting & r_setting);
		void		load_dungeon(int iIndex, const char * c_pszFileName);
		bool		valid_location(int32_t lMapIndex, int32_t x, int32_t y, int32_t & r_lValidMapIndex, pixel_position & r_pos, uint8_t empire = 0);
		bool		recall_position_by_empire(int iMapIndex, uint8_t bEmpire, pixel_position & r_pos);

		const map_region *	get_map_region(int32_t lMapIndex);
		int			map_index(int32_t x, int32_t y);
		const map_region *	find_region_by_partial_name(const char* szMapName);

		bool		map_base_position(int32_t x, int32_t y, pixel_position & r_pos);
		bool		map_base_position_by_map_index(int32_t lMapIndex, pixel_position & r_pos);
		bool		movable_position(int32_t lMapIndex, int32_t x, int32_t y, pixel_position & pos);
		bool		is_movable_position(int32_t lMapIndex, int32_t x, int32_t y);
		bool		center_position_of_map(int32_t lMapIndex, pixel_position & r_pos);
		bool        random_location(int32_t lMapIndex, pixel_position & r_pos, uint32_t dwCurrentX = 0, uint32_t dwCurrentY = 0, int iMaxDistance = 0);

		int32_t		create_private_map(int32_t lMapIndex);	// returns new private map index, returns 0 when fail
		void		destroy_private_map(int32_t lMapIndex);

		area_map&	dungeon_area(int32_t lMapIndex);
		void		send_npc_position(character * ch);
		void		insert_npc_position(int32_t lMapIndex, uint8_t bType, const char* szName, int32_t x, int32_t y);

		uint8_t		empire_from_map_index(int32_t lMapIndex);

		void		purge_monsters_in_map(int32_t lMapIndex);
		void		purge_stones_in_map(int32_t lMapIndex);
		size_t		monster_count_in_map(int32_t lMapIndex);
		size_t		monster_count_in_map(int32_t lMpaIndex, uint32_t dwVnum);

		/// Performs a specific operation on the Sectree Attribute for an area.
		/**
		 * @param [in]	lMapIndex Map index to apply to
		 * @param [in]	lStartX leftmost coordinate of the rectangle
		 * @param [in]	lStartY topmost coordinate of the rectangle
		 * @param [in]	lEndX rightmost coordinate of the rectangle
		 * @param [in]	lEndY bottommost coordinate of the rectangle
		 * @param [in]	lRotate rotation angle of the area
		 * @param [in]	dwAttr Attribute to apply
		 * @param [in]	mode type of operation on the Attribute
		 */
		bool		for_attr_region(int32_t lMapIndex, int32_t lStartX, int32_t lStartY, int32_t lEndX, int32_t lEndY, int32_t lRotate, uint32_t dwAttr, attr_region_mode mode);

		bool		save_attribute_to_image(int lMapIndex, const char * c_pszFileName, sectree_map * pMapSrc = nullptr);

	private:

		/// Performs a specific operation on the Sectree Attribute for a right-angled rectangular area.
		/**
		 * @param [in]	lMapIndex Map index to apply to
		 * @param [in]	lCX coordinate of the leftmost Cell of the rectangle
		 * @param [in]	lCY coordinate of the topmost Cell of the rectangle
		 * @param [in]	lCW width of the rectangle in Cells
		 * @param [in]	lCH height of the rectangle in Cells
		 * @param [in]	lRotate rotation angle (right angle)
		 * @param [in]	dwAttr Attribute to apply
		 * @param [in]	mode type of operation on the Attribute
		 */
		bool		for_attr_region_right_angle( int32_t lMapIndex, int32_t lCX, int32_t lCY, int32_t lCW, int32_t lCH, int32_t lRotate, uint32_t dwAttr, attr_region_mode mode );

		/// Performs a specific operation on the Sectree Attribute for a non-right-angled rectangular area.
		/**
		 * @param [in]	lMapIndex Map index to apply to
		 * @param [in]	lCX coordinate of the leftmost Cell of the rectangle
		 * @param [in]	lCY coordinate of the topmost Cell of the rectangle
		 * @param [in]	lCW width of the rectangle in Cells
		 * @param [in]	lCH height of the rectangle in Cells
		 * @param [in]	lRotate rotation angle (non-right angle)
		 * @param [in]	dwAttr Attribute to apply
		 * @param [in]	mode type of operation on the Attribute
		 */
		bool		for_attr_region_free_angle( int32_t lMapIndex, int32_t lCX, int32_t lCY, int32_t lCW, int32_t lCH, int32_t lRotate, uint32_t dwAttr, attr_region_mode mode );

		/// Performs a specific operation on the Attribute of a single Cell.
		/**
		 * @param [in]	lMapIndex Map index to apply to
		 * @param [in]	lCX X coordinate of the Cell
		 * @param [in]	lCY Y coordinate of the Cell
		 * @param [in]	dwAttr Attribute to apply
		 * @param [in]	mode type of operation on the Attribute
		 */
		bool		for_attr_region_cell( int32_t lMapIndex, int32_t lCX, int32_t lCY, uint32_t dwAttr, attr_region_mode mode );

		static uint16_t			current_sectree_version_;
		std::map<uint32_t, sectree_map *>	map_pk_sectree_;
		std::map<int, area_map>	map_pk_area_;
		std::vector<map_region>		vec_map_region_;
		std::map<uint32_t, std::vector<npc_info> > npc_position_;

		// <Factor> Circular private map indexing
		typedef std::unordered_map<int32_t, int> private_index_map_type;
		private_index_map_type next_private_index_map_;
};
}
