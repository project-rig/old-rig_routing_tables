#include <functional>
#include <map>
#include <set>
#include <vector>

#include "routing_table.h"

#pragma once

using RoutingTable::Table;

namespace OrderedCovering
{
/* Alias table type **********************************************************/
typedef std::map<RoutingTable::KeyMask,
                 std::set<RoutingTable::KeyMask>> Aliases;

typedef std::vector<bool> Merge;  // TODO Own flexible bitvector

/*****************************************************************************/

/* minimise ******************************************************************/
void minimise(Table& table, unsigned int target_length);
void minimise(Table& table, unsigned int target_length, Aliases& aliases);
/*****************************************************************************/

/*****************************************************************************/
/* Core of the Ordered Covering algorithm ************************************/

// Get the best merge (greedy) in a routing table
Merge get_best_merge(const Table& table, const Aliases& aliases);

// Get the position in a table where a new entry of given generality should be
// inserted.
Table::const_iterator get_insertion_index(
  const Table& table,
  const unsigned int generality
);
Table::const_iterator get_insertion_index(
  const Table& table,
  const RoutingTable::Entry& entry
);
Table::const_iterator get_insertion_index(
  const Table& table,
  const Merge& merge
);

// Refine a merge by pruning any entries which would cause an entry lower in
// the table to become covered.
int refine_merge_downcheck(
  const Table& table,
  const Aliases& aliases,
  Merge& merge,
  const int min_goodness
);

// Refine a merge by pruning any entries which would be covered existing
// entries higher in the table.
int refine_merge_upcheck(
  const Table& table,
  Merge& merge,
  const int min_goodness
);
/*****************************************************************************/

/*****************************************************************************/
/* Merges ********************************************************************/
// A merge represents a set of entries which may be combined together to create
// a new routing table entry.  Applying a merge will remove from a routing
// table the original routing table entries and lead to the insertion of a new
// entry.

// Generate the entry that would be the result of a merge
RoutingTable::Entry merge_entries(const Table& table,
                                         const Merge& merge);

// Get the number of entries contained within a merge
int merge_goodness(const Merge& merge);

// Apply a merge to a routing table
void merge_apply(Table& table,
                        Aliases& aliases,
                        const Merge& merge);
/*****************************************************************************/

/*****************************************************************************/
/* Get best merge ************************************************************/
Merge get_best_merge(const Table& table, const Aliases& aliases)
{
  // Create holders for the current best merge and its goodness
  auto best_merge = Merge(table.size(), false);
  int best_goodness = 0;

  // Create a bitset to track which routing table entries have been considered
  // as part of a merge.
  auto considered = std::vector<bool>(table.size(), false);

  // For every entry in the table which hasn't already been considered as part
  // of a merge look through the rest of the table to determine with which
  // other entries it could be merged.
  unsigned int index = 0;
  for (auto p_entry = table.begin();
       p_entry < table.end();
       p_entry++, index++)
  {
    auto entry = *p_entry;

    // Skip to the next entry if this entry has already been considered.
    if (considered[index])
    {
      continue;
    }
    considered[index] = true;

    // Look through the rest of the table to see which other entries this entry
    // could be merged.
    auto current_merge = Merge(table.size(), false);
    current_merge[index] = true;
    int current_goodness = 0;

    for (auto p_other = p_entry + 1; p_other < table.end(); p_other++)
    {
      unsigned int other_index = p_other - table.begin();
      auto other = *p_other;

      // If the routes are the same then the entries may be merged.
      if (other.route == entry.route)
      {
        current_merge[other_index] = true;
        considered[other_index] = true;
        current_goodness++;
      }
    }

    // If this merge is better than the current best then work to ensure that
    // it is valid.
    if (current_goodness > best_goodness)
    {
      // Remove entries such that it would not cover any existing entries.
      current_goodness -= refine_merge_downcheck(
        table, aliases, current_merge, best_goodness);

      if (current_goodness > best_goodness)
      {
        // Remove entries which would be covered by any existing entries.
        int removed = refine_merge_upcheck(table, current_merge,
                                           best_goodness);
        current_goodness -= removed;

        // If entries were removed then the down-check needs to be recomputed.
        if (removed && current_goodness > best_goodness)
        {
          current_goodness -= refine_merge_downcheck(
            table, aliases, current_merge, best_goodness);
        }

        // Finally, if this merge is still better than the best known merge we
        // record it as the best known merge.
        if (current_goodness > best_goodness)
        {
          best_goodness = current_goodness;
          best_merge = current_merge;
        }
      }
    }
  }

  return best_merge;
}
/*****************************************************************************/

/*****************************************************************************/
/* Completely empty a merge **************************************************/
// TODO Reimplement as a method of the bitvector type!
void merge_clear(Merge& merge)
{
  for (unsigned int i = 0; i < merge.size(); i++)
  {
    merge[i] = false;
  }
}
/*****************************************************************************/

/*****************************************************************************/
/* Compute the goodness of a merge *******************************************/
// TODO Reimplement as a method of the bitvector type
int merge_goodness(const Merge& merge)
{
  // Just count the number of set elements in the merge
  int count = -1;
  for (auto b : merge)
  {
    if (b)
    {
      count++;
    }
  }
  return count;
}
/*****************************************************************************/

/*****************************************************************************/
/* Get the entry resulting from a merge **************************************/
RoutingTable::Entry merge_entries(const Table& table,
                                         const Merge& merge)
{
  // Iterate through the table, combining the entries.
  uint32_t any_ones = 0x00000000;  // Where there is a one in ANY of the keys
  uint32_t all_ones = 0xffffffff;  // Where there is a one in ALL of the keys
  uint32_t all_sels = 0xffffffff;  // Where there is a one in ALL of the masks
  uint32_t sources  = 0x00000000;  // Union of the source fields

  // Union of the route fields (we rely on the caller to ensure what they are
  // doing is valid!)
  uint32_t routes   = 0x00000000;

  for (unsigned int i = 0; i < table.size(); i++)
  {
    // If this entry is in the merge then include the entry
    if (merge[i])
    {
      // Get the entry
      auto entry = table[i];

      // Include in the values
      any_ones |= entry.keymask.key;
      all_ones &= entry.keymask.key;
      all_sels &= entry.keymask.mask;
      sources  |= entry.source;
      routes   |= entry.route;
    }
  }

  // Compute the new key and mask
  auto any_zeros = ~all_ones;
  auto new_xs = any_ones ^ any_zeros;
  auto mask = all_sels & new_xs;  // Combine existing and new Xs
  auto key = all_ones & mask;

  // Create and return the new entry
  return {{key, mask}, sources, routes};
}
/*****************************************************************************/

/*****************************************************************************/
/* Determine where a new entry should be inserted in a routing table *********/
// For a given generality
Table::const_iterator get_insertion_index(
    const Table& table, const unsigned int generality
)
{
  // Perform a binary search through the table until we find an entry with
  // equivalent generality.
  auto bottom = table.cbegin(), top = table.cend();
  auto pos = bottom + (top - bottom)/2;

  while ((*pos).keymask.count_xs() != generality &&
         bottom < pos && pos < top)
  {
    if ((*pos).keymask.count_xs() < generality)
    {
      bottom = pos;
    }
    else
    {
      top = pos;
    }

    // Update the current position
    pos = bottom + (top - bottom)/2;
  }

  // Iterate through the table until the either the next generality or the end
  // of the table is found.
  while (pos != table.end() && (*pos).keymask.count_xs() <= generality)
  {
    pos++;
  }

  return pos;
}

// For a given entry
Table::const_iterator get_insertion_index(
  const Table& table,
  const RoutingTable::Entry& entry
)
{
  return get_insertion_index(table, entry.keymask.count_xs());
}

// For a given merge
Table::const_iterator get_insertion_index(
  const Table& table,
  const Merge& merge
)
{
  auto new_entry = merge_entries(table, merge);
  return get_insertion_index(table, new_entry);
}
/*****************************************************************************/

/*****************************************************************************/
/* Apply a merge *************************************************************/
void merge_apply(Table& table,
                        Aliases& aliases,
                        const Merge& merge)
{
  // Get the merged entry and where to insert it in the table.
  auto merge_entry = merge_entries(table, merge);
  auto insertion_point = get_insertion_index(table, merge_entry);

  // Keep track of the size of the finished table.
  unsigned int final_size = table.size() + 1;

  // Use two iterators to move through the table, copying elements from one
  // iterator position to the other as required.
  auto insert = table.begin();
  for (auto remove = table.cbegin(); remove < table.cend(); remove++)
  {
    // Insert the new entry if this is the correct point at which to do so.
    if (remove == insertion_point)
    {
      *insert = merge_entry;
      insert++;
    }

    unsigned int i = remove - table.begin();
    if (!merge[i])
    {
      // If this entry is not part of the merge then copy it across to the new
      // table.
      *insert = *remove;
      insert++;
    }
    else
    {
      // Update the aliases table; if the entry we're removing is in the
      // aliases list then copy all entries from its entry to the new entry, if
      // it isn't then add just the keymask from the old entry to the aliases
      // dictionary.
      auto old_km = (*remove).keymask;
      if (aliases.count(old_km))
      {
        // Copy all keymasks from the old entry
        auto old_entries = (*aliases.find(old_km)).second;
        aliases[merge_entry.keymask].insert(old_entries.begin(),
                                            old_entries.end());

        // Remove the old entry
        aliases.erase(old_km);
      }
      else
      {
        // Just add the old key mask to the new entry
        aliases[merge_entry.keymask].insert(old_km);
      }

      // Count this entry as removed.
      final_size--;
    }
  }

  // If inserting beyond the old end of the table then perform the insertion at
  // the new end of the table.
  if (table.end() == insertion_point)
  {
    *insert = merge_entry;
  }

  // Resize the table (this will only ever be a shrink of the table).
  table.resize(final_size);
}
/*****************************************************************************/

/*****************************************************************************/
/* Avoid covering entries with a merge ***************************************/

struct CoverInfo
{
  // If any key-masks lower in the table than the entry resulting from the
  // merge were covered
  bool covers;
  uint32_t set_to_zero;  // Bits which could be set to 0 to avoid the cover
  uint32_t set_to_one;   // Bits which could be set to 1 to avoid the cover

