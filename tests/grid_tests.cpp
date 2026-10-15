#include "check.h"

#include <metin2/common/grid.h>

using namespace metin2;

TEST(grid_places_and_frees_items) {
    grid grid(5, 3);
    CHECK_EQ(grid.size(), 15u);
    CHECK_EQ(grid.find_blank(1, 1), 0);

    CHECK(grid.put(0, 2, 2));
    CHECK(!grid.is_empty(6, 1, 1));
    CHECK(grid.is_empty(2, 1, 1));
    CHECK(!grid.put(1, 1, 1));
    CHECK_EQ(grid.find_blank(2, 2), 2);

    grid.get(0, 2, 2);
    CHECK(grid.is_empty(0, 2, 2));
}

TEST(grid_rejects_blocks_outside) {
    grid grid(5, 3);
    CHECK(!grid.is_empty(-1, 1, 1));
    CHECK(!grid.put(4, 2, 1));
    CHECK(!grid.put(10, 1, 2));
    CHECK_EQ(grid.find_blank(6, 1), -1);
    CHECK_EQ(grid.find_blank(1, 4), -1);
}

TEST(grid_finds_no_blank_when_full) {
    grid grid(2, 2);
    CHECK(grid.put(0, 2, 2));
    CHECK_EQ(grid.find_blank(1, 1), -1);

    grid.clear();
    CHECK_EQ(grid.find_blank(2, 2), 0);
}

TEST(grid_resize_keeps_leading_cells) {
    grid grid(5, 1);
    CHECK(grid.put(0, 1, 1));

    const metin2::common::grid larger(grid, 5, 3);
    CHECK_EQ(larger.size(), 15u);
    CHECK(!larger.is_empty(0, 1, 1));
    CHECK(larger.is_empty(5, 5, 2));
}
