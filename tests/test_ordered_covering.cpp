#include <gtest/gtest.h>
#include "ordered_covering.h"


class OrderedCoveringTest : public ::testing::Test
{
};


TEST(OrderedCoveringTest, test_merge_entries)
{
  // Check that wherever bits differ in entries an X is added to the merged
  // entry.
  // Create a table from which to take entries to merge.
  auto table = RoutingTable::Table(3);
  table[0] = {{0x0, 0xffffffff}, 1, 1};  // 00000000000000000000000000000000
  table[1] = {{0x1, 0xffffffff}, 2, 2};  // 00000000000000000000000000000001
  table[2] = {{0x3, 0xffffffff}, 4, 1};  // 00000000000000000000000000000011

  auto merge = OrderedCovering::Merge(table.size(), true);  // Merge all

  // Check the merged entry is correct, (000000000000000000000000000000XX)
  RoutingTable::Entry expected = {{0x0, 0xfffffffc}, 0b111, 0b11};
  EXPECT_TRUE(OrderedCovering::merge_entries(table, merge) == expected);

  // Check that wherever there is an X in a single entry there is an X in the
  // merged entry.
  table[0] = {{0x0, 0xffffffff}, 1, 1};  // 000000000000000000000000000000XX
  table[1] = {{0x1, 0xffffffff}, 2, 4};  // 00000000000000000000000000000X00
  table[2] = {{0x3, 0xffffffff}, 4, 1};  // 000000000000000000000000000000X0

  merge[1] = false; // Don't include the second entry in the table
  expected.source = 0b101;
  expected.route = 0b001;
  EXPECT_TRUE(OrderedCovering::merge_entries(table, merge) == expected);
}


TEST(OrderedCoveringTest, test_get_insertion_index)
{
  using OrderedCovering::get_insertion_index;

  // Construct a table containing only generality 31 entries.
  RoutingTable::Table table = {
    {{0b00, 0b01}, 0x0, 0x0},  // ...X0
    {{0b01, 0b01}, 0x0, 0x0},  // ...X1
    {{0b00, 0b10}, 0x0, 0x0},  // ...0X
    {{0b10, 0b10}, 0x0, 0x0},  // ...1X
  };

  // The insertion index for any generality 30 expression should be at the
  // start of the table:
  EXPECT_TRUE(get_insertion_index(table, 30) == table.begin());

  // Add a generality 30 entry into the start of the table
  table.insert(table.begin(), {{0b00, 0b11}, 0x0, 0x0});  // ...00

  // The insertion index for any generality 30 expression should be one after
  // the beginning of the table.
  EXPECT_TRUE(get_insertion_index(table, 30) == table.begin() + 1);

  // The insertion index for any generality 31 expression should be at the end
  // of the table.
  EXPECT_TRUE(get_insertion_index(table, 31) == table.end());

  // Check that a generality 32-entry would go to the end of the table and then
  // add one.
  EXPECT_TRUE(get_insertion_index(table, {{0x0, 0x0}, 0x0, 0x0}) ==
              table.end());
  table.push_back({{0x0, 0x0}, 0x0, 0x0});

  // Check that any generality 31-entries should go before the final entry
  EXPECT_TRUE(get_insertion_index(table, 31) == table.end() - 1);
}


