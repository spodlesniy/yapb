#pragma once

#include <string>
#include <vector>

namespace ai::navvis {

struct Point {
   unsigned id {};
   float x {}, y {}, z {};
   int source {};
   bool standing {}, crouching {}, isolated {};
};

struct Edge {
   unsigned from {}, to {};
   int type {};
};

struct Data {
   std::string map;
   std::string sha256;
   std::vector <Point> points;
   std::vector <Edge> edges;
};

// Read-only snapshot parser shared by the game DLL and independent unit tests.
bool load (const char *filename, const char *expectedMap, Data &out);

} // namespace ai::navvis
