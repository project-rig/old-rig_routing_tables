#include <stdint.h>
#include <vector>

#pragma once

namespace RoutingTable
{

/*****************************************************************************/
/* Key-Mask ******************************************************************/
struct KeyMask
{
  uint32_t key;
  uint32_t mask;

  // True if two keymasks would match any of the same keys
  bool intersect(const KeyMask& b) const
  {
    return (this->key & b.mask) == (b.key & this->mask);
  }

  // Apply an ordering to key-masks for use with maps
  bool operator<(const KeyMask& b) const
  {

      uint64_t i_a = (((uint64_t) this->key) << 32) | ((uint64_t) this->mask);
      uint64_t i_b = (((uint64_t) b.key) << 32) | ((uint64_t) b.mask);

      return i_a < i_b;
  }

  bool operator==(const KeyMask& b) const
  {
    return (this->key == b.key && this->mask == b.mask);
  }

  // Get a mask indicating the presence of Xs in a key-mask
  uint32_t get_xs() const
  {
    return ~this->mask & ~this->key;
  }

  // Count the number of Xs in a key-mask pair
  unsigned int count_xs() const
  {
    const uint32_t xs = this->get_xs();
    return __builtin_popcount(xs);
  }
};
/*****************************************************************************/

/*****************************************************************************/
/* Routing Table Entry *******************************************************/
struct Entry
{
  KeyMask keymask;  // Key and mask for the entry
  uint32_t source;  // Routes by which packets may arrive at the router
  uint32_t route;   // Routes by which matching packets will be sent

  bool operator ==(const Entry& b) const
  {
    return (this->source == b.source &&
            this->route == b.route &&
            this->keymask == b.keymask);
  }
};

typedef std::vector<Entry> Table;  // Routing tables are just vectors
/*****************************************************************************/

}
