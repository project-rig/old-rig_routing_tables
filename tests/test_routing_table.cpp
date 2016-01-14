#include <gtest/gtest.h>
#include "routing_table.h"


class KeyMaskTest : public ::testing::Test
{
};


TEST(KeyMaskTest, test_get_xs)
{
  RoutingTable::KeyMask km;

  // No Xs
  km = {0x0, 0xffffffff};
  EXPECT_EQ(km.get_xs(), 0x00000000);

  // All Xs
  km.mask = 0x0;
  EXPECT_EQ(km.get_xs(), 0xffffffff);

  // Some Xs
  km.mask = 0xffff0000;
  EXPECT_EQ(km.get_xs(), 0x0000ffff);

  km.mask = 0x0000ffff;
  EXPECT_EQ(km.get_xs(), 0xffff0000);
}


TEST(KeyMaskTest, test_count_xs)
{
  RoutingTable::KeyMask km;

  // No Xs
  km = {0x0, 0xffffffff};
  EXPECT_EQ(km.count_xs(), 0);

  // All Xs
  km.mask = 0x0;
  EXPECT_EQ(km.count_xs(), 32);

  // X in LSB
  km.mask = 0xfffffffe;
  EXPECT_EQ(km.count_xs(), 1);

  // X in MSB
  km.mask = 0x7fffffff;
  EXPECT_EQ(km.count_xs(), 1);
}


TEST(KeyMaskTest, test_lt)
{
  RoutingTable::KeyMask a = {0x00000000, 0x00000000};
  RoutingTable::KeyMask b = {0x00000000, 0xffffffff};
  EXPECT_TRUE(a < b);
  EXPECT_FALSE(b < a);  // Trivial

  a = {0xffffffff, 0xfffffffe};
  b = {0xffffffff, 0xffffffff};
  EXPECT_TRUE(a < b);
  EXPECT_FALSE(b < a);  // Trivial
}
