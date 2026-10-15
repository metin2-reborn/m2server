

#include <metin2/game/items/blend_item.h>

#include <metin2/game/app/constants.h>
#include <metin2/game/items/item.h>
#include <metin2/game/text/locale_service.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>

#include <metin2/common/utils.h>
#include <metin2/log/log.h>
#include <metin2/core/utils.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#define DO_ALL_BLEND_INFO(iter)	for (iter=blend_info.begin(); iter!=blend_info.end(); ++iter)

namespace metin2::game::inline items {


struct blend_item_info
{
	uint32_t	item_vnum;
	int		apply_type;
	int		apply_value[MAX_BLEND_ITEM_VALUE];
	int		apply_duration[MAX_BLEND_ITEM_VALUE];
};


typedef std::vector<blend_item_info>	t_blend_item_info;
t_blend_item_info	blend_info;

bool	blend_item_init()
{
	char	file_name[256];
	snprintf (file_name, sizeof(file_name), "%s/blend.txt", locale_service_get_base_path().c_str());

	metin2::log::info( "Blend_Item_init {} ", file_name);

	blend_info.clear();

	if (false==blend_item_load(file_name))
	{
		metin2::log::error("<Blend_Item_init> fail");
		return false;
	}
	return true;
}

bool	blend_item_load(char *file)
{
	FILE	*fp;
	char	one_line[256];
	const char	*delim = " \t\r\n";
	char	*v;

	blend_item_info	blend_item_info{};

	if (nullptr==file || 0==file[0])
		return false;

	if ((fp = fopen(file, "r"))==nullptr)
		return false;

	while (fgets(one_line, 256, fp))
	{
		if (one_line[0]=='#')
			continue;

		const char* token_string = strtok(one_line, delim);

		if (nullptr==token_string)
			continue;

		TOKEN("section")
		{
			blend_item_info = {};
		}
		else TOKEN("item_vnum")
		{
			v = strtok(nullptr, delim);

			if (nullptr==v)
			{
				fclose(fp);
				return false;
			}

			str_to_number(blend_item_info.item_vnum, v);
		}
		else TOKEN("apply_type")
		{
			v = strtok(nullptr, delim);

			if (nullptr==v)
			{
				fclose(fp);
				return false;
			}

			if (0 == (blend_item_info.apply_type = fn_get_apply_type(v)))
			{
				metin2::log::error("Invalid apply_type({})", v);
				fclose(fp);
				return false;
			}
		}
		else TOKEN("apply_value")
		{
			for (int i=0; i<MAX_BLEND_ITEM_VALUE; ++i)
			{
				v = strtok(nullptr, delim);

				if (nullptr==v)
				{
					fclose(fp);
					return false;
				}

				str_to_number(blend_item_info.apply_value[i], v); 
			}
		}
		else TOKEN("apply_duration")
		{
			for (int i=0; i<MAX_BLEND_ITEM_VALUE; ++i)
			{
				v = strtok(nullptr, delim);

				if (nullptr==v)
				{
					fclose(fp);
					return false;
				}

				str_to_number(blend_item_info.apply_duration[i], v);
			}
		}
		else TOKEN("end")
		{
			blend_info.push_back(blend_item_info);
		}
	}

	fclose(fp);

	return true;
}

static int fn_random_index()
{
	int	percent = number(1,100);

	if (percent<=10)
		return 0;
	else if (percent<=30)
		return 1;
	else if (percent<=70)
		return 2;
	else if (percent<=90)
		return 3;
	else
		return 4;

	return 0;
}


static int fn_ecs_random_index()
{
	int	percent = number(1,100);

	if (percent<=5)
		return 0;
	else if (percent<=15)
		return 1;
	else if (percent<=60)
		return 2;
	else if (percent<=85)
		return 3;
	else
		return 4;

	return 0;
}


bool	blend_item_set_value(item * item)
{
	blend_item_info	*blend_info_value;
	t_blend_item_info::iterator	iter;

	DO_ALL_BLEND_INFO(iter)
	{
		blend_info_value = &*iter;
		if (blend_info_value->item_vnum == item->vnum())
		{
			int	apply_type;
			int	apply_value;
			int	apply_duration;
	
			if (item->vnum() == 51002)
			{
				apply_type		= blend_info_value->apply_type;
				apply_value		= blend_info_value->apply_value		[fn_ecs_random_index()];
				apply_duration	= blend_info_value->apply_duration	[fn_ecs_random_index()];
			}
			else
			{
				apply_type		= blend_info_value->apply_type;
				apply_value		= blend_info_value->apply_value		[fn_random_index()];
				apply_duration	= blend_info_value->apply_duration	[fn_random_index()];
			}
			metin2::log::info( "blend_item : type : {:d}, value : {:d}, du : {:d}", apply_type, apply_value, apply_duration);
			item->set_socket(0, apply_type);
			item->set_socket(1, apply_value);
			item->set_socket(2, apply_duration);
			return true;
		}

	}
	return false;
}

bool	blend_item_find(uint32_t item_vnum)
{
	blend_item_info	*blend_info_value;
	t_blend_item_info::iterator	iter;

	DO_ALL_BLEND_INFO(iter)
	{
		blend_info_value = &*iter;
		if (blend_info_value->item_vnum == item_vnum)
			return true;
	}
	return false;
}
}
