//
// AiPB - event-only navigation diagnostic records, not training features.
// SPDX-License-Identifier: MIT
//
#pragma once

#include <cstdint>
#include <ai/ai_observation.h>

namespace ai {

constexpr int kNavigationDiagnosticPathNodes = 32;
constexpr float kNavigationDiagnosticSamplePeriod = 2.0f;
constexpr float kNavigationDiagnosticMinProgress = 32.0f;

enum class NavigationEventType : uint8_t {
  TaskChange, RouteRequest, RouteObserved, WaypointChanged, LowDisplacement,
};

enum class NavigationEventReason : uint8_t {
  TaskStarted, TaskCleared, TaskCompleted, GoalChanged, SameGoalRepath,
  RouteObserved, WaypointChanged, LowDisplacement,
};

constexpr const char *navigationEventName(NavigationEventType type) {
  switch (type) {
  case NavigationEventType::TaskChange: return "task_change";
  case NavigationEventType::RouteRequest: return "route_request";
  case NavigationEventType::RouteObserved: return "route_observed";
  case NavigationEventType::WaypointChanged: return "waypoint_changed";
  case NavigationEventType::LowDisplacement: return "low_displacement";
  }
  return "unknown";
}

constexpr const char *navigationEventReasonName(NavigationEventReason reason) {
  switch (reason) {
  case NavigationEventReason::TaskStarted: return "task_started";
  case NavigationEventReason::TaskCleared: return "task_cleared";
  case NavigationEventReason::TaskCompleted: return "task_completed";
  case NavigationEventReason::GoalChanged: return "goal_changed";
  case NavigationEventReason::SameGoalRepath: return "same_goal_repath";
  case NavigationEventReason::RouteObserved: return "path_snapshot_after_request";
  case NavigationEventReason::WaypointChanged: return "waypoint_index_changed";
  case NavigationEventReason::LowDisplacement: return "low_position_displacement";
  }
  return "unknown";
}

struct NavigationEvent {
  NavigationEventType type { NavigationEventType::TaskChange };
  NavigationEventReason reason { NavigationEventReason::TaskStarted };
  float gameTime {}, roundStartTime {};
  uint32_t roundId {};
  uint64_t episodeId {};
  int32_t botId { -1 }, team { -1 }, task { -1 }, aiAction { -1 };
  int32_t previousTask { -1 }, nextTask { -1 };
  int32_t previousNode { -1 }, currentNode { -1 }, goalNode { -1 };
  int32_t routeSource { -1 }, routeDestination { -1 }, pathType { -1 };
  int32_t pathNodeCount {};
  int32_t pathNodes[kNavigationDiagnosticPathNodes] {};
  bool pathTruncated {};
  float estimatedPathDistance {}, physicalDisplacement {}, distanceToGoal {};
  Vec3 position {}, velocity {};
};

// This is an observed lack of displacement, not a diagnosis of WHY.
// Combat, blind, ladders and intentional pauses must be excluded by caller.
constexpr bool shouldReportLowNavigationDisplacement(float elapsedTime,
                                                       float displacement, bool movingToGoal,
                                                       bool inCombatOrBlind) {
  return elapsedTime >= kNavigationDiagnosticSamplePeriod
      && displacement < kNavigationDiagnosticMinProgress
      && movingToGoal && !inCombatOrBlind;
}

} // namespace ai
