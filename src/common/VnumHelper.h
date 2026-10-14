#ifndef __HEADER_VNUM_HELPER__
#define	__HEADER_VNUM_HELPER__

/**
	Currently, existing and future items, mobs, etc. are identified in the source by
	hardcoded identifiers (numbers = VNum), which hurts readability badly.

	Added at Seung-cheol's suggestion so that you can tell which item (or mob) it is just by reading the source.

		* This file is expected to change often; putting it in the PCH would force a full rebuild on every change, 
		so for now it is included only in the cpp files that need it.

		* Implementing it in a cpp would require compiling ~ linking, so only a header was put in common. (to be used by both the game and db projects)

	@date	2011. 8. 29.
*/


#include <cstdint>

class CItemVnumHelper
{
public:
	/// Ramadan event Crescent Moon Ring (originally a special item for the Ramadan event, but it will reportedly keep being reused in various ways)
	static constexpr bool	IsRamadanMoonRing(uint32_t vnum)		{ return 71135 == vnum; }

	/// Halloween candy (same spec as the Crescent Moon Ring)
	static constexpr bool	IsHalloweenCandy(uint32_t vnum)		{ return 71136 == vnum; }

	/// Christmas Ring of Happiness
	static constexpr bool	IsHappinessRing(uint32_t vnum)		{ return 71143 == vnum; }

	/// Valentine Pendant of Love 
	static constexpr bool	IsLovePendant(uint32_t vnum)		{ return 71145 == vnum; }
};

class CMobVnumHelper
{
public:

	/// Black horse for the Ramadan event reward (20119) .. Ramadan black horse clone for the Halloween event (same spec, 20219)
	static constexpr bool	IsRamadanBlackHorse(uint32_t vnum)		{ return 20119 == vnum || 20219 == vnum || 22022 == vnum; }
};

#endif	//__HEADER_VNUM_HELPER__