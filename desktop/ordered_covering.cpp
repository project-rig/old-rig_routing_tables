#include <ctime>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include "ordered_covering.h"
#include "default_routes.h"


int main(int argc, char* argv[])
{
  // We expect two arguments; an input routing table file and an output routing
  // table file. An optional 4th argument is the target length of the routing
  // table.
  if (argc < 3)
  {
    fprintf(stderr, "Usage: rig-ordered-covering in_file out_file [target length]\n");
  }

  // Prepare the input and output streams; for each routing table in the input
  // file we minimise it and then write it out to file immediately.
  std::ifstream in  (argv[1], std::ios::in | std::ios::binary);
  std::ofstream out (argv[2], std::ios::out | std::ios::binary);

  // Get the target length
  unsigned int target_length = 0;
  if (argc == 4)
  {
    target_length = atoi(argv[3]);
  }

  while (!in.eof())
  {
    // The first two bytes are the co-ordinates of the routing table.
    unsigned char x, y;
    in.read((char *) &x, 1);
    in.read((char *) &y, 1);
    fprintf(stdout, "(%3u, %3u)\t", x, y);

    // The next short is the original length of the table
    unsigned short length;
    in.read((char *) &length, 2);
    fprintf(stdout, "%5u\t", length);

    // Create the table and iterate through adding entries
    auto table = RoutingTable::Table(length);
    in.read((char *) &(table[0]), sizeof(RoutingTable::Entry) * length);

    // Minimise the table
    auto t = clock();
    OrderedCovering::minimise(table, target_length);
    float time = ((float) (clock() - t)) / CLOCKS_PER_SEC;
    fprintf(stdout, "%5u\t%f s\n", (unsigned int) table.size(), time);

    // Write the table out again
    // (BYTE: x, BYTE: y, SHORT: length)
    auto new_length = table.size();
    out.write((char *) &x, 1);
    out.write((char *) &y, 1);
    out.write((char *) &new_length, 2);

    // Dump out the table
    out.write((char *) &table[0], sizeof(RoutingTable::Entry) * new_length);
  }
}