TEST(OrderedCoveringTest, test_refine_merge_upcheck)
{
  // Test that entries which would be covered by being moved below entries are
  // removed from a merge.
  RoutingTable::Table table = {
    {{0b1101, 0b1111}, 0x2, 0x8},  // 1101
    {{0b1011, 0b1111}, 0x2, 0x8},  // 1011
    {{0b1001, 0b1111}, 0x2, 0x8},  // 1001
    {{0b0001, 0b1111}, 0x2, 0x8},  // 0001
    {{0b0000, 0b1111}, 0x2, 0x8},  // 0001
    {{0b1001, 0b1001}, 0x2, 0x4},  // 1XX1
  };

  // The first 4 entries cannot be merged as this would cause the first 3
  // entries become covered. `refine_merge_upcheck` should remove entries from
  // the merge set and return the number of returned entries.
  auto merge = OrderedCovering::Merge(table.size(), false);
  merge[0] = merge[1] = merge[2] = merge[3] = merge[4] = true;

  auto removed = OrderedCovering::refine_merge_upcheck(table, merge, 0);

  EXPECT_EQ(removed, 3);  // Removed first three entries from the merge
  EXPECT_FALSE(merge[0]);
  EXPECT_FALSE(merge[1]);
  EXPECT_FALSE(merge[2]);
  EXPECT_TRUE(merge[3]);
  EXPECT_TRUE(merge[4]);
  EXPECT_FALSE(merge[5]);  // Never part of merge!

  // If the best goodness is such that a merge should just be discarded assert
  // that an empty merge is the result.
  merge[0] = merge[1] = merge[2] = merge[3] = true;
  removed = OrderedCovering::refine_merge_upcheck(table, merge, 2);

  EXPECT_EQ(removed, 4);  // Removed all three entries from the merge
  EXPECT_FALSE(merge[0]);
  EXPECT_FALSE(merge[1]);
  EXPECT_FALSE(merge[2]);
  EXPECT_FALSE(merge[3]);
  EXPECT_FALSE(merge[4]);  // Never part of merge!
}


TEST(OrderedCoveringTest, test_refine_merge_downcheck_does_nothing_if_no_covers)
{
  // Test that refine_merge_downcheck does nothing if there are no covered
  // entries as the result of a merge.
  // Construct the table:
  //
  //   11001 -> E
  //   11010 -> E
  //   00XXX -> NE
  //   X1XXX -> N  {01000, 11111}
  RoutingTable::Table table = {
    {{0b11001, 0b11111}, 0b010, 0b001},
    {{0b11010, 0b11111}, 0b010, 0b001},
    {{0b10000, 0b11000}, 0b001, 0b010},
    {{0b01000, 0b01000}, 0b001, 0b100},
  };

  OrderedCovering::Aliases aliases = OrderedCovering::Aliases();
  aliases[{0b01000, 0b01000}] = {{0b01000, 0b11111}, {0b11111, 0b11111}};

  OrderedCovering::Merge merge = {true, true, false, false};

  // Check that the merge is not modified at all and that no entries are
  // reported as being removed.
  auto removed = OrderedCovering::refine_merge_downcheck(
    table, aliases, merge, 0);

  EXPECT_EQ(removed, 0);
  EXPECT_TRUE(merge[0]);
  EXPECT_TRUE(merge[1]);
  EXPECT_FALSE(merge[2]);
  EXPECT_FALSE(merge[3]);
}


TEST(OrderedCoveringTest,
     test_refine_merge_downcheck_clears_merge_if_unresolvable)
{
  // Test that refine_merge_downcheck completely empties a merge if it is
  // impossible to avoid covering a lower entry.
  // Construct the table:
  //
  //   1001 -> E
  //   1010 -> E
  //   1XXX -> N
  RoutingTable::Table table = {
    {{0b1001, 0b1111}, 0b010, 0b001},
    {{0b1010, 0b1111}, 0b010, 0b001},
    {{0b1000, 0b1000}, 0b001, 0b100},
  };
  OrderedCovering::Aliases aliases = OrderedCovering::Aliases();
  OrderedCovering::Merge merge = {true, true, false};

  // Check that the merge is emptied and that all entries are reported to have
  // been removed.
  auto removed = OrderedCovering::refine_merge_downcheck(
    table, aliases, merge, 0);

  EXPECT_EQ(removed, 2);
  EXPECT_FALSE(merge[0]);
  EXPECT_FALSE(merge[1]);
  EXPECT_FALSE(merge[2]);

  // Modify the aliases table so that the table is:
  //
  //   1001 -> E
  //   1010 -> E
  //   1XXX -> N  {1011, 1100}
  aliases[{0b1000, 0b1000}] = {{0b1011, 0b1111}, {0b1100, 0b1111}};
  merge = {true, true, false};  // Reset the merge

  // Check that the merge is emptied and that all entries are reported to have
  // been removed.
  removed = OrderedCovering::refine_merge_downcheck(table, aliases, merge, 0);

  EXPECT_EQ(removed, 2);
  EXPECT_FALSE(merge[0]);
  EXPECT_FALSE(merge[1]);
  EXPECT_FALSE(merge[2]);
}


