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

// Throttle diagnostics only; gameplay pathfinding remains unchanged.
constexpr float kNavigationSameGoalRouteInterval = 12.0f;
constexpr float kNavigationChangedGoalRouteInterval = 4.0f;
constexpr float kNavigationWaypointEventInterval = 6.0f;
constexpr float kNavigationLowDisplacementEventInterval = 12.0f;

// Shared round identity; zero means no round start was observed yet.
constexpr uint32_t nextNavigationRoundId(uint32_t previous) {
  const uint32_t next = previous + 1;
  return next ? next : 1;
}

class NavigationDiagnosticGate final {
private:
  float m_lastRouteTime {}, m_lastWaypointTime {}, m_lastLowDisplacementTime {};
  int m_lastRouteDestination { -1 }, m_lastRoutePathType { -1 };
  bool m_hasRoute {}, m_hasWaypoint {}, m_hasLowDisplacement {};

public:
  void reset() { *this = NavigationDiagnosticGate {}; }
  bool hasRoute() const { return m_hasRoute; }
  int lastRouteDestination() const { return m_lastRouteDestination; }

  bool acceptRoute(float now, int destination, int pathType) {
    if (m_hasRoute && now >= m_lastRouteTime) {
      const bool changed = destination != m_lastRouteDestination
          || pathType != m_lastRoutePathType;
      const float interval = changed
          ? kNavigationChangedGoalRouteInterval : kNavigationSameGoalRouteInterval;
      if (now - m_lastRouteTime < interval) return false;
    }
    m_hasRoute = true;
    m_lastRouteTime = now;
    m_lastRouteDestination = destination;
    m_lastRoutePathType = pathType;
    return true;
  }

  bool acceptWaypoint(float now) {
    if (m_hasWaypoint && now >= m_lastWaypointTime
        && now - m_lastWaypointTime < kNavigationWaypointEventInterval) return false;
    m_hasWaypoint = true;
    m_lastWaypointTime = now;
    return true;
  }

  bool acceptLowDisplacement(float now) {
    if (m_hasLowDisplacement && now >= m_lastLowDisplacementTime
        && now - m_lastLowDisplacementTime < kNavigationLowDisplacementEventInterval) return false;
    m_hasLowDisplacement = true;
    m_lastLowDisplacementTime = now;
    return true;
  }
};

enum class NavigationEventType : uint8_t {
  TaskChange, RouteRequest, RouteObserved, WaypointChanged, LowDisplacement, DroppedBombGuard,
};

enum class NavigationEventReason : uint8_t {
  TaskStarted, TaskCleared, TaskCompleted, GoalChanged, SameGoalRepath,
  RouteObserved, WaypointChanged, LowDisplacement,
  DroppedBombAssigned, DroppedBombSupport, DroppedBombNoCover, DroppedBombReleased,
};

constexpr const char *navigationEventName(NavigationEventType type) {
  switch (type) {
  case NavigationEventType::TaskChange: return "task_change";
  case NavigationEventType::RouteRequest: return "route_request";
  case NavigationEventType::RouteObserved: return "route_observed";
  case NavigationEventType::WaypointChanged: return "waypoint_changed";
  case NavigationEventType::LowDisplacement: return "low_displacement";
  case NavigationEventType::DroppedBombGuard: return "dropped_bomb_guard";
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
  case NavigationEventReason::DroppedBombAssigned: return "primary_assigned";
  case NavigationEventReason::DroppedBombSupport: return "support_assigned";
  case NavigationEventReason::DroppedBombNoCover: return "no_safe_cover";
  case NavigationEventReason::DroppedBombReleased: return "guard_released";
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
  // D186: explain objective cover choice without adding a training transition.
  int32_t guardExposure { -1 };
  float guardRouteDistance { -1.0f }, guardNearestAllyDistance { -1.0f };
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
