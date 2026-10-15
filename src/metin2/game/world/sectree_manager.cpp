#include <metin2/game/world/sectree_manager.h>

#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/constants.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/desc_manager.h>
#include <metin2/game/world/entity.h>
#include <metin2/game/data/image_file.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/protocol/lzo_manager.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/world/regen.h>
#include <metin2/game/world/sectree.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/world/vid.h>

#include <metin2/common/attribute.h>
#include <metin2/log/log.h>
#include <metin2/core/utils.h>

#include <lzo/lzo1x.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ios>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <strings.h>

namespace metin2::game::inline world {

uint16_t sectree_manager::current_sectree_version_ = 0x0300;

sectree_map::sectree_map()
{
	memset( &setting, 0, sizeof(setting) );
}

sectree_map::~sectree_map()
{
	map_type::iterator it = map_.begin();

	while (it != map_.end()) {
		sectree * sectree = (it++)->second;
		delete sectree;
	}

	map_.clear();
}

sectree_map::sectree_map(sectree_map & r)
{
	setting = r.setting;

	map_type::iterator it = r.map_.begin();

	while (it != r.map_.end())
	{
		sectree * tree = new sectree;

		tree->id_.coord = it->second->id_.coord;
		tree->clone_attribute(it->second);

		map_.insert(map_type::value_type(it->first, tree));
		++it;
	}

	build();
}

sectree * sectree_map::find(uint32_t package)
{
	map_type::iterator it = map_.find(package);

	if (it == map_.end())
		return nullptr;

	return it->second;
}

sectree * sectree_map::find(uint32_t x, uint32_t y)
{
	sectreeid id;
	id.coord.x = x / SECTREE_SIZE;
	id.coord.y = y / SECTREE_SIZE;
	return find(id.package);
}

void sectree_map::build()
{
    struct neighbor_coord_s
	{
		int x;
		int y;
	} neighbor_coord[8] = {
		{ -SECTREE_SIZE,	0		},
		{  SECTREE_SIZE,	0		},
		{ 0	       ,	-SECTREE_SIZE	},
		{ 0	       ,	 SECTREE_SIZE	},
		{ -SECTREE_SIZE,	 SECTREE_SIZE	},
		{  SECTREE_SIZE,	-SECTREE_SIZE	},
		{ -SECTREE_SIZE,	-SECTREE_SIZE	},
		{  SECTREE_SIZE,	 SECTREE_SIZE	},
	};

	map_type::iterator it = map_.begin();

	while (it != map_.end())
	{
		sectree * tree = it->second;

		tree->neighbor_list_.push_back(tree);

		metin2::log::debug( "{:d}x{:d}", tree->id_.coord.x, tree->id_.coord.y);

		int x = tree->id_.coord.x * SECTREE_SIZE;
		int y = tree->id_.coord.y * SECTREE_SIZE;

		for (uint32_t i = 0; i < 8; ++i)
		{
			sectree * tree2 = find(x + neighbor_coord[i].x, y + neighbor_coord[i].y);

			if (tree2)
			{
				metin2::log::debug( "   {:d} {:d}x{:d}", i, tree2->id_.coord.x, tree2->id_.coord.y);
				tree->neighbor_list_.push_back(tree2);
			}
		}

		++it;
	}
}

sectree_manager::sectree_manager()
{
}

sectree_manager::~sectree_manager()
{
}

sectree_map * sectree_manager::map(int32_t map_index)
{
	std::map<uint32_t, sectree_map *>::iterator it = map_pk_sectree_.find(map_index);

	if (it == map_pk_sectree_.end())
		return nullptr;

	return it->second;
}

sectree * sectree_manager::get(uint32_t index, uint32_t package)
{
	sectree_map * pkSectreeMap = map(index);

	if (!pkSectreeMap)
		return nullptr;

	return pkSectreeMap->find(package);
}

sectree * sectree_manager::get(uint32_t index, uint32_t x, uint32_t y)
{
	sectreeid id;
	id.coord.x = x / SECTREE_SIZE;
	id.coord.y = y / SECTREE_SIZE;
	return get(index, id.package);
}

int sectree_manager::load_setting_file(int32_t map_index, const char * setting_file_name, map_setting & r_setting)
{
	memset(&r_setting, 0, sizeof(map_setting));

	FILE * fp = fopen(setting_file_name, "r");

	if (!fp)
	{
		metin2::log::error("cannot open file: {}", setting_file_name);
		return 0;
	}

	char buf[256], cmd[256];
	int iWidth = 0, iHeight = 0;

	while (fgets(buf, 256, fp))
	{
		sscanf(buf, " %s ", cmd);

		if (!strcasecmp(cmd, "MapSize"))
		{
			sscanf(buf, " %s %d %d ", cmd, &iWidth, &iHeight);
		}
		else if (!strcasecmp(cmd, "BasePosition"))
		{
			sscanf(buf, " %s %d %d", cmd, &r_setting.base_x, &r_setting.base_y);
		}
		else if (!strcasecmp(cmd, "CellScale"))
		{
			sscanf(buf, " %s %d ", cmd, &r_setting.cell_scale);
		}
	}

	fclose(fp);

	if ((iWidth == 0 && iHeight == 0) || r_setting.cell_scale == 0)
	{
		metin2::log::error("Invalid Settings file: {}", setting_file_name);
		return 0;
	}

	r_setting.index = map_index;
	r_setting.width = (r_setting.cell_scale * 128 * iWidth);
	r_setting.height = (r_setting.cell_scale * 128 * iHeight);
	return 1;
}

sectree_map * sectree_manager::build_sectree_from_setting(map_setting & r_setting)
{
	sectree_map * pkMapSectree = new sectree_map;

	pkMapSectree->setting = r_setting;

	int x, y;
	sectree * tree;

	for (x = r_setting.base_x; x < r_setting.base_x + r_setting.width; x += SECTREE_SIZE)
	{
		for (y = r_setting.base_y; y < r_setting.base_y + r_setting.height; y += SECTREE_SIZE)
		{
			tree = new sectree;
			tree->id_.coord.x = x / SECTREE_SIZE;
			tree->id_.coord.y = y / SECTREE_SIZE;
			pkMapSectree->add(tree->id_.package, tree);
			metin2::log::debug( "new sectree {:d} x {:d}", tree->id_.coord.x, tree->id_.coord.y);
		}
	}

	if ((r_setting.base_x + r_setting.width) % SECTREE_SIZE)
	{
		tree = new sectree;
		tree->id_.coord.x = ((r_setting.base_x + r_setting.width) / SECTREE_SIZE) + 1;
		tree->id_.coord.y = ((r_setting.base_y + r_setting.height) / SECTREE_SIZE);
		pkMapSectree->add(tree->id_.package, tree);
	}

	if ((r_setting.base_y + r_setting.height) % SECTREE_SIZE)
	{
		tree = new sectree;
		tree->id_.coord.x = ((r_setting.base_x + r_setting.width) / SECTREE_SIZE);
		tree->id_.coord.y = ((r_setting.base_y + r_setting.height) / SECTREE_SIZE) + 1;
		pkMapSectree->add(tree->id_.package, tree);
	}

	return pkMapSectree;
}

void sectree_manager::load_dungeon(int index, const char * file_name)
{
	FILE* fp = fopen(file_name, "r");

	if (!fp)
		return;

	int count = 0;

	while (!feof(fp))
	{
		char buf[1024];

		if (nullptr == fgets(buf, 1024, fp))
			break;

		if (buf[0] == '#' || (buf[0] == '/' && buf[1] == '/'))
			continue;

		std::istringstream ins(buf, std::ios_base::in);
		std::string position_name;
		int x, y, sx, sy, dir;

		ins >> position_name >> x >> y >> sx >> sy >> dir;

		if (ins.fail())
			continue;

		x -= sx;
		y -= sy;
		sx *= 2;
		sy *= 2;
		sx += x;
		sy += y;

		map_pk_area_[index].insert(std::make_pair(position_name, area_info(x, y, sx, sy, dir)));

		count++;
	}

	fclose(fp);

	metin2::log::info( "Dungeon Position Load [{:3d}]{} count {:d}", index, file_name, count);
}

bool sectree_manager::load_map_region(const char * file_name, map_setting & r_setting, const char * map_name_value)
{
	FILE * fp = fopen(file_name, "r");

	if ( metin2::config::test_server() )
		metin2::log::info( "[LoadMapRegion] file({})", file_name );

	if (!fp)
		return false;

	int iX=0, iY=0;
	pixel_position pos[3] = { {0,0,0}, {0,0,0}, {0,0,0} };

	fscanf(fp, " %d %d ", &iX, &iY);

	int iEmpirePositionCount = fscanf(fp, " %d %d %d %d %d %d ", 
			&pos[0].x, &pos[0].y,
			&pos[1].x, &pos[1].y,
			&pos[2].x, &pos[2].y);

	fclose(fp);

	if( iEmpirePositionCount == 6 )
	{
		for ( int n = 0; n < 3; ++n )
			metin2::log::info("LoadMapRegion {:d} {:d} ", pos[n].x, pos[n].y );
	}
	else
	{
		metin2::log::info( "LoadMapRegion no empire specific start point" );
	}

	map_region region;

	region.index = r_setting.index;
	region.sx = r_setting.base_x;
	region.sy = r_setting.base_y;
	region.ex = r_setting.base_x + r_setting.width;
	region.ey = r_setting.base_y + r_setting.height;

	region.map_name = map_name_value;

	region.pos_spawn.x = r_setting.base_x + (iX * 100);
	region.pos_spawn.y = r_setting.base_y + (iY * 100); 

	r_setting.pos_spawn = region.pos_spawn;

	metin2::log::info( "LoadMapRegion {:d} x {:d} ~ {:d} y {:d} ~ {:d}, town {:d} {:d}", 
			region.index,
			region.sx,
			region.ex,
			region.sy,
			region.ey,
			region.pos_spawn.x,
			region.pos_spawn.y);

	if (iEmpirePositionCount == 6)
	{
		region.empire_spawn_different = true;

		for (int i = 0; i < 3; i++)
		{
			region.pos_empire[i].x = r_setting.base_x + (pos[i].x * 100);
			region.pos_empire[i].y = r_setting.base_y + (pos[i].y * 100);
		}
	}
	else
	{
		region.empire_spawn_different = false;
	}

	vec_map_region_.push_back(region);

	metin2::log::info("LoadMapRegion {:d} End", region.index);
	return true;
}

bool sectree_manager::load_attribute(sectree_map * map_sectree, const char * file_name, map_setting & r_setting)
{
	FILE * fp = fopen(file_name, "rb");

	if (!fp)
	{
		metin2::log::error("SECTREE_MANAGER::LoadAttribute : cannot open {}", file_name);
		return false;
	}

	int iWidth, iHeight;

	fread(&iWidth, sizeof(int), 1, fp);
	fread(&iHeight, sizeof(int), 1, fp);

	int maxMemSize = lzo_manager::instance().max_compressed_size(sizeof(uint32_t) * (SECTREE_SIZE / CELL_SIZE) * (SECTREE_SIZE / CELL_SIZE));

	unsigned int uiSize;
	lzo_uint uiDestSize;

	std::vector<uint8_t> abComp(maxMemSize);
	auto attr = std::make_unique_for_overwrite<uint32_t[]>(maxMemSize);

	for (int y = 0; y < iHeight; ++y)
		for (int x = 0; x < iWidth; ++x)
		{
			sectreeid id;
			id.coord.x = (r_setting.base_x / SECTREE_SIZE) + x;
			id.coord.y = (r_setting.base_y / SECTREE_SIZE) + y;

			sectree * tree = map_sectree->find(id.package);

			if (tree == nullptr)
			{
				metin2::log::error("FATAL ERROR! LoadAttribute({}) - cannot find sectree(package={:x}, coord=({:d}, {:d}), map_index={:d}, map_base=({:d}, {:d}))", 
						file_name, id.package, id.coord.x, id.coord.y, r_setting.index, r_setting.base_x, r_setting.base_y);
				metin2::log::error("ERROR_ATTR_POS({:d}, {:d}) attr_size({:d}, {:d})", x, y, iWidth, iHeight);
				metin2::log::error("CHECK! 'Setting.txt' and 'server_attr' MAP_SIZE!!");

				map_sectree->dump_all_to_sys_err();
				abort();

				return false;
			}

			if (tree->id_.package != id.package)
			{
				metin2::log::error("returned tree id mismatch! return {:d}, request {:d}", 
						tree->id_.package, id.package);
				fclose(fp);

				return false;
			}

			fread(&uiSize, sizeof(int), 1, fp);
			if (uiSize > abComp.size())
			{
				metin2::log::error("attribute block of {:d} bytes is larger than {:d}", uiSize, abComp.size());
				fclose(fp);
				return false;
			}
			fread(abComp.data(), sizeof(char), uiSize, fp);

			uiDestSize = sizeof(uint32_t) * maxMemSize;
			lzo_manager::instance().decompress(abComp.data(), uiSize, (uint8_t *) attr.get(), &uiDestSize);

			if (uiDestSize != sizeof(uint32_t) * (SECTREE_SIZE / CELL_SIZE) * (SECTREE_SIZE / CELL_SIZE))
			{
				metin2::log::error("SECTREE_MANAGER::LoadAttribte : {} : {:d} {:d} size mismatch! {:d}",
						file_name, tree->id_.coord.x, tree->id_.coord.y, static_cast<int>(uiDestSize));
				fclose(fp);

				return false;
			}

			tree->bind_attribute(new attribute(attr.get(), SECTREE_SIZE / CELL_SIZE, SECTREE_SIZE / CELL_SIZE));
		}

	fclose(fp);

	return true;
}

bool sectree_manager::recall_position_by_empire(int map_index, uint8_t empire, pixel_position & r_pos)
{
	std::vector<map_region>::iterator it = vec_map_region_.begin();

	if (map_index >= 10000)
	{
		map_index /= 10000;
	}

	while (it != vec_map_region_.end())
	{
		map_region & rRegion = *(it++);

		if (rRegion.index == map_index)
		{
			if (rRegion.empire_spawn_different && empire >= 1 && empire <= 3)
				r_pos = rRegion.pos_empire[empire - 1];
			else
				r_pos = rRegion.pos_spawn;

			return true;
		}
	}

	return false;
}

bool sectree_manager::center_position_of_map(int32_t map_index, pixel_position & r_pos)
{
	std::vector<map_region>::iterator it = vec_map_region_.begin();

	while (it != vec_map_region_.end())
	{
		map_region & rRegion = *(it++);

		if (rRegion.index == map_index)
		{
			r_pos.x = rRegion.sx + (rRegion.ex - rRegion.sx) / 2;
			r_pos.y = rRegion.sy + (rRegion.ey - rRegion.sy) / 2;
			r_pos.z = 0;
			return true;
		}
	}

	return false;
}

bool sectree_manager::map_base_position_by_map_index(int32_t map_index, pixel_position & r_pos)
{
	if (map_index> 10000) map_index /= 10000;
	std::vector<map_region>::iterator it = vec_map_region_.begin();

	while (it != vec_map_region_.end())
	{
		map_region & rRegion = *(it++);

		if (map_index == rRegion.index)
		{
			r_pos.x = rRegion.sx;
			r_pos.y = rRegion.sy;
			r_pos.z = 0;
			return true;
		}
	}

	return false;
}

bool sectree_manager::map_base_position(int32_t x, int32_t y, pixel_position & r_pos)
{
	std::vector<map_region>::iterator it = vec_map_region_.begin();

	while (it != vec_map_region_.end())
	{
		map_region & rRegion = *(it++);

		if (x >= rRegion.sx && y >= rRegion.sy && x < rRegion.ex && y < rRegion.ey)
		{
			r_pos.x = rRegion.sx;
			r_pos.y = rRegion.sy;
			r_pos.z = 0;
			return true;
		}
	}

	return false;
}

const map_region * sectree_manager::find_region_by_partial_name(const char* map_name_value)
{
	std::vector<map_region>::iterator it = vec_map_region_.begin();

	while (it != vec_map_region_.end())
	{
		map_region & rRegion = *(it++);

		if (rRegion.map_name.find(map_name_value))
			return &rRegion;
	}

	return nullptr;
}

const map_region * sectree_manager::get_map_region(int32_t map_index)
{
	std::vector<map_region>::iterator it = vec_map_region_.begin();

	while (it != vec_map_region_.end())
	{
		map_region & rRegion = *(it++);

		if (rRegion.index == map_index)
			return &rRegion;
	}

	return nullptr;
}

int sectree_manager::map_index(int32_t x, int32_t y)
{
	std::vector<map_region>::iterator it = vec_map_region_.begin();

	while (it != vec_map_region_.end())
	{
		map_region & rRegion = *(it++);

		if (x >= rRegion.sx && y >= rRegion.sy && x < rRegion.ex && y < rRegion.ey)
			return rRegion.index;
	}

	metin2::log::info( "SECTREE_MANAGER::GetMapIndex({:d}, {:d})", x, y);

	std::vector<map_region>::iterator i;
	for (i = vec_map_region_.begin(); i !=vec_map_region_.end(); ++i)
	{
		map_region & rRegion = *i;
		metin2::log::info( "{:d}: ({:d}, {:d}) ~ ({:d}, {:d})", rRegion.index, rRegion.sx, rRegion.sy, rRegion.ex, rRegion.ey);
	}

	return 0;
}

int sectree_manager::build(const char * list_file_name, const char* map_base_path)
{
	if (true == metin2::config::test_server())
	{
		metin2::log::info( "[BUILD] Build {} {} ", list_file_name, map_base_path );
	}

	FILE* fp = fopen(list_file_name, "r");

	if (nullptr == fp)
		return 0;

	char buf[256 + 1];
	char szFilename[256];
	char szMapName[256];
	int iIndex;

	while (fgets(buf, 256, fp))
	{
		*strrchr(buf, '\n') = '\0';

		if (!strncmp(buf, "//", 2) || *buf == '#')
			continue;

		sscanf(buf, " %d %s ", &iIndex, szMapName);

		snprintf(szFilename, sizeof(szFilename), "%s/%s/Setting.txt", map_base_path, szMapName);

		map_setting setting;
		setting.index = iIndex;

		if (!load_setting_file(iIndex, szFilename, setting))
		{
			metin2::log::error("can't load file {} in LoadSettingFile", szFilename);
			fclose(fp);
			return 0;
		}

		snprintf(szFilename, sizeof(szFilename), "%s/%s/Town.txt", map_base_path, szMapName);

		if (!load_map_region(szFilename, setting, szMapName))
		{
			metin2::log::error("can't load file {} in LoadMapRegion", szFilename);
			fclose(fp);
			return 0;
		}

		if (true == metin2::config::test_server())
			metin2::log::info("[BUILD] Build {} {} {:d} ",map_base_path, szMapName, iIndex );

		if (map_allow_find(iIndex))
		{
			sectree_map * pkMapSectree = build_sectree_from_setting(setting);
			map_pk_sectree_.insert(std::map<uint32_t, sectree_map *>::value_type(iIndex, pkMapSectree));

			snprintf(szFilename, sizeof(szFilename), "%s/%s/server_attr", map_base_path, szMapName);
			load_attribute(pkMapSectree, szFilename, setting);

			snprintf(szFilename, sizeof(szFilename), "%s/%s/regen.txt", map_base_path, szMapName);
			regen_load(szFilename, setting.index, setting.base_x, setting.base_y);

			snprintf(szFilename, sizeof(szFilename), "%s/%s/npc.txt", map_base_path, szMapName);
			regen_load(szFilename, setting.index, setting.base_x, setting.base_y);

			snprintf(szFilename, sizeof(szFilename), "%s/%s/boss.txt", map_base_path, szMapName);
			regen_load(szFilename, setting.index, setting.base_x, setting.base_y);

			snprintf(szFilename, sizeof(szFilename), "%s/%s/stone.txt", map_base_path, szMapName);
			regen_load(szFilename, setting.index, setting.base_x, setting.base_y);

			snprintf(szFilename, sizeof(szFilename), "%s/%s/dungeon.txt", map_base_path, szMapName);
			load_dungeon(iIndex, szFilename);

			pkMapSectree->build();
		}
	}

	fclose(fp);

	return 1;
}

bool sectree_manager::is_movable_position(int32_t map_index, int32_t x, int32_t y)
{
	sectree * tree = sectree_manager::instance().get(map_index, x, y);

	if (!tree)
		return false;

	return (!tree->is_attr(x, y, ATTR_BLOCK | ATTR_OBJECT));
}

bool sectree_manager::movable_position(int32_t map_index, int32_t x, int32_t y, pixel_position & pos)
{
	int i = 0;

	do
	{
		int32_t dx = x + arround_coords[i].x;
		int32_t dy = y + arround_coords[i].y;

		sectree * tree = sectree_manager::instance().get(map_index, dx, dy);

		if (!tree)
			continue;

		if (!tree->is_attr(dx, dy, ATTR_BLOCK | ATTR_OBJECT))
		{
			pos.x = dx;
			pos.y = dy;
			return true;
		}
	} while (++i < ARROUND_COORD_MAX_NUM);

	pos.x = x;
	pos.y = y;
	return false;
}

bool sectree_manager::valid_location(int32_t map_index, int32_t x, int32_t y, int32_t & r_l_valid_map_index, pixel_position & r_pos, uint8_t empire)
{
	sectree_map * pkSectreeMap = map(map_index);

	if (!pkSectreeMap)
	{
		if (map_index >= 10000)
		{
			return valid_location(map_index / 10000, x, y, r_l_valid_map_index, r_pos);
		}
		else
		{
			metin2::log::error("cannot find sectree_map by map index {:d}", map_index);
			return false;
		}
	}

	int32_t lRealMapIndex = map_index;

	if (lRealMapIndex >= 10000)
		lRealMapIndex = lRealMapIndex / 10000;

	std::vector<map_region>::iterator it = vec_map_region_.begin();

	while (it != vec_map_region_.end())
	{
		map_region & rRegion = *(it++);

		if (rRegion.index == lRealMapIndex)
		{
			sectree * tree = pkSectreeMap->find(x, y);

			if (!tree)
			{
				metin2::log::error("cannot find tree by {:d} {:d} (map index {:d})", x, y, map_index);
				return false;
			}

			r_l_valid_map_index = map_index;
			r_pos.x = x;
			r_pos.y = y;
			return true;
		}
	}

	metin2::log::error("invalid location (map index {:d} {:d} x {:d})", lRealMapIndex, x, y);
	return false;
}

bool sectree_manager::random_location(int32_t map_index, pixel_position & r_pos, uint32_t current_x, uint32_t current_y, int max_distance)
{
	sectree_map * pkSectreeMap = map(map_index);

	if (!pkSectreeMap)
		return false;

	uint32_t x, y;

	std::vector<map_region>::iterator it = vec_map_region_.begin();

	while (it != vec_map_region_.end())
	{
		map_region & rRegion = *(it++);

		if (rRegion.index != map_index)
			continue;

		int i = 0;

		while (i++ < 100)
		{
			x = number(rRegion.sx + 50, rRegion.ex - 50);
			y = number(rRegion.sy + 50, rRegion.ey - 50);

			if (max_distance != 0)
			{
				int d;

				d = std::abs((float)current_x - x);

				if (d > max_distance)
				{
					if (x < current_x)
						x = current_x - max_distance;
					else
						x = current_x + max_distance;
				}

				d = std::abs((float)current_y - y);

				if (d > max_distance)
				{
					if (y < current_y)
						y = current_y - max_distance;
					else
						y = current_y + max_distance;
				}
			}

			sectree * tree = pkSectreeMap->find(x, y);

			if (!tree)
				continue;

			if (tree->is_attr(x, y, ATTR_BLOCK | ATTR_OBJECT))
				continue;

			r_pos.x = x;
			r_pos.y = y;
			return true;
		}
	}

	return false;
}

int32_t sectree_manager::create_private_map(int32_t map_index)
{
	if (map_index >= 10000)
		return 0;

	sectree_map * pkMapSectree = map(map_index);

	if (!pkMapSectree)
	{
		metin2::log::error("Cannot find map index {:d}", map_index);
		return 0;
	}

	int32_t base = map_index * 10000;
	int index_cap = 10000;
	if ( map_index == 107 || map_index == 108 || map_index == 109 ) {
		index_cap = (metin2::config::test_server() ? 1 : 51);
	}
	private_index_map_type::iterator it = next_private_index_map_.find(map_index);
	if (it == next_private_index_map_.end()) {
		it = next_private_index_map_.insert(private_index_map_type::value_type(map_index, 0)).first;
	}
	int i, next_index = it->second;
	for (i = 0; i < index_cap; ++i) {
		if (map(base + next_index) == nullptr) {
			break;
		}
		if (++next_index >= index_cap) {
			next_index = 0;
		}
	}
	if (i == index_cap) {
		return 0;
	}
	int32_t lNewMapIndex = base + next_index;
	if (++next_index >= index_cap) {
		next_index = 0;
	}
	it->second = next_index;

	pkMapSectree = new sectree_map(*pkMapSectree);
	map_pk_sectree_.insert(std::map<uint32_t, sectree_map *>::value_type(lNewMapIndex, pkMapSectree));

	metin2::log::info( "PRIVATE_MAP: {:d} created (original {:d})", lNewMapIndex, map_index);
	return lNewMapIndex;
}

struct f_destroy_private_map_entity
{
	void operator() (entity * ent)
	{
		if (ent->is_type(ENTITY_CHARACTER))
		{
			character * ch = (character *) ent;
			metin2::log::info( "PRIVAE_MAP: removing character {}", ch->name());

			if (ch->get_desc())
				desc_manager::instance().destroy_desc(ch->get_desc());
			else
				character_manager::instance().destroy_character(ch);
		}
		else if (ent->is_type(ENTITY_ITEM))
		{
			item * item = (game::items::item *) ent;
			metin2::log::info( "PRIVATE_MAP: removing item {}", item->name());

			item_manager::instance().destroy_item(item);
		}
		else
			metin2::log::error("PRIVAE_MAP: trying to remove unknown entity {:d}", ent->type());
	}
};

void sectree_manager::destroy_private_map(int32_t map_index)
{
	if (map_index < 10000)
		return;

	sectree_map * pkMapSectree = map(map_index);

	if (!pkMapSectree)
		return;

	// Entities on this map but outside every sectree cannot be deleted here.
	f_destroy_private_map_entity f;
	pkMapSectree->for_each(f);

	map_pk_sectree_.erase(map_index);
	delete pkMapSectree;

	metin2::log::info( "PRIVATE_MAP: {:d} destroyed", map_index);
}

area_map& sectree_manager::dungeon_area(int32_t map_index)
{
	auto it = map_pk_area_.find(map_index);

	if (it == map_pk_area_.end())
	{
		return map_pk_area_[-1];
	}
	return it->second;
}

void sectree_manager::send_npc_position(character * ch)
{
	desc * d = ch->get_desc();
	if (!d)
		return;

	int32_t lMapIndex = ch->map_index();

	if (npc_position_[lMapIndex].empty())
		return;

	temp_buffer buf;
	packet_gcnpc_position p;
	p.header = HEADER_GC_NPC_POSITION;
	p.count = npc_position_[lMapIndex].size();

	npc_position np;

	decltype((npc_position_[lMapIndex]).begin()) it;

	for (it = npc_position_[lMapIndex].begin(); it != npc_position_[lMapIndex].end(); ++it)
	{
		np.type = it->type;
		strlcpy(np.name, it->name, sizeof(np.name));
		np.x = it->x;
		np.y = it->y;
		buf.write(&np, sizeof(np));
	}

	p.size = sizeof(p) + buf.size();

	if (buf.size())
	{
		d->buffered_packet(&p, sizeof(packet_gcnpc_position));
		d->packet(buf.read_peek(), buf.size());
	}
	else
		d->packet(&p, sizeof(packet_gcnpc_position));
}

void sectree_manager::insert_npc_position(int32_t map_index, uint8_t type, const char* name, int32_t x, int32_t y)
{
	npc_position_[map_index].push_back(npc_info(type, name, x, y));
}

uint8_t sectree_manager::empire_from_map_index(int32_t map_index)
{
	if (map_index >= 1 && map_index <= 20)
		return 1;

	if (map_index >= 21 && map_index <= 40)
		return 2;

	if (map_index >= 41 && map_index <= 60)
		return 3;

	if ( map_index == 184 || map_index == 185 )
		return 1;
	
	if ( map_index == 186 || map_index == 187 )
		return 2;
	
	if ( map_index == 188 || map_index == 189 )
		return 3;

	switch ( map_index )
	{
		case 190 :
			return 1;
		case 191 :
			return 2;
		case 192 :
			return 3;
	}
	
	return 0;
}


bool sectree_manager::for_attr_region_cell( int32_t map_index, int32_t cx, int32_t cy, uint32_t attr, attr_region_mode mode )
{
	sectreeid id;

	id.coord.x = cx / (SECTREE_SIZE / CELL_SIZE);
	id.coord.y = cy / (SECTREE_SIZE / CELL_SIZE);

	int32_t lTreeCX = id.coord.x * (SECTREE_SIZE / CELL_SIZE);
	int32_t lTreeCY = id.coord.y * (SECTREE_SIZE / CELL_SIZE);

	sectree * pSec = get( map_index, id.package );
	if ( !pSec )
		return false;

	switch (mode)
	{
		case ATTR_REGION_MODE_SET:
			pSec->set_attribute( cx - lTreeCX, cy - lTreeCY, attr );
			break;

		case ATTR_REGION_MODE_REMOVE:
			pSec->remove_attribute( cx - lTreeCX, cy - lTreeCY, attr );
			break;

		case ATTR_REGION_MODE_CHECK:
			if ( pSec->is_attr( cx * CELL_SIZE, cy * CELL_SIZE, ATTR_OBJECT ) )
				return true;
			break;

		default:
			metin2::log::error("Unknown region mode {:d}", mode);
			break;
	}

	return false;
}

bool sectree_manager::for_attr_region_right_angle( int32_t map_index, int32_t cx, int32_t cy, int32_t cw, int32_t ch, int32_t rotate, uint32_t attr, attr_region_mode mode )
{
	if (1 == rotate/90 || 3 == rotate/90)
	{
		for (int x = 0; x < ch; ++x)
			for (int y = 0; y < cw; ++y)
			{
				if ( for_attr_region_cell( map_index, cx + x, cy + y, attr, mode ) )
					return true;
			}
	}
	if (0 == rotate/90 || 2 == rotate/90)
	{
		for (int x = 0; x < cw; ++x)
			for (int y = 0; y < ch; ++y)
			{
				if ( for_attr_region_cell( map_index, cx + x, cy + y, attr, mode) )
					return true;
			}
	}

	return mode == ATTR_REGION_MODE_CHECK ? false : true;
}


bool sectree_manager::for_attr_region_free_angle( int32_t map_index, int32_t cx, int32_t cy, int32_t cw, int32_t ch, int32_t rotate, uint32_t attr, attr_region_mode mode )
{
	float fx1 = (-cw/2) * sinf(float(rotate)/180.0f*3.14f) + (-ch/2) * cosf(float(rotate)/180.0f*3.14f);
	float fy1 = (-cw/2) * cosf(float(rotate)/180.0f*3.14f) - (-ch/2) * sinf(float(rotate)/180.0f*3.14f);

	float fx2 = (+cw/2) * sinf(float(rotate)/180.0f*3.14f) + (-ch/2) * cosf(float(rotate)/180.0f*3.14f);
	float fy2 = (+cw/2) * cosf(float(rotate)/180.0f*3.14f) - (-ch/2) * sinf(float(rotate)/180.0f*3.14f);

	float fx3 = (-cw/2) * sinf(float(rotate)/180.0f*3.14f) + (+ch/2) * cosf(float(rotate)/180.0f*3.14f);
	float fy3 = (-cw/2) * cosf(float(rotate)/180.0f*3.14f) - (+ch/2) * sinf(float(rotate)/180.0f*3.14f);

	float fx4 = (+cw/2) * sinf(float(rotate)/180.0f*3.14f) + (+ch/2) * cosf(float(rotate)/180.0f*3.14f);
	float fy4 = (+cw/2) * cosf(float(rotate)/180.0f*3.14f) - (+ch/2) * sinf(float(rotate)/180.0f*3.14f);

	float fdx1 = fx2 - fx1;
	float fdy1 = fy2 - fy1;
	float fdx2 = fx1 - fx3;
	float fdy2 = fy1 - fy3;

	if (0 == fdx1 || 0 == fdx2)
	{
		metin2::log::error( "SECTREE_MANAGER::ForAttrRegion - Unhandled exception. MapIndex: {:d}", map_index );
		return false;
	}

	float fTilt1 = float(fdy1) / float(fdx1);
	float fTilt2 = float(fdy2) / float(fdx2);
	float fb1 = fy1 - fTilt1*fx1;
	float fb2 = fy1 - fTilt2*fx1;
	float fb3 = fy4 - fTilt1*fx4;
	float fb4 = fy4 - fTilt2*fx4;

	float fxMin = std::min({fx1, fx2, fx3, fx4});
	float fxMax = std::max({fx1, fx2, fx3, fx4});
	for (int i = int(fxMin); i < int(fxMax); ++i)
	{
		float fyValue1 = fTilt1*i + std::min(fb1, fb3);
		float fyValue2 = fTilt2*i + std::min(fb2, fb4);

		float fyValue3 = fTilt1*i + std::max(fb1, fb3);
		float fyValue4 = fTilt2*i + std::max(fb2, fb4);

		float fMinValue;
		float fMaxValue;
		if (abs(int(fyValue1)) < abs(int(fyValue2)))
			fMaxValue = fyValue1;
		else
			fMaxValue = fyValue2;
		if (abs(int(fyValue3)) < abs(int(fyValue4)))
			fMinValue = fyValue3;
		else
			fMinValue = fyValue4;

		for (int j = int(std::min(fMinValue, fMaxValue)); j < int(std::max(fMinValue, fMaxValue)); ++j) {
			if ( for_attr_region_cell( map_index, cx + (cw / 2) + i, cy + (ch / 2) + j, attr, mode ) )
				return true;
		}
	}

	return mode == ATTR_REGION_MODE_CHECK ? false : true;
}

bool sectree_manager::for_attr_region(int32_t map_index, int32_t start_x, int32_t start_y, int32_t end_x, int32_t end_y, int32_t rotate, uint32_t attr, attr_region_mode mode)
{
	sectree_map * pkMapSectree = map(map_index);

	if (!pkMapSectree)
	{
		metin2::log::error("Cannot find SECTREE_MAP by map index {:d}", map_index);
		return mode == ATTR_REGION_MODE_CHECK ? true : false;
	}


	start_x	-= start_x % CELL_SIZE;
	start_y	-= start_y % CELL_SIZE;
	end_x	+= end_x % CELL_SIZE;
	end_y	+= end_y % CELL_SIZE;


	int32_t lCX = start_x / CELL_SIZE;
	int32_t lCY = start_y / CELL_SIZE;
	int32_t lCW = (end_x - start_x) / CELL_SIZE;
	int32_t lCH = (end_y - start_y) / CELL_SIZE;

	metin2::log::debug( "ForAttrRegion {:d} {:d} ~ {:d} {:d}", start_x, start_y, end_x, end_y);

	rotate = rotate % 360;

	if (0 == rotate % 90)
		return for_attr_region_right_angle( map_index, lCX, lCY, lCW, lCH, rotate, attr, mode );

	return for_attr_region_free_angle( map_index, lCX, lCY, lCW, lCH, rotate, attr, mode );
}

bool sectree_manager::save_attribute_to_image(int map_index, const char * file_name, sectree_map * map_src)
{
	sectree_map * pMap = sectree_manager::map(map_index);

	if (!pMap)
	{
		if (map_src)
			pMap = map_src;
		else
		{
			metin2::log::error("cannot find sectree_map {:d}", map_index);
			return false;
		}
	}

	int iMapHeight = pMap->setting.height / 128 / 200;
	int iMapWidth = pMap->setting.width / 128 / 200;

	if (iMapHeight < 0 || iMapWidth < 0)
	{
		metin2::log::error("map size error w {:d} h {:d}", iMapWidth, iMapHeight);
		return false;
	}

	metin2::log::info( "SaveAttributeToImage w {:d} h {:d} file {}", iMapWidth, iMapHeight, file_name);

	const int iImageWidth = 512 * iMapWidth;
	const int iImageHeight = 512 * iMapHeight;
	std::vector<uint32_t> image(static_cast<size_t>(iImageWidth) * iImageHeight);
	uint32_t * pdwDest = image.data();

	int pixels = 0;
	int x, x2;
	int y, y2;

	auto pdwLine = std::make_unique_for_overwrite<uint32_t[]>(SECTREE_SIZE / CELL_SIZE);

	for (y = 0; y < 4 * iMapHeight; ++y)
	{
		for (y2 = 0; y2 < SECTREE_SIZE / CELL_SIZE; ++y2)
		{
			for (x = 0; x < 4 * iMapWidth; ++x)
			{
				sectreeid id;

				id.coord.x = x + pMap->setting.base_x / SECTREE_SIZE;
				id.coord.y = y + pMap->setting.base_y / SECTREE_SIZE;

				sectree * pSec = pMap->find(id.package);

				if (!pSec)
				{
					metin2::log::error("cannot get sectree for {:d} {:d} {:d} {:d}", id.coord.x, id.coord.y, pMap->setting.base_x, pMap->setting.base_y);
					continue;
				}

				pSec->attribute_->copy_row(y2, pdwLine.get());

				if (!pdwLine)
				{
					metin2::log::error("cannot get attribute line pointer");
					continue;
				}

				for (x2 = 0; x2 < SECTREE_SIZE / CELL_SIZE; ++x2)
				{
					uint32_t dwColor;

					if (is_set(pdwLine[x2], ATTR_WATER))
						dwColor = 0xff0000ff;
					else if (is_set(pdwLine[x2], ATTR_BANPK))
						dwColor = 0xff00ff00;
					else if (is_set(pdwLine[x2], ATTR_BLOCK))
						dwColor = 0xffff0000;
					else
						dwColor = 0xffffffff;

					*(pdwDest++) = dwColor;
					pixels++;
				}
			}
		}
	}

	if (save_tga_bgra(file_name, image.data(), iImageWidth, iImageHeight))
	{
		metin2::log::info( "SECTREE: map {:d} attribute saved to {} ({:d} bytes)", map_index, file_name, pixels);
		return true;
	}
	else
	{
		metin2::log::error("cannot save file, map_index {:d} filename {}", map_index, file_name);
		return false;
	}
}

struct f_purge_monsters
{
	void operator() (entity * ent)
	{
		if ( ent->is_type(ENTITY_CHARACTER) == true )
		{
			character * lpChar = (character *)ent;

			if ( lpChar->is_monster() == true && !lpChar->is_pet())
			{
				character_manager::instance().destroy_character(lpChar);
			}
		}
	}
};

void sectree_manager::purge_monsters_in_map(int32_t map_index)
{
	sectree_map * sectree = sectree_manager::instance().map(map_index);

	if ( sectree != nullptr )
	{
		struct f_purge_monsters f;

		sectree->for_each( f );
	}
}

struct f_purge_stones
{
	void operator() (entity * ent)
	{
		if ( ent->is_type(ENTITY_CHARACTER) == true )
		{
			character * lpChar = (character *)ent;

			if ( lpChar->is_stone() == true )
			{
				character_manager::instance().destroy_character(lpChar);
			}
		}
	}
};

void sectree_manager::purge_stones_in_map(int32_t map_index)
{
	sectree_map * sectree = sectree_manager::instance().map(map_index);

	if ( sectree != nullptr )
	{
		struct f_purge_stones f;

		sectree->for_each( f );
	}
}


struct f_count_monsters
{
	std::map<vid, vid> map_monsters;

