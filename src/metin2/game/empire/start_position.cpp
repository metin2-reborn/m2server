#include <metin2/game/empire/start_position.h>

#include <cstdint>

namespace metin2::game::inline empire {


char nation_name[4][32] =
{
	"",
	"신수국",
	"천조국",
	"진노국",
};


int32_t start_map[4] =
{
	0,
	1,
	21,
	41
};

uint32_t start_position[4][2] =
{
	{      0,      0 },
	{ 469300, 964200 },
	{  55700, 157900 },
	{ 969600, 278400 }
};


uint32_t arena_return_position[4][2] =
{
	{       0,  0       },
	{   347600, 882700  },
	{   138600, 236600  },
	{   857200, 251800  }
};


uint32_t create_position[4][2] = 
{
	{		0,		0 },
	{ 459800, 953900 },
	{ 52070, 166600 },
	{ 957300, 255200 },	
};
}
