#include <stdio.h>
#include "routing_table.h"

#ifndef __DEFAULT_ROUTES_H__

using RoutingTable::Table;


namespace DefaultRoutes
{

// Determine if an entry may be replaced by default routing.
bool defaultable(const Table& table, const Table::iterator p_entry)
{
  // An entry may be replaced by default routing iff. packets arrive at the
  // router through 1 link and exit by the opposing link (they go straight
  // through) AND there are no other entries lower in the table which would
  // match any of the same packets.
  auto entry = *p_entry;

  // If either the source or the route contain any cores the entry may not
  // be replaced by a default route.
  if ((entry.source & 0xffffffc0) || (entry.route & 0xffffffc0))
  {
    return false;
  }

  // If the out-route is not opposite to the in-route then the entry cannot be
  // replaced by a default route.
  auto opp_source = ((entry.source << 3) & 0x38) | ((entry.source >> 3) & 0x7);
  if (opp_source != entry.route)
  {
    return false;
  }

  // If there is more than one way that packets can arrive or leave the router
  // according to this route then the entry cannot be replaced by a default
  // route.
  unsigned int count_in = 0;
  for (unsigned int i = 0; i < 6; i++)
  {
    if (entry.source & (1 << i))
    {
      count_in++;
    }
  }

  unsigned int count_out = 0;
  for (unsigned int i = 0; i < 6; i++)
  {
    if (entry.route & (1 << i))
    {
      count_out++;
    }
  }

  if (count_in != 1 || count_out != 1)
  {
    return false;
  }

  // If the entry intersects at all with any entry lower in the table then it
  // cannot be replaced by a default route.
  for (auto other_entry = p_entry + 1;
       other_entry < table.end();
       other_entry++)
  {
    auto other_km = (*other_entry).keymask;
    if (other_km.intersect(entry.keymask))
    {
      return false;
    }
  }

  return true;
}

/*****************************************************************************/
// Minimise a table by removing entries which could be handled by default
// routing.
void minimise(Table& table)
{
  auto final_size = table.size();  // Length of finished table

  // Iterate through the table removing any entries which could be managed by
  // default routing.
  auto insert = table.begin();
  for (auto remove = table.begin(); remove < table.end(); remove++)
  {
    if (!defaultable(table, remove))
    {
      // If this entry could not be replaced by a default entry then insert it
      // into the table.
      *insert = *remove;
      insert++;
    }
    else
    {
      // Otherwise don't include this entry and instead decrement the final
      // table size.
      final_size--;
    }
  }

  // Shrink the table to account for removed elements
  table.resize(final_size);
}
/*****************************************************************************/
}


#define __DEFAULT_ROUTES_H__
#endif  // __DEFAULT_ROUTES_H__
