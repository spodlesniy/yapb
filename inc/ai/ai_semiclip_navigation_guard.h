//
// AiPB - teammate semiclip navigation guard.
// SPDX-License-Identifier: MIT
//
#pragma once

#include <cstdint>

namespace ai {

// Tactical reservations and physical traversal are different concepts.
// Semiclip removes teammate body blocking on ordinary waypoint traversal,
// but it should not make teammates' camping/cover positions unreserved.
enum class NodeOccupancyPurpose : uint8_t {
  Tactical,
  Traversal,
};

constexpr bool shouldIgnoreTeammateOccupancy(bool teamSemiclipEnabled,
                                            NodeOccupancyPurpose purpose,
                                            bool deliberateTeammateBoost) {
  return teamSemiclipEnabled
      && purpose == NodeOccupancyPurpose::Traversal
      && !deliberateTeammateBoost;
}

// A teammate at the first inspected node must not conceal another teammate
// occupying the destination farther down the client list.
constexpr bool hasTeammateWaypointReservation(int node, int currentNode, int previousNode) {
  return node == currentNode || node == previousNode;
}

} // namespace ai