TEST(OrderedCoveringTest,
     test_refine_merge_downcheck_removes_one_entry)
{
  // Test that refine_merge_downcheck completely removes a single entry from
  // merge if it is necessary to do so to avoid covering a lower entry.
  // Construct the table:
  //
  //   1000 -> E
  //   0000 -> E
  //   0001 -> E
  //   1XXX -> N
  //
  RoutingTable::Table table = {
    {{0b1001, 0b1111}, 0b010, 0b001},
    {{0b0000, 0b1111}, 0b010, 0b001},
    {{0b0001, 0b1111}, 0b010, 0b001},
    {{0b1000, 0b1000}, 0b001, 0b100},
  };
  OrderedCovering::Aliases aliases = OrderedCovering::Aliases();
  OrderedCovering::Merge merge = {true, true, true, false};

  // Check that the first entry is removed from the merge.
  auto removed = OrderedCovering::refine_merge_downcheck(
    table, aliases, merge, 0);
  EXPECT_EQ(removed, 1);
  EXPECT_FALSE(merge[0]);
  EXPECT_TRUE(merge[1]);
  EXPECT_TRUE(merge[2]);
  EXPECT_FALSE(merge[3]);  // Never part of the merge

  // Test that refine_merge_downcheck completely removes a single entry from
  // merge if it is necessary to do so to avoid covering a lower entry.
  // Construct the table:
  //
  //   0000 -> E
  //   1000 -> E
  //   1001 -> E
  //   0XXX -> N
  //
  table = {
    {{0b0001, 0b1111}, 0b010, 0b001},
    {{0b1000, 0b1111}, 0b010, 0b001},
    {{0b1001, 0b1111}, 0b010, 0b001},
    {{0b0000, 0b1000}, 0b001, 0b100},
  };
  merge = {true, true, true, false};

  // Check that the first entry is removed from the merge.
  removed = OrderedCovering::refine_merge_downcheck(table, aliases, merge, 0);
  EXPECT_EQ(removed, 1);
  EXPECT_FALSE(merge[0]);
  EXPECT_TRUE(merge[1]);
  EXPECT_TRUE(merge[2]);
  EXPECT_FALSE(merge[3]);  // Never part of the merge
}


TEST(OrderedCoveringTest, test_refine_merge_downcheck_iterates)
{
  // Check that if there are multiple covered entries refine_merge_downcheck
  // will remove sufficient entries from the merge to avoid covering all of
  // them.
  //
  //   00000 -> N
  //   00100 -> N
  //   11000 -> N
  //   11100 -> N
  //   X0XXX -> NE
  //   1XXXX -> E
  RoutingTable::Table table = {
    {{0b00000, 0b11111}, 0b001, 0b100},
    {{0b00100, 0b11111}, 0b001, 0b100},
    {{0b11000, 0b11111}, 0b001, 0b100},
    {{0b10100, 0b11111}, 0b001, 0b100},
    {{0b00000, 0b01000}, 0b001, 0b010},
    {{0b10000, 0b10000}, 0b010, 0b001},
  };
  auto aliases = OrderedCovering::Aliases();
  OrderedCovering::Merge merge = {true, true, true, true, false, false};

  // Should remove all entries from the merge (should require the function to
  // iterate until there are no covers).
  auto removed = OrderedCovering::refine_merge_downcheck(table, aliases,
                                                         merge, 0);
  EXPECT_EQ(removed, 4);
  EXPECT_FALSE(merge[0]);
  EXPECT_FALSE(merge[1]);
  EXPECT_FALSE(merge[2]);
  EXPECT_FALSE(merge[3]);
  EXPECT_FALSE(merge[4]);  // Never part of the merge
  EXPECT_FALSE(merge[5]);  // Never part of the merge
}


