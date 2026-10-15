#include "check.h"

#include <libgame/grid.h>

TEST(grid_places_and_frees_items) {
    CGrid grid(5, 3);
    CHECK_EQ(grid.GetSize(), 15u);
    CHECK_EQ(grid.FindBlank(1, 1), 0);

    // A 2x2 item at the top-left covers cells 0, 1, 5 and 6.
    CHECK(grid.Put(0, 2, 2));
    CHECK(!grid.IsEmpty(6, 1, 1));
    CHECK(grid.IsEmpty(2, 1, 1));
    CHECK(!grid.Put(1, 1, 1));
    CHECK_EQ(grid.FindBlank(2, 2), 2);

    grid.Get(0, 2, 2);
    CHECK(grid.IsEmpty(0, 2, 2));
}

TEST(grid_rejects_blocks_outside) {
    CGrid grid(5, 3);
    CHECK(!grid.IsEmpty(-1, 1, 1));
    CHECK(!grid.Put(4, 2, 1)); // would wrap into the next row
    CHECK(!grid.Put(10, 1, 2)); // would run past the last row
    CHECK_EQ(grid.FindBlank(6, 1), -1);
    CHECK_EQ(grid.FindBlank(1, 4), -1);
}

TEST(grid_finds_no_blank_when_full) {
    CGrid grid(2, 2);
    CHECK(grid.Put(0, 2, 2));
    CHECK_EQ(grid.FindBlank(1, 1), -1);

    grid.Clear();
    CHECK_EQ(grid.FindBlank(2, 2), 0);
}

TEST(grid_resize_keeps_leading_cells) {
    CGrid grid(5, 1);
    CHECK(grid.Put(0, 1, 1));

    // Safebox growth: same width, more rows.
    const CGrid larger(grid, 5, 3);
    CHECK_EQ(larger.GetSize(), 15u);
    CHECK(!larger.IsEmpty(0, 1, 1));
    CHECK(larger.IsEmpty(5, 5, 2));
}
