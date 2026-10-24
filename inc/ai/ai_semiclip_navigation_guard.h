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

// Apply the final-node physical occupancy check to a single-node route too.
// The check's caller must use Traversal occupancy: semiclip bypasses it, while
// deliberate teammate boosts and tactical reservations keep their semantics.
constexpr bool isTerminalTraversalOccupancyCandidate(bool routeNotEmpty,
    bool hasNext, int candidateNode, int lastNode, int taskGoal) {
  return routeNotEmpty && candidateNode >= 0 && candidateNode == lastNode
      && (hasNext || taskGoal == lastNode);
}

// A teammate at the first inspected node must not conceal another teammate
// occupying the destination farther down the client list.
constexpr bool hasTeammateWaypointReservation(int node, int currentNode, int previousNode) {
  return node == currentNode || node == previousNode;
}

} // namespace ai
