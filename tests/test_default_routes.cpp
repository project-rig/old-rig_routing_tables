#include <gtest/gtest.h>
#include "default_routes.h"


class DefaultRoutesTest : public ::testing::Test
{
};


TEST(DefaultRoutesTest, test_minimise_orthogonal_table)
{
  // Check for the correct removal of default entries in an orthogonal routing
  // table:
  //
  //   N    -> 0000 -> S    -- Remove
  //   N    -> 0001 -> N    -- Keep
  //   N    -> 0010 -> 0    -- Keep
  //   N S  -> 0011 -> N S  -- Keep
  //   0    -> 0100 -> 0    -- Keep
  RoutingTable::Table table = {
    {{0x0, 0xf}, 0b0000100, 0b0100000},
    {{0x1, 0xf}, 0b0000100, 0b0000100},
    {{0x2, 0xf}, 0b0000100, 0b1000000},
    {{0x3, 0xf}, 0b0100100, 0b0100100},
    {{0x4, 0xf}, 0b1000000, 0b1000000},
  };

  // Remove default routes
  DefaultRoutes::minimise(table);

  // Check that only the first entry has been removed
  ASSERT_EQ(table.size(), 4);
  EXPECT_EQ(table[0].keymask.key, 0x1);
  EXPECT_EQ(table[1].keymask.key, 0x2);
  EXPECT_EQ(table[2].keymask.key, 0x3);
  EXPECT_EQ(table[3].keymask.key, 0x4);
}


TEST(DefaultRoutesTest, test_minimise_nonorthogonal_table)
{
  // Check for the correct removal of default entries in an non-orthogonal
  // routing table:
  //
  //   N -> 1000 -> S  -- Remove
  //   N -> 0000 -> S  -- Keep
  //   N -> 0XXX -> 0  -- Keep
  RoutingTable::Table table = {
    {{0x8, 0xf}, 0b0000100, 0b0100000},
    {{0x0, 0xf}, 0b0000100, 0b0100000},
    {{0x0, 0x8}, 0b0000100, 0b1000000},
  };

  // Remove default routes
  DefaultRoutes::minimise(table);

  // Check that only the first entry has been removed
  ASSERT_EQ(table.size(), 2);

  EXPECT_EQ(table[0].keymask.key, 0x0);
  EXPECT_EQ(table[0].keymask.mask, 0xf);

  EXPECT_EQ(table[1].keymask.key, 0x0);
  EXPECT_EQ(table[1].keymask.mask, 0x8);
}
