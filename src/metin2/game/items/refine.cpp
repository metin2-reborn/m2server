#include <metin2/game/items/refine.h>

#include <metin2/proto/tables.h>
#include <metin2/log/log.h>

#include <cstdint>
#include <utility>

namespace metin2::game::inline items {

CRefineManager::CRefineManager()
{
}

CRefineManager::~CRefineManager()
{
}

bool CRefineManager::Initialize(TRefineTable * table, int size)
{
	for (int i = 0; i < size; ++i, ++table)
	{
		metin2::log::info( "REFINE {:d} prob {:d} cost {:d}", table->id, table->prob, table->cost);
		m_map_RefineRecipe.insert(std::make_pair(table->id, *table));
	}

	metin2::log::info( "REFINE: COUNT {:d}", m_map_RefineRecipe.size());
	return true;
}

const TRefineTable* CRefineManager::GetRefineRecipe(uint32_t vnum)
{
	if (vnum == 0)
		return nullptr;

	auto it = m_map_RefineRecipe.find(vnum);
	metin2::log::info( "REFINE: FIND {:d} {}", vnum, it == m_map_RefineRecipe.end() ? "FALSE" : "TRUE");

	if (it == m_map_RefineRecipe.end())
	{
		return nullptr;
	}

	return &it->second;
}
}
