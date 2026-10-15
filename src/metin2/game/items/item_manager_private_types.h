#include <metin2/proto/length.h>

#include <cstdint>
#include <vector>

#pragma once

namespace metin2::game::inline items {

class item_drop_info
{
public:
	item_drop_info(int iLevelStart, int iLevelEnd, int iPercent, uint32_t dwVnum) :
	  level_start(iLevelStart), level_end(iLevelEnd), percent(iPercent), vnum(dwVnum)
	  {
	  }

	  int	level_start;
	  int	level_end;
	  int	percent; // 1 ~ 1000
	  uint32_t	vnum;

	  friend bool operator < (const item_drop_info & l, const item_drop_info & r)
	  {
		  return l.level_end < r.level_end;
	  }
};

extern std::vector<item_drop_info> vec_pk_common_drop_item[MOB_RANK_MAX_NUM];

struct drop_item {
	int		lv_start;
	int		lv_end;
	float	percent;
	char	item_name[PROTO_NAME_MAX_LEN + 1];
	int		count;
};
}
