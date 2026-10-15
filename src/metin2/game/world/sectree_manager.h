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

struct TMapRegion {
	int			index;
	int			sx, sy, ex, ey;
	PIXEL_POSITION	posSpawn;

	bool		bEmpireSpawnDifferent;
	PIXEL_POSITION	posEmpire[3];

	std::string		strMapName;
};

struct TAreaInfo
{
	int sx, sy, ex, ey, dir;
	TAreaInfo(int sx, int sy, int ex, int ey, int dir)
		: sx(sx), sy(sy), ex(ex), ey(ey), dir(dir)
		{}
};

struct npc_info
{
	uint8_t bType;
	const char* name;
	int32_t x, y;
	npc_info(uint8_t bType, const char* name, int32_t x, int32_t y)
		: bType(bType), name(name), x(x), y(y)
		{}
};

typedef std::map<std::string, TAreaInfo> TAreaMap;

struct TMapSetting {
	int			iIndex;
	int			iCellScale;
	int			iBaseX;
	int			iBaseY;
	int			iWidth;
	int			iHeight;

	PIXEL_POSITION	posSpawn;
};

class SECTREE_MAP
{
	public:
		typedef std::map<uint32_t, SECTREE *> MapType;

		SECTREE_MAP();
		SECTREE_MAP(SECTREE_MAP & r);
		virtual ~SECTREE_MAP();

		bool Add(uint32_t key, SECTREE * sectree) {
			return map_.insert(MapType::value_type(key, sectree)).second;
		}

		SECTREE *	Find(uint32_t dwPackage);
		SECTREE *	Find(uint32_t x, uint32_t y);
		void		Build();

		TMapSetting	m_setting;

		template< typename Func >
		void for_each( Func & rfunc )
		{
			// <Factor> Using snapshot copy to avoid side-effects
			FCollectEntity collector;
			std::map<uint32_t, SECTREE *>::iterator it = map_.begin();
			for ( ; it != map_.end(); ++it)
			{
				SECTREE * sectree = it->second;
				sectree->for_each_entity(collector);
			}
			collector.ForEach(rfunc);
		}

		void DumpAllToSysErr() {
			SECTREE_MAP::MapType::iterator i;
			for (i = map_.begin(); i != map_.end(); ++i)
			{
				metin2::log::error("SECTREE {:x}({:d}, {:d})", i->first, i->first & 0xffff, i->first >> 16);
			}
		}

	private:
		MapType map_;
};

enum EAttrRegionMode
{
	ATTR_REGION_MODE_SET,
	ATTR_REGION_MODE_REMOVE,
	ATTR_REGION_MODE_CHECK,
};

class SECTREE_MANAGER : public singleton<SECTREE_MANAGER>
{
	public:
		SECTREE_MANAGER();
		~SECTREE_MANAGER() override;

		SECTREE_MAP * GetMap(int32_t lMapIndex);
		SECTREE * 	Get(uint32_t dwIndex, uint32_t package);
		SECTREE * 	Get(uint32_t dwIndex, uint32_t x, uint32_t y);

		template< typename Func >
		void for_each( int iMapIndex, Func & rfunc )
		{
			SECTREE_MAP * pSecMap = SECTREE_MANAGER::instance().GetMap( iMapIndex );
			if ( pSecMap )
			{
				pSecMap->for_each( rfunc );
			}
		}
		
		int		LoadSettingFile(int32_t lIndex, const char * c_pszSettingFileName, TMapSetting & r_setting);
		bool		LoadMapRegion(const char * c_pszFileName, TMapSetting & r_Setting, const char * c_pszMapName);
		int		Build(const char * c_pszListFileName, const char* c_pszBasePath);
		SECTREE_MAP * BuildSectreeFromSetting(TMapSetting & r_setting);
		bool		LoadAttribute(SECTREE_MAP * pkMapSectree, const char * c_pszFileName, TMapSetting & r_setting);
		void		LoadDungeon(int iIndex, const char * c_pszFileName);
		bool		GetValidLocation(int32_t lMapIndex, int32_t x, int32_t y, int32_t & r_lValidMapIndex, PIXEL_POSITION & r_pos, uint8_t empire = 0);
		bool		GetRecallPositionByEmpire(int iMapIndex, uint8_t bEmpire, PIXEL_POSITION & r_pos);

