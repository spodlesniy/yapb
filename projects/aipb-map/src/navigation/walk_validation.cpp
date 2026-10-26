#include "walk_validation.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

namespace aipb::map::navigation::detail {
namespace {

constexpr double kMinimumUpNormal = 0.7;
constexpr double kSampleSpacing = 4.0;
constexpr std::size_t kMaximumSamples = 64;

Vec3 playerOrigin(
  const Candidate &candidate,
  std::size_t hull,
  double lift
) {
  const auto &normal = candidate.floorNormal;
  const double halfHeight = hull == 1 ? 36.0 : 18.0;
  const double support = 16.0 * (std::abs(normal.x) + std::abs(normal.y)) / normal.z;

  return {
    candidate.position.x,
    candidate.position.y,
    candidate.position.z + halfHeight + support + lift
  };
}

Vec3 localPoint(
  Vec3 point,
  Vec3 origin
) {
  return {
    point.x - origin.x,
    point.y - origin.y,
    point.z - origin.z
  };
}

bool clear(
  const collision::CollisionQueries &queries,
  ObjectId world,
  const std::vector<Brush> &brushes,
  std::size_t hull,
  Vec3 from,
  Vec3 to
) {
  const auto hit = queries.traceSegment(world, hull, from, to);

  if (hit.startSolid || hit.impactPlane) {
    return false;
  }

  for (const auto &brush : brushes) {
    const auto result = queries.traceSegment(brush.model->id, hull,
      localPoint(from, brush.entity->origin), localPoint(to, brush.entity->origin));

    if (result.startSolid || result.impactPlane) {
      return false;
    }
  }

  return true;
}

struct Surface {
  double originZ {};
  double normalZ {};
};

std::optional<Surface> floorAt(
  const collision::CollisionQueries &queries,
  ObjectId world,
  const std::vector<Brush> &brushes,
  std::size_t hull,
  Vec3 previous,
  double x,
  double y,
  const ConnectivityConfig &config
) {
  const Vec3 top {
    x,
    y,
    previous.z + config.maximumStepUp + config.clearanceLift
  };
  const Vec3 bottom {
    x,
    y,
    previous.z - config.maximumStepDown - config.groundTolerance - config.clearanceLift
  };
  std::optional<Surface> best;
  const auto consider = [&](const collision::TraceResult &hit, double offset) {
    if (hit.startSolid || !hit.impactPlane || hit.impactPlane->normal.z <= 0) {
      return;
    }
    const double z = hit.endPosition.z + offset;

    if (!best || z > best->originZ) {
      best = Surface {
        .originZ = z,
        .normalZ = hit.impactPlane->normal.z
      };
    }
  };
  consider(queries.traceSegment(world, hull, top, bottom), 0.0);

  for (const auto &brush : brushes) {
    if (!suppliesSupport(brush.role)) {
      continue;
    }
    const auto origin = brush.entity->origin;
    consider(queries.traceSegment(brush.model->id, hull,
      localPoint(top, origin), localPoint(bottom, origin)), origin.z);
  }

  return best;
}

// A short ledge can require the usual up-across-down maneuver.
bool stepClear(
  const collision::CollisionQueries &queries,
  ObjectId world,
  const std::vector<Brush> &brushes,
  std::size_t hull,
  Vec3 from,
  Vec3 to,
  const ConnectivityConfig &config
) {
  const Vec3 raised {
    from.x,
    from.y,
    from.z + config.maximumStepUp + config.clearanceLift
  };
  const Vec3 across {
    to.x,
    to.y,
    raised.z
  };

  if (!clear(queries, world, brushes, hull, from, raised) ||
      !clear(queries, world, brushes, hull, raised, across)) {
    return false;
  }
  // The downward leg may touch the destination floor, but cannot hit higher geometry.
  const auto near = Vec3 {
    to.x,
    to.y,
    to.z + config.clearanceLift
  };

  if (!clear(queries, world, brushes, hull, across, near)) {
    return false;
  }

  return clear(queries, world, brushes, hull, near, to);
}

} // namespace

WalkResult validateWalk(
  const collision::CollisionQueries &queries,
  ObjectId world,
  const std::vector<Brush> &brushes,
  const Candidate &from,
  const Candidate &to,
  std::size_t hull,
  const ConnectivityConfig &config
) {
  const Vec3 start = playerOrigin(from, hull, config.clearanceLift);
  const Vec3 destination = playerOrigin(to, hull, config.clearanceLift);
  const double planar = std::hypot(to.position.x - from.position.x,
                                   to.position.y - from.position.y);
  const auto steps = std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil(planar / kSampleSpacing)));

  if (steps > kMaximumSamples) {
    return { .failure = WalkFailure::Support };
  }
  Vec3 previous = start;

  for (std::size_t i = 1; i <= steps; ++i) {
    const double t = static_cast<double>(i) / static_cast<double>(steps);
    const double x = start.x + (destination.x - start.x) * t;
    const double y = start.y + (destination.y - start.y) * t;
    const auto floor = floorAt(queries, world, brushes, hull, previous, x, y, config);

    if (!floor) {
      return { .failure = WalkFailure::Support, .samples = i };
    }

    if (floor->normalZ < kMinimumUpNormal) {
      return { .failure = WalkFailure::Slope, .samples = i };
    }
    const Vec3 next { x, y, floor->originZ + config.clearanceLift };
    const double rise = next.z - previous.z;

    if (rise > config.maximumStepUp + config.clearanceLift ||
        rise < -config.maximumStepDown - config.groundTolerance) {
      return { .failure = WalkFailure::Step, .samples = i };
    }

    if (!clear(queries, world, brushes, hull, previous, next) &&
        !stepClear(queries, world, brushes, hull, previous, next, config)) {
      return { .failure = WalkFailure::Collision, .samples = i };
    }
    previous = next;
  }

  if (std::abs(previous.z - destination.z) > config.groundTolerance + config.clearanceLift) {
    return { .failure = WalkFailure::Endpoint, .samples = steps };
  }

  return { .samples = steps };
}

} // namespace aipb::map::navigation::detail
