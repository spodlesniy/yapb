#include <aipb/map/navigation/connectivity.h>
#include <aipb/map/hash.h>

#include "json.h"
#include "brush_policy.h"
#include "walk_validation.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>

namespace aipb::map::navigation {
namespace {
using io::Json;

using detail::Brush;

Json number(std::size_t value) {
  return static_cast<double>(value);
}

bool finite(double n) {
  return std::isfinite(n);
}

struct UnionFind {
  std::vector<std::size_t> parent;

  explicit UnionFind(std::size_t count) : parent(count) {
    std::iota(parent.begin(), parent.end(), std::size_t { 0 });
  }

  std::size_t root(std::size_t x) {
    while (parent[x] != x) {
      x = parent[x];
    }

    return x;
  }

  void merge(std::size_t a, std::size_t b) {
    a = root(a);
    b = root(b);
    parent[b] = a;
  }
};

} // namespace

ConnectivityReport buildConnectivity(
  const MapModel &map,
  const CandidateReport &candidates,
  ConnectivityConfig config
) {
  if (!finite(config.maximumPlanarDistance) || config.maximumPlanarDistance <= 0 ||
      !finite(config.minimumPlanarDistance) || config.minimumPlanarDistance < 0 ||
      config.minimumPlanarDistance >= config.maximumPlanarDistance ||
      !finite(config.maximumStepUp) || config.maximumStepUp < 0 ||
      !finite(config.maximumStepDown) || config.maximumStepDown < 0 ||
      !finite(config.groundTolerance) || config.groundTolerance <= 0 ||
      !finite(config.clearanceLift) || config.clearanceLift <= 0 ||
      config.maximumNeighborTrials == 0 || config.maximumOutgoingEdges == 0 ||
      config.maximumDirectedChecks == 0 || config.maximumEdges == 0 ||
      !finite(config.maximumDropHeight) || config.maximumDropHeight <= config.maximumStepDown ||
      !finite(config.maximumDropDistance) || config.maximumDropDistance <= config.minimumPlanarDistance ||
      config.maximumDropNeighborTrials == 0 || config.maximumDropEdgesPerCandidate == 0 ||
      config.maximumDropChecks == 0 ||
      !finite(config.maximumApproachDistance) || config.maximumApproachDistance <= 0 ||
      !finite(config.maximumJumpDistance) || config.maximumJumpDistance <= config.minimumPlanarDistance ||
      !finite(config.maximumJumpRise) || config.maximumJumpRise <= config.maximumStepUp ||
      !finite(config.maximumCrouchJumpRise) || config.maximumCrouchJumpRise <= config.maximumJumpRise ||
      config.maximumJumpNeighborTrials == 0 || config.maximumJumpEdgesPerCandidate == 0 ||
      config.maximumJumpChecks == 0) {
    throw std::invalid_argument("invalid connectivity configuration");
  }

  if (!map.partition || map.partition->models.empty() ||
      candidates.mapName != map.name || candidates.sourceSha256 != map.sourceSha256) {
    throw std::invalid_argument("connectivity map identity or collision partition mismatch");
  }
  (void)jumpImpulse(config.jumpPhysics);
  ConnectivityReport report;
  report.mapName = candidates.mapName;
  report.sourceSha256 = candidates.sourceSha256;
  report.artifactSha256 = candidates.artifactSha256;
  report.candidatesSha256 = sha256Hex(candidatesJson(candidates));
  report.config = config;
  report.candidateCount = candidates.candidates.size();
  collision::CollisionQueries queries(map);
  const auto world = map.partition->models.front().id;
  const auto brushes = detail::collectBrushes(map);
  const auto side = config.maximumPlanarDistance;
  using Cell = std::pair<std::int64_t, std::int64_t>;
  std::map<Cell, std::vector<std::size_t>> cells;
  std::map<std::size_t, std::size_t> indices;

  for (std::size_t i = 0; i < candidates.candidates.size(); ++i) {
    const auto &c = candidates.candidates[i];

    if (c.id == 0 || !indices.emplace(c.id, i).second ||
        !finite(c.position.x) || !finite(c.position.y) || !finite(c.position.z) ||
        !finite(c.floorNormal.z) || c.floorNormal.z < 0.7 ||
        (!c.standingFree && !c.crouchingFree)) {
      throw std::invalid_argument("invalid navigation candidate for connectivity");
    }
    const auto x = std::floor(c.position.x / side);
    const auto y = std::floor(c.position.y / side);

    if (x < -1000000 || x > 1000000 || y < -1000000 || y > 1000000) {
      throw std::invalid_argument("candidate grid coordinate out of bounds");
    }
    cells[{ static_cast<std::int64_t>(x), static_cast<std::int64_t>(y) }].push_back(i);
  }

  UnionFind connected(candidates.candidates.size());
  std::vector<std::size_t> incident(candidates.candidates.size(), 0);

  for (std::size_t i = 0; i < candidates.candidates.size(); ++i) {
    if (report.testedDirections >= config.maximumDirectedChecks ||
        report.edges.size() >= config.maximumEdges) {
      break;
    }
    const auto &from = candidates.candidates[i];
    const auto cx = static_cast<std::int64_t>(std::floor(from.position.x / side));
    const auto cy = static_cast<std::int64_t>(std::floor(from.position.y / side));
    std::vector<std::pair<double, std::size_t>> neighbors;

    for (std::int64_t dx = -1; dx <= 1; ++dx) {
      for (std::int64_t dy = -1; dy <= 1; ++dy) {
        const auto it = cells.find({ cx + dx, cy + dy });

        if (it == cells.end()) {
          continue;
        }

        for (const auto j : it->second) {
          if (i == j) {
            continue;
          }
          const auto &to = candidates.candidates[j];
          const double planar = std::hypot(to.position.x - from.position.x,
                                            to.position.y - from.position.y);

          if (planar >= config.minimumPlanarDistance && planar <= side) {
            neighbors.emplace_back(planar, j);
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

    for (const auto &[planar, j] : neighbors) {
      if (trials >= config.maximumNeighborTrials || accepted >= config.maximumOutgoingEdges ||
          report.testedDirections >= config.maximumDirectedChecks ||
          report.edges.size() >= config.maximumEdges) {
        break;
      }
      ++trials;
      ++report.testedDirections;
      const auto &to = candidates.candidates[j];
      const double rise = to.position.z - from.position.z;

      // Bound impossible gradients, but allow a long climb made of local walkable steps.
      constexpr double kMinimumUpNormal = 0.7;
      const double maximumGrade = std::sqrt(1.0 / (kMinimumUpNormal * kMinimumUpNormal) - 1.0);

      if (rise > planar * maximumGrade + config.maximumStepUp ||
          rise < -planar * maximumGrade - config.maximumStepDown) {
        ++report.rejectedHeight;
        continue;
      }
      const bool standing = from.standingFree && to.standingFree;
      const bool crouching = from.crouchingFree && to.crouchingFree;

      if (!standing && !crouching) {
        ++report.rejectedStance;
        continue;
      }
      bool collisionRejected = false;
      bool groundRejected = false;
      bool acceptedEdge = false;

      for (const auto hull : { std::size_t { 1 }, std::size_t { 3 } }) {
        if ((hull == 1 && !standing) || (hull == 3 && !crouching)) {
          continue;
        }
        const auto walk = detail::validateWalk(queries, world, brushes, from, to, hull, config);
        report.walkSamples += walk.samples;

        if (!walk.passed()) {
          collisionRejected |= walk.failure == detail::WalkFailure::Collision;
          groundRejected |= walk.failure != detail::WalkFailure::Collision;

          switch (walk.failure) {
          case detail::WalkFailure::Collision:
            ++report.rejectedWalkCollision;
            break;
          case detail::WalkFailure::Support:
            ++report.rejectedWalkSupport;
            break;
          case detail::WalkFailure::Slope:
            ++report.rejectedWalkSlope;
            break;
          case detail::WalkFailure::Step:
            ++report.rejectedWalkStep;
            break;
          case detail::WalkFailure::Endpoint:
            ++report.rejectedWalkEndpoint;
            break;
          case detail::WalkFailure::None:
            break;
          }

          continue;
        }
        const auto stance = hull == 1 ? collision::PlayerStance::Standing : collision::PlayerStance::Crouching;
        report.edges.push_back({
          .id = report.edges.size() + 1,
          .from = from.id,
          .to = to.id,
          .planarDistance = planar,
          .heightChange = rise,
          .stance = stance,
          .traversal = TraversalKind::Walk,
          .conditionalSupport = from.conditionalSupport || to.conditionalSupport,
          .estimatedFlightSeconds = 0.0,
          .estimatedHorizontalSpeed = 0.0,
          .canStandAfterLanding = to.standingFree
        });
        report.conditionalEdges += from.conditionalSupport || to.conditionalSupport;
        report.standingEdges += hull == 1;
        report.crouchingEdges += hull == 3;
        ++accepted;
        ++incident[i];
        ++incident[j];
        connected.merge(i, j);
        acceptedEdge = true;
        break;
      }

      if (!acceptedEdge) {
        report.rejectedCollision += collisionRejected;
        report.rejectedGround += !collisionRejected && groundRejected;
      }
    }
  }
  std::map<std::size_t, std::size_t> groups;

  for (std::size_t i = 0; i < candidates.candidates.size(); ++i) {
    ++groups[connected.root(i)];
    report.isolatedCandidates += incident[i] == 0;
  }
  report.weakComponents = groups.size();

  for (const auto &[root, size] : groups) {
    (void)root;
    report.largestComponent = std::max(report.largestComponent, size);
  }
  appendJumpTransitions(map, candidates, report);

  return report;
}

std::string connectivityJson(const ConnectivityReport &report) {
  Json::Array edges;
  edges.reserve(report.edges.size());

  for (const auto &e : report.edges) {
    edges.emplace_back(Json::Object {
      { "id", number(e.id) },
      { "from", number(e.from) },
      { "to", number(e.to) },
      { "planar_distance", e.planarDistance },
      { "height_change", e.heightChange },
      { "stance", e.stance == collision::PlayerStance::Standing ? "standing" : "crouching" },
      { "traversal", e.traversal == TraversalKind::Walk ? "walk" :
                       e.traversal == TraversalKind::Jump ? "jump" :
                       e.traversal == TraversalKind::CrouchJump ? "crouch_jump" : "drop" },
      { "conditional_support", e.conditionalSupport },
      { "estimated_flight_seconds", e.estimatedFlightSeconds },
      { "estimated_horizontal_speed", e.estimatedHorizontalSpeed },
      { "available_approach_distance", e.availableApproachDistance },
      { "reachable_takeoff_speed", e.reachableTakeoffSpeed },
      { "can_stand_after_landing", e.canStandAfterLanding }
    });
  }

  return io::writeJson(Json::Object {
    { "format", "aipb-preliminary-connectivity" },
    { "schema_version", "0.7.0" },
    { "source_candidates_schema", "0.3.3" },
    { "candidates_sha256", report.candidatesSha256 },
    { "map_name", report.mapName },
    { "source_sha256", report.sourceSha256 },
    { "artifact_sha256", report.artifactSha256 },
    { "config", Json::Object {
      { "maximum_planar_distance", report.config.maximumPlanarDistance },
      { "minimum_planar_distance", report.config.minimumPlanarDistance },
      { "maximum_step_up", report.config.maximumStepUp },
      { "maximum_step_down", report.config.maximumStepDown },
      { "ground_tolerance", report.config.groundTolerance },
      { "clearance_lift", report.config.clearanceLift },
      { "walk_sample_spacing", 4.0 },
      { "maximum_walk_samples", 64.0 },
      { "maximum_neighbor_trials", number(report.config.maximumNeighborTrials) },
      { "maximum_outgoing_edges", number(report.config.maximumOutgoingEdges) },
      { "maximum_directed_checks", number(report.config.maximumDirectedChecks) },
      { "maximum_edges", number(report.config.maximumEdges) },
      { "maximum_jump_distance", report.config.maximumJumpDistance },
      { "maximum_approach_distance", report.config.maximumApproachDistance },
      { "maximum_jump_rise", report.config.maximumJumpRise },
      { "maximum_crouch_jump_rise", report.config.maximumCrouchJumpRise },
      { "jump_physics", Json::Object {
        { "gravity", report.config.jumpPhysics.gravity },
        { "jump_apex", report.config.jumpPhysics.jumpApex },
        { "maximum_horizontal_speed", report.config.jumpPhysics.maximumHorizontalSpeed },
        { "ground_acceleration", report.config.jumpPhysics.groundAcceleration },
        { "standing_half_height", report.config.jumpPhysics.standingHalfHeight },
        { "crouching_half_height", report.config.jumpPhysics.crouchingHalfHeight },
        { "airborne_duck_delay", report.config.jumpPhysics.airborneDuckDelay },
        { "jump_impulse", jumpImpulse(report.config.jumpPhysics) },
        { "theoretical_jump_rise", theoreticalJumpRise(report.config.jumpPhysics, false) },
        { "theoretical_crouch_jump_rise", theoreticalJumpRise(report.config.jumpPhysics, true) }
      } },
      { "maximum_jump_neighbor_trials", number(report.config.maximumJumpNeighborTrials) },
      { "maximum_jump_edges_per_candidate", number(report.config.maximumJumpEdgesPerCandidate) },
      { "maximum_jump_checks", number(report.config.maximumJumpChecks) },
      { "maximum_drop_height", report.config.maximumDropHeight },
      { "maximum_drop_distance", report.config.maximumDropDistance },
      { "maximum_drop_neighbor_trials", number(report.config.maximumDropNeighborTrials) },
      { "maximum_drop_edges_per_candidate", number(report.config.maximumDropEdgesPerCandidate) },
      { "maximum_drop_checks", number(report.config.maximumDropChecks) }
    } },
    { "candidate_count", number(report.candidateCount) },
    { "edge_count", number(report.edges.size()) },
    { "tested_directions", number(report.testedDirections) },
    { "rejected_height", number(report.rejectedHeight) },
    { "rejected_stance", number(report.rejectedStance) },
    { "rejected_collision", number(report.rejectedCollision) },
    { "rejected_ground", number(report.rejectedGround) },
    { "walk_samples", number(report.walkSamples) },
    { "rejected_walk_collision", number(report.rejectedWalkCollision) },
    { "rejected_walk_support", number(report.rejectedWalkSupport) },
    { "rejected_walk_slope", number(report.rejectedWalkSlope) },
    { "rejected_walk_step", number(report.rejectedWalkStep) },
    { "rejected_walk_endpoint", number(report.rejectedWalkEndpoint) },
    { "standing_edges", number(report.standingEdges) },
    { "crouching_edges", number(report.crouchingEdges) },
    { "jump_edges", number(report.jumpEdges) },
    { "crouch_jump_edges", number(report.crouchJumpEdges) },
    { "jump_trials", number(report.jumpTrials) },
    { "drop_trials", number(report.dropTrials) },
    { "drop_edges", number(report.dropEdges) },
    { "rejected_drop_physics", number(report.rejectedDropPhysics) },
    { "rejected_drop_collision", number(report.rejectedDropCollision) },
    { "rejected_drop_landing", number(report.rejectedDropLanding) },
    { "rejected_jump_collision", number(report.rejectedJumpCollision) },
    { "rejected_jump_landing", number(report.rejectedJumpLanding) },
    { "rejected_jump_physics", number(report.rejectedJumpPhysics) },
    { "rejected_jump_approach", number(report.rejectedJumpApproach) },
    { "crouched_landings", number(report.crouchedLandings) },
    { "crouched_landings_without_standing", number(report.crouchedLandingsWithoutStanding) },
    { "conditional_edges", number(report.conditionalEdges) },
    { "conditional_islands", number(report.conditionalIslands) },
    { "weak_components", number(report.weakComponents) },
    { "largest_component", number(report.largestComponent) },
    { "isolated_candidates", number(report.isolatedCandidates) },
    { "edges", std::move(edges) }
  }) + "\n";
}

} // namespace aipb::map::navigation