TEST(OrderedCoveringTest, test_merge_apply_at_start_of_table)
{
  // Merge the first two entries:
  //
  //   E -> 0000 -> N
  //   W -> 0001 -> N
  //   N -> XXXX -> S
  //
  // The result should be:
  //
  //   E W -> 000X -> N {0000, 0001}
  //     N -> XXXX -> S
  RoutingTable::Table table = {
    {{0x0, 0xf}, 0b000001, 0b000100},
    {{0x1, 0xf}, 0b001000, 0b000100},
    {{0x0, 0x0}, 0b000100, 0b100000},
  };
  auto aliases = OrderedCovering::Aliases();
  OrderedCovering::Merge merge = {true, true, false};

  // Apply the merge
  OrderedCovering::merge_apply(table, aliases, merge);

  // Check that the table is correct.
  EXPECT_EQ(table.size(), 2);  // Now contains two entries

  // [0] = E W -> 000X -> N
  EXPECT_EQ(table[0].keymask.key, 0x0);
  EXPECT_EQ(table[0].keymask.mask, 0xe);
  EXPECT_EQ(table[0].source, 0b001001);
  EXPECT_EQ(table[0].route, 0b000100);

  // [1] = N -> XXXX -> S
  EXPECT_EQ(table[1].keymask.key, 0x0);
  EXPECT_EQ(table[1].keymask.mask, 0x0);
  EXPECT_EQ(table[1].source, 0b000100);
  EXPECT_EQ(table[1].route, 0b100000);

  // Check that the aliases map is correct
  EXPECT_EQ(aliases.size(), 1);  // Only one entry in the aliases dictionary
  EXPECT_EQ(aliases.count({0x0, 0xe}), 1);  // 000X in the aliases dict
  auto alias_list = (*aliases.find({0x0, 0xe})).second;
  EXPECT_EQ(alias_list.count({0x0, 0xf}), 1);  // 0000 in the set for 000X
  EXPECT_EQ(alias_list.count({0x1, 0xf}), 1);  // 0001 in the set for 000X
}


TEST(OrderedCoveringTest, test_merge_apply_at_end_of_table)
{
  // Merge the first two entries:
  //
  //   E -> 0000 -> N
  //   W -> 0001 -> N
  //   N -> 1111 -> S
  //
  // The result should be:
  //
  //     N -> 1111 -> S
  //   E W -> 000X -> N {0000, 0001}
  RoutingTable::Table table = {
    {{0x0, 0xf}, 0b000001, 0b000100},
    {{0x1, 0xf}, 0b001000, 0b000100},
    {{0xf, 0xf}, 0b000100, 0b100000},
  };
  auto aliases = OrderedCovering::Aliases();
  OrderedCovering::Merge merge = {true, true, false};

  // Apply the merge
  OrderedCovering::merge_apply(table, aliases, merge);

  // Check that the table is correct.
  EXPECT_EQ(table.size(), 2);  // Now contains two entries

  // [0] = N -> XXXX -> S
  EXPECT_EQ(table[0].keymask.key, 0xf);
  EXPECT_EQ(table[0].keymask.mask, 0xf);
  EXPECT_EQ(table[0].source, 0b000100);
  EXPECT_EQ(table[0].route, 0b100000);

  // [1] = E W -> 000X -> N
  EXPECT_EQ(table[1].keymask.key, 0x0);
  EXPECT_EQ(table[1].keymask.mask, 0xe);
  EXPECT_EQ(table[1].source, 0b001001);
  EXPECT_EQ(table[1].route, 0b000100);

  // Check that the aliases map is correct
  EXPECT_EQ(aliases.size(), 1);  // Only one entry in the aliases dictionary
  EXPECT_EQ(aliases.count({0x0, 0xe}), 1);  // 000X in the aliases dict
  auto alias_list = (*aliases.find({0x0, 0xe})).second;
  EXPECT_EQ(alias_list.count({0x0, 0xf}), 1);  // 0000 in the set for 000X
  EXPECT_EQ(alias_list.count({0x1, 0xf}), 1);  // 0001 in the set for 000X
}


