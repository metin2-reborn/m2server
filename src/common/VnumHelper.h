#pragma once

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

namespace metin2::inline proto {

class CItemVnumHelper {
  public:
    /// Ramadan event Crescent Moon Ring (originally a special item for the Ramadan event, but it will reportedly keep being reused in various ways)
    static constexpr auto IsRamadanMoonRing(uint32_t vnum) -> bool { return 71135 == vnum; }

    /// Halloween candy (same spec as the Crescent Moon Ring)
    static constexpr auto IsHalloweenCandy(uint32_t vnum) -> bool { return 71136 == vnum; }

    /// Christmas Ring of Happiness
    static constexpr auto IsHappinessRing(uint32_t vnum) -> bool { return 71143 == vnum; }

    /// Valentine Pendant of Love
    static constexpr auto IsLovePendant(uint32_t vnum) -> bool { return 71145 == vnum; }
};

class CMobVnumHelper {
  public:
    /// Black horse for the Ramadan event reward (20119) .. Ramadan black horse clone for the Halloween event (same spec, 20219)
    static constexpr auto IsRamadanBlackHorse(uint32_t vnum) -> bool {
        return 20119 == vnum ||
               20219 == vnum ||
               22022 == vnum;
    }
};
}