		const TMapRegion *	GetMapRegion(int32_t lMapIndex);
		int			GetMapIndex(int32_t x, int32_t y);
		const TMapRegion *	FindRegionByPartialName(const char* szMapName);

		bool		GetMapBasePosition(int32_t x, int32_t y, PIXEL_POSITION & r_pos);
		bool		GetMapBasePositionByMapIndex(int32_t lMapIndex, PIXEL_POSITION & r_pos);
		bool		GetMovablePosition(int32_t lMapIndex, int32_t x, int32_t y, PIXEL_POSITION & pos);
		bool		IsMovablePosition(int32_t lMapIndex, int32_t x, int32_t y);
		bool		GetCenterPositionOfMap(int32_t lMapIndex, PIXEL_POSITION & r_pos);
		bool        GetRandomLocation(int32_t lMapIndex, PIXEL_POSITION & r_pos, uint32_t dwCurrentX = 0, uint32_t dwCurrentY = 0, int iMaxDistance = 0);

		int32_t		CreatePrivateMap(int32_t lMapIndex);	// returns new private map index, returns 0 when fail
		void		DestroyPrivateMap(int32_t lMapIndex);

		TAreaMap&	GetDungeonArea(int32_t lMapIndex);
		void		SendNPCPosition(CHARACTER * ch);
		void		InsertNPCPosition(int32_t lMapIndex, uint8_t bType, const char* szName, int32_t x, int32_t y);

		uint8_t		GetEmpireFromMapIndex(int32_t lMapIndex);

		void		PurgeMonstersInMap(int32_t lMapIndex);
		void		PurgeStonesInMap(int32_t lMapIndex);
		size_t		GetMonsterCountInMap(int32_t lMapIndex);
		size_t		GetMonsterCountInMap(int32_t lMpaIndex, uint32_t dwVnum);

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
		bool		ForAttrRegion(int32_t lMapIndex, int32_t lStartX, int32_t lStartY, int32_t lEndX, int32_t lEndY, int32_t lRotate, uint32_t dwAttr, EAttrRegionMode mode);

		bool		SaveAttributeToImage(int lMapIndex, const char * c_pszFileName, SECTREE_MAP * pMapSrc = nullptr);

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
		bool		ForAttrRegionRightAngle( int32_t lMapIndex, int32_t lCX, int32_t lCY, int32_t lCW, int32_t lCH, int32_t lRotate, uint32_t dwAttr, EAttrRegionMode mode );

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
		bool		ForAttrRegionFreeAngle( int32_t lMapIndex, int32_t lCX, int32_t lCY, int32_t lCW, int32_t lCH, int32_t lRotate, uint32_t dwAttr, EAttrRegionMode mode );

		/// Performs a specific operation on the Attribute of a single Cell.
		/**
		 * @param [in]	lMapIndex Map index to apply to
		 * @param [in]	lCX X coordinate of the Cell
		 * @param [in]	lCY Y coordinate of the Cell
		 * @param [in]	dwAttr Attribute to apply
		 * @param [in]	mode type of operation on the Attribute
		 */
		bool		ForAttrRegionCell( int32_t lMapIndex, int32_t lCX, int32_t lCY, uint32_t dwAttr, EAttrRegionMode mode );

		static uint16_t			current_sectree_version;
		std::map<uint32_t, SECTREE_MAP *>	m_map_pkSectree;
		std::map<int, TAreaMap>	m_map_pkArea;
		std::vector<TMapRegion>		m_vec_mapRegion;
		std::map<uint32_t, std::vector<npc_info> > m_mapNPCPosition;

		// <Factor> Circular private map indexing
		typedef std::unordered_map<int32_t, int> PrivateIndexMapType;
		PrivateIndexMapType next_private_index_map_;
};
}