  // NOTE: If both the uint32_t s are zero and the bool is true then it is not
  // possible to avoid the cover.
};

void get_settables(RoutingTable::KeyMask& a,
                          RoutingTable::KeyMask& b,
                          unsigned int& stringency,
                          uint32_t& set_to_zero,
                          uint32_t& set_to_one)
{
  // We can avoid merging by setting to either 0 or 1 bits where the
  // merged entry has an X but the covered entry does not.
  uint32_t settable = a.get_xs() & ~b.get_xs();

  // Compute the stringency of this collision; if it's less than the
  // previous stringency then reset the set_to_x variables; if it's more
  // then disregard and if it's equal then update them.
  unsigned int this_stringency = __builtin_popcount(settable);

  if (this_stringency < stringency)
  {
    stringency  = this_stringency;
    set_to_one  = settable & ~b.key;
    set_to_zero = settable & b.key;
  }
  else if (this_stringency == stringency)
  {
    set_to_one  |= settable & ~b.key;
    set_to_zero |= settable & b.key;
  }
}

struct CoverInfo get_cover_info(
    const Table& table,
    const Aliases& aliases,
    const Merge& merge
)
{
  struct CoverInfo info = {false, 0x0, 0x0};

  // Get the entry which would be generated by the merge
  auto merge_entry = merge_entries(table, merge);
  auto merge_km = merge_entry.keymask;

  unsigned int stringency = 33;  // Number of bits which MAY be set

  // Look through the table to see if there are entries below the point where
  // the merge would be inserted which would be covered by the entry resulting
  // from performing the merge.
  for (auto i = get_insertion_index(table, merge_entry);
       i != table.end(); i++)
  {
    // Get the entry key-mask
    auto entry_km = (*i).keymask;

    if (merge_km.intersect(entry_km))
    {
      // See if the key-mask is in the aliases table
      auto alias_list = aliases.find(entry_km);
      if (alias_list == aliases.end())
      {
        // As there are no aliases we need to avoid colliding with the key-mask
        // from the entry.
        info.covers = true;
        get_settables(merge_km, entry_km, stringency,
                      info.set_to_zero, info.set_to_one);
      }
      else
      {
        // As this key-mask is in the aliases table then check that none of the
        // aliased key-masks intersect with the key-mask resulting from the
        // merge.
        for (auto alias : (*alias_list).second)
        {
          if (alias.intersect(merge_km))
          {
            info.covers = true;
            get_settables(merge_km, alias, stringency,
                          info.set_to_zero, info.set_to_one);
          }
        }
      }
    }
  }

  return info;
}

template <typename F>
std::vector<unsigned int> find_removes(
    const Table& table,
    const Merge& merge,
    F f
)
{
  // If this is a bit we could set to VAL then find all entries which
  // contain an X or NOT VAL in the specified bit.
  auto to_remove = std::vector<unsigned int>();
  for (unsigned int j = 0; j < table.size(); j++)
  {
    if (merge[j] && f(table[j].keymask))
    {
      to_remove.push_back(j);
    }
  }

  return to_remove;
}

// Prune a merge to ensure that no entries below the merge insertion point will
// be covered by the new entry created by the merge.
// Return the number of pruned entries.
int refine_merge_downcheck(
    const Table& table,
    const Aliases& aliases,
    Merge& merge,
    const int min_goodness
)
{
  int removed = 0;                       // Count number of removed entries
  int goodness = merge_goodness(merge);  // Original merge goodness

  while (goodness > min_goodness)
  {
    // Determine if any covering occurs
    auto info = get_cover_info(table, aliases, merge);
    if (!info.covers)
    {
      // If there was no covering then we can break out of this loop
      break;
    }

    if (!info.set_to_one && !info.set_to_zero)
    {
      // We cannot do anything to avoid covering the lower entries, so abandon
      // the merge entirely.
      merge_clear(merge);
      removed += goodness + 1;
      goodness = 0;
    }
    else
    {
      // Find the smallest number of entries we could remove to set one of the
      // bits in the merged entry such that it would avoid covering a lower
      // entry.
      auto best_removes = std::vector<unsigned int>();
      for (uint32_t bit = (1 << 31);
           bit > 0 && best_removes.size() != 1;
           bit >>= 1)
      {
        // If this bit may be set to zero then look for any entries to remove
        // to achieve this.
        if (bit & info.set_to_zero)
        {
          auto new_removes = find_removes(
            table, merge,
            [bit] (auto km) { return (~km.mask & bit) || (km.key & bit); }
          );

          if (((best_removes.size() == 0) ||
               (new_removes.size() < best_removes.size())) &&
              (new_removes.size() != 0))
          {
            best_removes = new_removes;
          }
        }

        // If this bit may be set to one then look for any entries to remove to
        // achieve this.
        if (bit & info.set_to_one)
        {
          auto new_removes = find_removes(
            table, merge,
            [bit] (auto km) { return ~km.key & bit; }
          );

          if (((best_removes.size() == 0) ||
               (new_removes.size() < best_removes.size())) &&
              (new_removes.size() != 0))
          {
            best_removes = new_removes;
          }
        }
      }

      // Remove all the entries found in best_removes
      for (auto i : best_removes)
      {
        merge[i] = false;
      }
      removed += best_removes.size();
      goodness -= best_removes.size();

      if (goodness == 0)
      {
        merge_clear(merge);
        removed++;
      }
    }
  }

  return removed;
}
/*****************************************************************************/

/*****************************************************************************/
/* Avoid being covered by merging ********************************************/
// Prune a merge to ensure that no entries contained within the merge will be
// covered by existing entries located above the insertion point of the merge.
// Return the number of pruned entries.
int refine_merge_upcheck(
    const Table& table,
    Merge& merge,
    const int min_goodness
)
{
  int
    removed = 0,                       // Count number of removed entries
    goodness = merge_goodness(merge);  // Original merge goodness

  // Get the insertion position of the merge in the table.
  auto insertion_point = get_insertion_index(table, merge);

  // For each entry in the merge (in decreasing order of generality) check to
  // see if there are any entries above the merge position which would cause
  // the entry to become covered if the merge were to go ahead.
  // Abort this process once the goodness of the merge is no greater than the
  // specified minimum goodness.
  for (auto entry = table.cend() - 1;
       entry >= table.cbegin() && goodness > min_goodness;
       entry--)
  {
    unsigned int index = entry - table.begin();  // Get the index of the entry

    // Ignore this entry if it's not in the merge
    if (merge[index])
    {
      // Get the key-mask of this entry
      auto entry_km = (*entry).keymask;

      // Check to see if any entry between the current entry position and the
      // position where the merge will be inserted would partially or wholly
      // cover the entry. If it would then remove the entry from the merge.
      for (auto other_entry = entry + 1; other_entry < insertion_point;
           other_entry++)
      {
        auto other_km = (*other_entry).keymask;

        if (entry_km.intersect(other_km))
        {
          // This entry would become covered if the merge were to go ahead so
          // remove it from the merge.
          removed++;
          goodness--;
          merge[index] = false;

          // Recompute where the entry resulting from the merge would be
          // inserted in the table.
          insertion_point = get_insertion_index(table, merge);
          break;
        }
      }
    }
  }

  // If the merge is now no better than the specified minimum goodness empty
  // the merge and return.
  if (goodness <= min_goodness)
  {
    // Empty the merge entirely
    merge_clear(merge);
    removed += goodness;
    goodness = 0;
  }

  return removed;
}
/*****************************************************************************/

/*****************************************************************************/
/* minimise Implementation ***************************************************/
void minimise(Table& table, unsigned int target_length)
{
  // Create empty aliases table and call minimise with that
  auto aliases = Aliases();
  minimise(table, target_length, aliases);
}

void minimise(Table& table,
                     unsigned int target_length,
                     Aliases& aliases)
{
  // While the table is still longer than the target length continue to get
  // and apply merges.
  while (table.size() > target_length)
  {
    // Get the best candidate merge; if the merge is empty then the table
    // cannot be further minimised and we should exit the loop.
    Merge merge = OrderedCovering::get_best_merge(table, aliases);
    if (OrderedCovering::merge_goodness(merge) < 1)
    {
      break;
    }

    // Otherwise apply the merge to the routing table. This will modify the
    // table and the aliases dictionary.
    OrderedCovering::merge_apply(table, aliases, merge);
  }
}
/*****************************************************************************/
}
