#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

namespace ai::navvis {

constexpr bool validPlayerIndex (int index, int capacity) {
   return index >= 0 && index < capacity;
}

template <typename PointType> struct NearbyPoint {
   const PointType *point {};
   float distanceSquared {};
};

// Keep the closest bounded set, regardless of the export file's ID ordering.
template <typename PointType, std::size_t Capacity>
std::size_t selectNearby (
   const std::vector <PointType> &points,
   unsigned sourceMask,
   float x, float y, float z, float radius,
   NearbyPoint <PointType> (&selected)[Capacity]) {

   static_assert (Capacity > 0, "Nearby selection needs nonzero capacity");

   const float maxDistanceSquared = radius * radius;
   const auto closer = [] (const NearbyPoint <PointType> &a, const NearbyPoint <PointType> &b) {
      return a.distanceSquared < b.distanceSquared ||
         (a.distanceSquared == b.distanceSquared && a.point->id < b.point->id);
   };
   std::size_t count = 0;

   for (const auto &point : points) {
      if ((sourceMask & (1u << point.source)) == 0) {
         continue;
      }
      const float dx = point.x - x;
      const float dy = point.y - y;
      const float dz = point.z - z;
      const float distanceSquared = dx * dx + dy * dy + dz * dz;

      if (distanceSquared > maxDistanceSquared) {
         continue;
      }
      const NearbyPoint <PointType> entry { &point, distanceSquared };

      if (count < Capacity) {
         selected[count++] = entry;
         std::push_heap (selected, selected + count, closer);
      }
      else if (closer (entry, selected[0])) {
         std::pop_heap (selected, selected + count, closer);
         selected[count - 1] = entry;
         std::push_heap (selected, selected + count, closer);
      }
   }

   std::sort (selected, selected + count, closer);
   return count;
}

} // namespace ai::navvis
