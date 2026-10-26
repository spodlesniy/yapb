#include <aipb/map/navigation/connectivity.h>

#include "brush_policy.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <numeric>
#include <utility>
#include <vector>

namespace aipb::map::navigation {
namespace {

struct Groups {
  std::vector<std::size_t> parent;

  explicit Groups(std::size_t n) : parent(n) {
    std::iota(parent.begin(), parent.end(), std::size_t { 0 });
  }

  std::size_t root(std::size_t i) {
    while (parent[i] != i) {
      i = parent[i];
    }

    return i;
  }

  void join(std::size_t a, std::size_t b) {
    parent[root(b)] = root(a);
  }
};

Vec3 playerOrigin(const Candidate &point, double halfHeight, double lift) {
  const auto &n = point.floorNormal;
  const double slope = 16.0 * (std::abs(n.x) + std::abs(n.y)) / n.z;

  return { point.position.x, point.position.y, point.position.z + halfHeight + slope + lift };
}

Vec3 trajectoryPoint(
  Vec3 start,
  Vec3 end,
  double t,
  double flightSeconds,
  const JumpPhysics &physics
) {
  const double time = flightSeconds * t;

  return {
    start.x + (end.x - start.x) * t,
    start.y + (end.y - start.y) * t,
    start.z + jumpDisplacement(physics, time)
  };
}

bool clear(
  const collision::CollisionQueries &queries,
  ObjectId world,
  const std::vector<detail::Brush> &brushes,
  std::size_t hull,
  Vec3 from,
  Vec3 to
) {
  const auto hit = queries.traceSegment(world, hull, from, to);

  if (hit.startSolid || hit.impactPlane) {
    return false;
  }

  for (const auto &brush : brushes) {
    const auto shift = brush.entity->origin;
    const Vec3 a { from.x - shift.x, from.y - shift.y, from.z - shift.z };
    const Vec3 b { to.x - shift.x, to.y - shift.y, to.z - shift.z };
    const auto brushHit = queries.traceSegment(brush.model->id, hull, a, b);

    if (brushHit.startSolid || brushHit.impactPlane) {
      return false;
    }
  }

  return true;
}

bool flight(
  const collision::CollisionQueries &queries,
  ObjectId world,
  const std::vector<detail::Brush> &brushes,
  Vec3 start,
  Vec3 target,
  bool crouched,
  double flightSeconds,
  const JumpPhysics &physics
) {
  constexpr std::size_t segments = 24;
  const double difference = physics.standingHalfHeight - physics.crouchingHalfHeight;

  for (std::size_t step = 0; step < segments; ++step) {
    const double t0 = static_cast<double>(step) / static_cast<double>(segments);
    const double t1 = static_cast<double>(step + 1) / static_cast<double>(segments);
    const std::size_t hull = crouched && step >= 4 && step < segments - 4 ? 3u : 1u;
    const double offset = hull == 3 ? difference : 0.0;
    const auto a = trajectoryPoint(start, target, t0, flightSeconds, physics);
    const auto b = trajectoryPoint(start, target, t1, flightSeconds, physics);
    const Vec3 p { a.x, a.y, a.z - offset };
    const Vec3 q { b.x, b.y, b.z - offset };

    if (!clear(queries, world, brushes, hull, p, q)) {
      return false;
    }
  }

  return true;
}

void recount(
  const CandidateReport &candidates,
  ConnectivityReport &report
) {
  Groups groups(candidates.candidates.size());
  std::map<std::size_t, std::size_t> ids;
  std::vector<std::size_t> degree(candidates.candidates.size(), 0);

  for (std::size_t i = 0; i < candidates.candidates.size(); ++i) {
    ids.emplace(candidates.candidates[i].id, i);
  }

  for (const auto &edge : report.edges) {
    const auto a = ids.at(edge.from);
    const auto b = ids.at(edge.to);
    groups.join(a, b);
    ++degree[a];
    ++degree[b];
  }
  std::map<std::size_t, std::size_t> sizes;
  std::map<std::size_t, bool> conditional;

  for (std::size_t i = 0; i < candidates.candidates.size(); ++i) {
    const auto root = groups.root(i);
    ++sizes[root];
    conditional[root] |= candidates.candidates[i].conditionalSupport;
    report.isolatedCandidates += degree[i] == 0;
  }
  report.weakComponents = sizes.size();
  report.largestComponent = 0;
  report.conditionalIslands = 0;

  for (const auto &[root, size] : sizes) {
    report.largestComponent = std::max(report.largestComponent, size);
    report.conditionalIslands += conditional[root];
  }
}

} // namespace

void appendJumpTransitions(
  const MapModel &map,
  const CandidateReport &candidates,
  ConnectivityReport &report
) {
  const auto &config = report.config;
  const auto brushes = detail::collectBrushes(map);
  const auto world = map.partition->models.front().id;
  collision::CollisionQueries queries(map);
  using Cell = std::pair<std::int64_t, std::int64_t>;
  std::map<Cell, std::vector<std::size_t>> cells;
  const double width = config.maximumJumpDistance;

  for (std::size_t i = 0; i < candidates.candidates.size(); ++i) {
    const auto &p = candidates.candidates[i].position;
    cells[{ static_cast<std::int64_t>(std::floor(p.x / width)),
            static_cast<std::int64_t>(std::floor(p.y / width)) }].push_back(i);
  }

  for (const auto &from : candidates.candidates) {
    if (!from.standingFree || report.jumpTrials >= config.maximumJumpChecks ||
        report.edges.size() >= config.maximumEdges) {
      continue;
    }
    std::vector<std::pair<double, std::size_t>> neighbors;

    const auto x = static_cast<std::int64_t>(std::floor(from.position.x / width));
    const auto y = static_cast<std::int64_t>(std::floor(from.position.y / width));

    for (std::int64_t dx = -1; dx <= 1; ++dx) {
      for (std::int64_t dy = -1; dy <= 1; ++dy) {
        const auto cell = cells.find({ x + dx, y + dy });

        if (cell == cells.end()) {
          continue;
        }

        for (const auto j : cell->second) {
          const auto &to = candidates.candidates[j];

          if (to.id == from.id || !to.standingFree) {
            continue;
          }
          const double rise = to.position.z - from.position.z;
          const double xy = std::hypot(to.position.x - from.position.x, to.position.y - from.position.y);

          if (xy > config.minimumPlanarDistance && xy <= config.maximumJumpDistance &&
              rise > config.maximumStepUp && rise <= config.maximumCrouchJumpRise) {
            neighbors.emplace_back(xy, j);
          }
        }
      }
    }
    std::sort(neighbors.begin(), neighbors.end(), [&](const auto &a, const auto &b) {
      return a.first == b.first ? candidates.candidates[a.second].id <
             candidates.candidates[b.second].id : a.first < b.first;
    });
    std::size_t trials = 0;
    std::size_t accepted = 0;

    for (const auto &[xy, index] : neighbors) {
      if (trials >= config.maximumJumpNeighborTrials ||
          accepted >= config.maximumJumpEdgesPerCandidate ||
          report.jumpTrials >= config.maximumJumpChecks ||
          report.edges.size() >= config.maximumEdges) {
        break;
      }
      const auto &to = candidates.candidates[index];
      ++trials;
      ++report.jumpTrials;
      const auto start = playerOrigin(from, 36.0, config.clearanceLift);
      const auto target = playerOrigin(to, 36.0, config.clearanceLift);
      const double rise = to.position.z - from.position.z;
      const auto &physics = config.jumpPhysics;
      const bool simple = rise <= config.maximumJumpRise &&
                          rise <= theoreticalJumpRise(physics, false);
      const bool crouchedAllowed = rise <= config.maximumCrouchJumpRise &&
                                   rise <= theoreticalJumpRise(physics, true);
      const auto ordinaryTime = simple ? descendingJumpTime(physics, rise) : std::nullopt;
      const auto crouchTime = crouchedAllowed ?
                               descendingJumpTime(physics, rise) :
                               std::nullopt;
      const bool ordinarySpeed = ordinaryTime && xy / *ordinaryTime <= physics.maximumHorizontalSpeed;
      const bool crouchSpeed = crouchTime && xy / *crouchTime <= physics.maximumHorizontalSpeed;

      if (!ordinarySpeed && !crouchSpeed) {
        ++report.rejectedJumpPhysics;
        continue;
      }
      const bool ordinaryPass = ordinarySpeed &&
                                flight(queries, world, brushes, start, target, false, *ordinaryTime, physics);
      const bool crouchPass = !ordinaryPass && crouchSpeed &&
                              flight(queries, world, brushes, start, target, true, *crouchTime, physics);

      if (!ordinaryPass && !crouchPass) {
        ++report.rejectedJumpCollision;
        continue;
      }
      const auto touchdown = playerOrigin(to, 36.0, config.clearanceLift);
      const Vec3 floorProbe { touchdown.x, touchdown.y, touchdown.z - 36.0 - config.groundTolerance - 1.0 };
      bool landed = false;
      const auto worldHit = queries.traceSegment(world, 1, touchdown, floorProbe);
      landed = !worldHit.startSolid && worldHit.impactPlane && worldHit.impactPlane->normal.z >= 0.7;

      for (const auto &brush : brushes) {
        if (landed || !detail::suppliesSupport(brush.role)) {
          continue;
        }
        const auto origin = brush.entity->origin;
        const Vec3 a { touchdown.x - origin.x, touchdown.y - origin.y, touchdown.z - origin.z };
        const Vec3 b { floorProbe.x - origin.x, floorProbe.y - origin.y, floorProbe.z - origin.z };
        const auto hit = queries.traceSegment(brush.model->id, 1, a, b);
        landed = !hit.startSolid && hit.impactPlane && hit.impactPlane->normal.z >= 0.7;
      }

      if (!landed) {
        ++report.rejectedJumpLanding;
        continue;
      }
      const bool conditional = from.conditionalSupport || to.conditionalSupport;
      const auto traversal = ordinaryPass ? TraversalKind::Jump : TraversalKind::CrouchJump;
      report.edges.push_back({
        .id = report.edges.size() + 1,
        .from = from.id,
        .to = to.id,
        .planarDistance = xy,
        .heightChange = rise,
        .stance = collision::PlayerStance::Standing,
        .traversal = traversal,
        .conditionalSupport = conditional,
        .estimatedFlightSeconds = ordinaryPass ? *ordinaryTime : *crouchTime,
        .estimatedHorizontalSpeed = xy / (ordinaryPass ? *ordinaryTime : *crouchTime)
      });
      report.jumpEdges += ordinaryPass;
      report.crouchJumpEdges += crouchPass;
      report.conditionalEdges += conditional;
      ++accepted;
    }
  }
  report.isolatedCandidates = 0;
  recount(candidates, report);
}

} // namespace aipb::map::navigation