TEST(OrderedCoveringTest, test_merge_apply_mid_table)
{
  // Merge the first two entries:
  //
  //   E -> 0000 -> N
  //   W -> 0001 -> N
  //   N -> 1111 -> S
  //   N -> XXXX -> E
  //
  // The result should be:
  //
  //     N -> 1111 -> S
  //   E W -> 000X -> N {0000, 0001}
  //     N -> XXXX -> E
  RoutingTable::Table table = {
    {{0x0, 0xf}, 0b000001, 0b000100},
    {{0x1, 0xf}, 0b001000, 0b000100},
    {{0xf, 0xf}, 0b000100, 0b100000},
    {{0x0, 0x0}, 0b000100, 0b000001},
  };
  auto aliases = OrderedCovering::Aliases();
  OrderedCovering::Merge merge = {true, true, false, false};

  // Apply the merge
  OrderedCovering::merge_apply(table, aliases, merge);

  // Check that the table is correct.
  EXPECT_EQ(table.size(), 3);

  // [0] = N -> XXXX -> S
  EXPECT_EQ(table[0].keymask.key, 0xf);
  EXPECT_EQ(table[0].keymask.mask, 0xf);
  EXPECT_EQ(table[0].source, 0b000100);
  EXPECT_EQ(table[0].route, 0b100000);

  // [1] = E W -> 000X -> N
  EXPECT_EQ(table[1].keymask.key, 0x0);
  EXPECT_EQ(table[1].keymask.mask, 0xe);
  EXPECT_EQ(table[1].source, 0b001001);
  EXPECT_EQ(table[1].route, 0b000100);

  // [2] = N -> XXXX -> E
  EXPECT_EQ(table[2].keymask.key, 0x0);
  EXPECT_EQ(table[2].keymask.mask, 0x0);
  EXPECT_EQ(table[2].source, 0b000100);
  EXPECT_EQ(table[2].route, 0b000001);

  // Check that the aliases map is correct
  EXPECT_EQ(aliases.size(), 1);  // Only one entry in the aliases dictionary
  EXPECT_EQ(aliases.count({0x0, 0xe}), 1);  // 000X in the aliases dict
  auto alias_list = (*aliases.find({0x0, 0xe})).second;
  EXPECT_EQ(alias_list.count({0x0, 0xf}), 1);  // 0000 in the set for 000X
  EXPECT_EQ(alias_list.count({0x1, 0xf}), 1);  // 0001 in the set for 000X
}


TEST(OrderedCoveringTest, test_merge_apply_updates_aliases)
{
  // Merge the first two entries:
  //
  //   N -> 1111 -> S
  //   E -> 000X -> N {0000, 0001}
  //   W -> 001X -> N {0010, 0011}
  //
  // The result should be:
  //
  //     N -> 1111 -> S
  //   E W -> 00XX -> N {0000, 0001, 0010, 0011}
  RoutingTable::Table table = {
    {{0xf, 0xf}, 0b000100, 0b100000},
    {{0x0, 0xe}, 0b000001, 0b000100},
    {{0x2, 0xe}, 0b001000, 0b000100},
  };
  auto aliases = OrderedCovering::Aliases();
  OrderedCovering::Merge merge = {false, true, true};

  // Add to the aliases table
  aliases[{0x0, 0xe}].insert({0x0, 0xf});
  aliases[{0x0, 0xe}].insert({0x1, 0xf});
  aliases[{0x2, 0xe}].insert({0x2, 0xf});
  aliases[{0x2, 0xe}].insert({0x3, 0xf});

  // Apply the merge
  OrderedCovering::merge_apply(table, aliases, merge);

  // Check that the aliases map is correct
  EXPECT_EQ(aliases.size(), 1);  // Only one entry in the aliases dictionary
  ASSERT_EQ(aliases.count({0x0, 0xc}), 1);  // 00XX in the aliases dict
  auto alias_list = (*aliases.find({0x0, 0xc})).second;
  EXPECT_EQ(alias_list.count({0x0, 0xf}), 1);  // 0000 in the set for 00XX
  EXPECT_EQ(alias_list.count({0x1, 0xf}), 1);  // 0001 in the set for 00XX
  EXPECT_EQ(alias_list.count({0x2, 0xf}), 1);  // 0010 in the set for 00XX
  EXPECT_EQ(alias_list.count({0x3, 0xf}), 1);  // 0011 in the set for 00XX
}


