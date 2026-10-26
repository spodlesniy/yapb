#include <aipb/map/collision/queries.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace aipb::map::collision {
namespace {

const CollisionPartition &requirePartition(const MapModel &map) {
  validate(map);

  if (!map.partition) {
    throw std::invalid_argument("map has no collision partition");
  }

  return *map.partition;
}

void requireFinite(Vec3 point) {
  if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) {
    throw std::invalid_argument("collision query coordinates must be finite");
  }
}

long double signedDistance(
  const CollisionPlane &plane,
  Vec3 point
) {
  const auto distance = static_cast<long double>(plane.distance);

  if (plane.type < 3) {
    const double coordinate = plane.type == 0 ? point.x : plane.type == 1 ? point.y : point.z;

    return static_cast<long double>(coordinate) - distance;
  }

  return static_cast<long double>(plane.normal.x) * point.x + static_cast<long double>(plane.normal.y) * point.y +
         static_cast<long double>(plane.normal.z) * point.z - distance;
}

Vec3 interpolate(
  Vec3 start,
  Vec3 end,
  double fraction
) {
  if (fraction == 0.0) {
    return start;
  }

  if (fraction == 1.0) {
    return end;
  }

  return {
    start.x + (end.x - start.x) * fraction,
    start.y + (end.y - start.y) * fraction,
    start.z + (end.z - start.z) * fraction
  };
}

CollisionPlane facingPlane(
  const CollisionPlane &plane,
  bool frontFirst
) {
  if (frontFirst) {
    return plane;
  }

  return {
    { -plane.normal.x, -plane.normal.y, -plane.normal.z },
    -plane.distance,
    plane.type
  };
}

std::size_t stanceHull(PlayerStance stance) {
  switch (stance) {
  case PlayerStance::Standing:
    return 1;
  case PlayerStance::Crouching:
    return 3;
  }

  throw std::invalid_argument("invalid player stance");
}

struct Interval {
  std::int32_t node {};
  double begin {};
  double end {};
  std::optional<CollisionPlane> entryPlane {};
};

} // namespace

CollisionQueries::CollisionQueries(const MapModel &map) : partition_(requirePartition(map)) {
}

std::int32_t CollisionQueries::root(
  ObjectId modelId,
  std::size_t hull
) const {
  if (hull >= 4) {
    throw std::invalid_argument("BSP hull must be in range 0..3");
  }
  const auto it = std::ranges::find_if(partition_.models, [modelId](const CollisionPartitionModel &model) {
    return model.id == modelId;
  });

  if (it == partition_.models.end()) {
    throw std::invalid_argument("unknown BSP collision model ID");
  }

  return it->headNodes[hull];
}

std::int32_t CollisionQueries::contents(
  std::size_t hull,
  std::int32_t terminal
) const {
  if (hull != 0) {
    return terminal;
  }
  const auto leaf = static_cast<std::size_t>(-static_cast<std::int64_t>(terminal) - 1);

  return partition_.leaves[leaf].contents;
}

std::int32_t CollisionQueries::pointContents(
  ObjectId modelId,
  std::size_t hull,
  Vec3 point
) const {
  requireFinite(point);
  auto nodeIndex = root(modelId, hull);
  const auto &nodes = hull == 0 ? partition_.renderNodes : partition_.clipNodes;

  while (nodeIndex >= 0) {
    const auto &node = nodes[static_cast<std::size_t>(nodeIndex)];
    const auto &plane = partition_.planes[node.planeIndex];
    nodeIndex = signedDistance(plane, point) >= 0 ? node.front : node.back;
  }

  return contents(hull, nodeIndex);
}

TraceResult CollisionQueries::traceSegment(
  ObjectId modelId,
  std::size_t hull,
  Vec3 start,
  Vec3 end
) const {
  requireFinite(start);
  requireFinite(end);
  const auto first = root(modelId, hull);
  const auto &nodes = hull == 0 ? partition_.renderNodes : partition_.clipNodes;
  TraceResult result;
  result.endPosition = end;
  result.startSolid = pointContents(modelId, hull, start) == kContentsSolid;
  result.allSolid = true;

  if (result.startSolid) {
    result.fraction = 0;
    result.endPosition = start;
  }
  std::vector<Interval> pending { { first, 0.0, 1.0, {} } };
  std::size_t visited = 0;
  const auto budget = nodes.size() > (static_cast<std::size_t>(-1) - 256) / 16
                          ? static_cast<std::size_t>(-1)
                          : nodes.size() * 16 + 256;

  while (!pending.empty()) {
    if (++visited > budget) {
      throw std::runtime_error("BSP trace exceeded partition traversal budget");
    }
    const auto current = pending.back();
    pending.pop_back();

    if (current.node < 0) {
      const auto value = contents(hull, current.node);

      if (value != kContentsSolid) {
        result.allSolid = false;
        result.inOpen |= value == kContentsEmpty;
        result.inLiquid |= value == -3 || value == -4 || value == -5 || (value <= -9 && value >= -14);
      } else if (!result.startSolid && !result.impactPlane && current.entryPlane) {
        result.fraction = current.begin;
        result.endPosition = interpolate(start, end, current.begin);
        result.impactPlane = current.entryPlane;
      }

      continue;
    }
    const auto &node = nodes[static_cast<std::size_t>(current.node)];
    const auto &plane = partition_.planes[node.planeIndex];
    const auto firstPoint = interpolate(start, end, current.begin);
    const auto lastPoint = interpolate(start, end, current.end);
    const auto d0 = signedDistance(plane, firstPoint);
    const auto d1 = signedDistance(plane, lastPoint);

    if (d0 >= 0 && d1 >= 0) {
      pending.push_back({ node.front, current.begin, current.end, current.entryPlane });
      continue;
    }

    if (d0 < 0 && d1 < 0) {
      pending.push_back({ node.back, current.begin, current.end, current.entryPlane });
      continue;
    }
    const auto fraction = std::clamp(static_cast<double>(d0 / (d0 - d1)), 0.0, 1.0);
    const auto middle = current.begin + (current.end - current.begin) * fraction;
    const bool frontFirst = d0 >= 0;
    const auto nearNode = frontFirst ? node.front : node.back;
    const auto farNode = frontFirst ? node.back : node.front;
    pending.push_back({ farNode, middle, current.end, facingPlane(plane, frontFirst) });
    pending.push_back({ nearNode, current.begin, middle, current.entryPlane });
  }

  return result;
}

bool CollisionQueries::canOccupy(
  ObjectId modelId,
  PlayerStance stance,
  Vec3 origin
) const {
  return pointContents(modelId, stanceHull(stance), origin) != kContentsSolid;
}

bool CollisionQueries::canTraverse(
  ObjectId modelId,
  PlayerStance stance,
  Vec3 start,
  Vec3 end
) const {
  const auto result = traceSegment(modelId, stanceHull(stance), start, end);

  return !result.startSolid && !result.impactPlane;
}

} // namespace aipb::map::collision