	void operator() (entity * ent)
	{
		if ( ent->is_type(ENTITY_CHARACTER) == true )
		{
			character * lpChar = (character *)ent;

			if ( lpChar->is_monster() == true )
			{
				map_monsters[lpChar->get_vid()] = lpChar->get_vid();
			}
		}
	}
};

size_t sectree_manager::monster_count_in_map(int32_t map_index)
{
	sectree_map * sectree = sectree_manager::instance().map(map_index);

	if ( sectree != nullptr )
	{
		struct f_count_monsters f;

		sectree->for_each( f );

		return f.map_monsters.size();
	}

	return 0;
}

struct f_count_specified_monster
{
	uint32_t specified_vnum;
	size_t cnt;

	f_count_specified_monster(uint32_t id)
		: specified_vnum(id), cnt(0)
	{}

	void operator() (entity * ent)
	{
		if (true == ent->is_type(ENTITY_CHARACTER))
		{
			character * pChar = static_cast<character *>(ent);

			if (true == pChar->is_stone())
			{
				if (pChar->get_mob_table().vnum == specified_vnum)
					cnt++;
			}
		}
	}
};

size_t sectree_manager::monster_count_in_map(int32_t map_index, uint32_t vnum)
{
	sectree_map * sectree = sectree_manager::instance().map(map_index);

	if (nullptr != sectree)
	{
		struct f_count_specified_monster f(vnum);

		sectree->for_each( f );

		return f.cnt;
	}

	return 0;
}
}