TEST(OrderedCoveringTest, test_get_best_merge_returns_largest_merge)
{
  // Test that for a table with three valid merges "get_best_merge" returns the
  // largest merge, which is the second merge it will encounter.
  //
  //   0000 -> E
  //   0001 -> E
  //   0010 -> E
  //   0011 -> NE
  //   0100 -> NE
  //   0101 -> NE
  //   0110 -> NE
  //   0111 -> N
  //   1000 -> N
  auto table = RoutingTable::Table(9);
  for (unsigned int i = 0; i < table.size(); i++)
  {
    table[i].keymask.key = i;
    table[i].keymask.mask = 0xf;

    if (i < 3)
    {
      table[i].route = 0b001;
    }
    else if( i < 7)
    {
      table[i].route = 0b010;
    }
    else
    {
      table[i].route = 0b100;
    }
  }

  // Empty aliases table
  auto aliases = OrderedCovering::Aliases();

  // Check that the largest merge is returned
  auto merge = OrderedCovering::get_best_merge(table, aliases);
  ASSERT_EQ(merge.size(), table.size());
  EXPECT_FALSE(merge[0]);
  EXPECT_FALSE(merge[1]);
  EXPECT_FALSE(merge[2]);
  EXPECT_TRUE(merge[3]);
  EXPECT_TRUE(merge[4]);
  EXPECT_TRUE(merge[5]);
  EXPECT_TRUE(merge[6]);
  EXPECT_FALSE(merge[7]);
  EXPECT_FALSE(merge[8]);
}


TEST(OrderedCoveringTest, test_get_best_merge_applies_downcheck)
{
  // Test that a downcheck is applied and an invalid merge is avoided.
  //
  //   00000000 -> E
  //   00010000 -> E
  //   00100000 -> E
  //   10000000 -> E
  //   11110000 -> E
  //   1XXXXXXX -> N
  RoutingTable::Table table = {
    {{0x00, 0xff}, 0b010, 0b001},
    {{0x10, 0xff}, 0b010, 0b001},
    {{0x20, 0xff}, 0b010, 0b001},
    {{0x80, 0xff}, 0b010, 0b001},
    {{0xf0, 0xff}, 0b010, 0b001},
    {{0x80, 0x80}, 0b110, 0b100},
  };

  // Empty aliases table
  auto aliases = OrderedCovering::Aliases();

  // Check that the largest VALID merge is returned
  auto merge = OrderedCovering::get_best_merge(table, aliases);
  ASSERT_EQ(merge.size(), table.size());
  EXPECT_TRUE(merge[0]);
  EXPECT_TRUE(merge[1]);
  EXPECT_TRUE(merge[2]);
  EXPECT_FALSE(merge[3]);
  EXPECT_FALSE(merge[4]);
  EXPECT_FALSE(merge[5]);
}


TEST(OrderedCoveringTest, test_get_best_merge_applies_upcheck)
{
  // Test that an upcheck is applied and an invalid merge is avoided.
  //
  //   0000 -> E
  //   0001 -> E
  //   0010 -> E
  //   1000 -> E
  //   1111 -> E
  //   1XXX -> N
  RoutingTable::Table table = {
    {{0x0, 0xf}, 0b010, 0b001},
    {{0x1, 0xf}, 0b010, 0b001},
    {{0x2, 0xf}, 0b010, 0b001},
    {{0x8, 0xf}, 0b010, 0b001},
    {{0xf, 0xf}, 0b010, 0b001},
    {{0x8, 0x8}, 0b110, 0b100},
  };

  // Empty aliases table
  auto aliases = OrderedCovering::Aliases();

  // Check that the largest VALID merge is returned
  auto merge = OrderedCovering::get_best_merge(table, aliases);
  ASSERT_EQ(merge.size(), table.size());
  EXPECT_TRUE(merge[0]);
  EXPECT_TRUE(merge[1]);
  EXPECT_TRUE(merge[2]);
  EXPECT_FALSE(merge[3]);
  EXPECT_FALSE(merge[4]);
  EXPECT_FALSE(merge[5]);
}


TEST(OrderedCoveringTest, test_get_best_merge_applies_second_downcheck)
{
  // Test that a down-check is re-applied after a resolved up-check.
  //
  //   00000000 -> N
  //   00011111 -> N
  //   11100000 -> N
  //   1110000X -> E
  //   XXX01XXX -> NE
  //
  // We expect only the first two entries to be contained within the merge.
  RoutingTable::Table table = {
    {{0x00, 0xff}, 0b001, 0b100},
    {{0x1f, 0xff}, 0b001, 0b100},
    {{0xe0, 0xff}, 0b001, 0b100},
    {{0xe0, 0xfe}, 0b010, 0b001},
    {{0x08, 0x18}, 0b110, 0b010},
  };

  // Empty aliases table
  auto aliases = OrderedCovering::Aliases();

  // There should be no merge because there is no valid merge...
  auto merge = OrderedCovering::get_best_merge(table, aliases);
  ASSERT_EQ(merge.size(), table.size());
  EXPECT_FALSE(merge[0]);
  EXPECT_FALSE(merge[1]);
  EXPECT_FALSE(merge[2]);
  EXPECT_FALSE(merge[3]);
  EXPECT_FALSE(merge[4]);
}


TEST(OrderedCoveringTest, test_ordered_covering_full)
{
  // Test that the given table is minimised correctly::
  //
  //   0000 -> N NE
  //   0001 -> E
  //   0101 -> SW
  //   1000 -> N NE
  //   1001 -> E
  //   1110 -> SW
  //   1100 -> N NE
  //   0100 -> S SW
  //
  // The result (worked out by hand) should be:
  //
  //   0100 -> S SW
  //   X001 -> E
  //   XX00 -> N NE
  //   X1XX -> SW 
  RoutingTable::Table table = {
    {{0b0000, 0xf}, 0x0, 0b000110},
    {{0b0001, 0xf}, 0x0, 0b000001},
    {{0b0101, 0xf}, 0x0, 0b010000},
    {{0b1000, 0xf}, 0x0, 0b000110},
    {{0b1001, 0xf}, 0x0, 0b000001},
    {{0b1110, 0xf}, 0x0, 0b010000},
    {{0b1100, 0xf}, 0x0, 0b000110},
    {{0b0100, 0xf}, 0x0, 0b110000}
  };

  RoutingTable::Table expected_table = {
    {{0b0100, 0b1111}, 0x0, 0b110000},
    {{0b0001, 0b0111}, 0x0, 0b000001},
    {{0b0000, 0b0011}, 0x0, 0b000110},
    {{0b0100, 0b0100}, 0x0, 0b010000},
  };

  // Minimise the table as far as possible
  OrderedCovering::minimise(table, 0);
  ASSERT_EQ(table, expected_table);
}


TEST(OrderedCoveringTest, test_ordered_covering_terminates_early)
{
  // Test that minimisation doesn't occur if the table is already sufficiently
  // small and terminates early when possible.
  RoutingTable::Table table = {
    {{0b0000, 0xf}, 0x0, 0b000110},
    {{0b0001, 0xf}, 0x0, 0b000001},
    {{0b0101, 0xf}, 0x0, 0b010000},
    {{0b1000, 0xf}, 0x0, 0b000110},
    {{0b1001, 0xf}, 0x0, 0b000001},
    {{0b1110, 0xf}, 0x0, 0b010000},
    {{0b1100, 0xf}, 0x0, 0b000110},
    {{0b0100, 0xf}, 0x0, 0b110000}
  };

  // No minimisation because the table is already sufficiently small
  OrderedCovering::minimise(table, 1024);
  EXPECT_EQ(table.size(), 8);

  // Some minimisation terminating early because the table is small enough
  OrderedCovering::minimise(table, 7);
  EXPECT_LT(table.size(), 8);
  EXPECT_GT(table.size(), 4);
}
