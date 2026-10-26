#include <aipb/map/navigation/candidates.h>
#include <aipb/map/collision/queries.h>

#include "brush_policy.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace aipb::map::navigation {
namespace {

using detail::Brush;

bool finite(Vec3 p) {
  return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}

Vec3 localPoint(
  Vec3 world,
  Vec3 origin
) {
  return { world.x - origin.x, world.y - origin.y, world.z - origin.z };
}

double playerOriginOffset(
  std::size_t hull,
  Vec3 normal
) {
  const double halfHeight = hull == 1 ? 36.0 : 18.0;

  return (16.0 * (std::abs(normal.x) + std::abs(normal.y)) + halfHeight * normal.z) / normal.z;
}

bool overlaps(
  const Brush &brush,
  Vec3 world,
  double horizontal,
  double vertical
) {
  const auto local = localPoint(world, brush.entity->origin);

  return local.x >= brush.model->mins.x - horizontal &&
         local.x <= brush.model->maxs.x + horizontal &&
         local.y >= brush.model->mins.y - horizontal &&
         local.y <= brush.model->maxs.y + horizontal &&
         local.z >= brush.model->mins.z - vertical &&
         local.z <= brush.model->maxs.z + vertical;
}

bool stanceClear(
  const collision::CollisionQueries &queries,
  ObjectId worldModelId,
  const std::vector<Brush> &brushes,
  Vec3 floor,
  Vec3 normal,
  std::size_t hull,
  double lift
) {
  const Vec3 origin { floor.x, floor.y, floor.z + playerOriginOffset(hull, normal) + lift };

  if (queries.pointContents(worldModelId, hull, origin) == collision::kContentsSolid) {
    return false;
  }

  for (const auto &brush : brushes) {
    if (overlaps(brush, origin, 16.0, hull == 1 ? 36.0 : 18.0) &&
        queries.pointContents(brush.model->id, hull, localPoint(origin, brush.entity->origin)) ==
          collision::kContentsSolid) {
      return false;
    }
  }

  return true;
}

void updateCoverage(CandidateReport &report) {
  report.standingOnly = 0;
  report.crouchingOnly = 0;
  report.bothStances = 0;
  report.sloped = 0;
  report.refinementCandidates = 0;
  report.staticBrushCandidates = 0;

  for (auto &candidate : report.candidates) {
    report.standingOnly += candidate.standingFree && !candidate.crouchingFree;
    report.crouchingOnly += !candidate.standingFree && candidate.crouchingFree;
    report.bothStances += candidate.standingFree && candidate.crouchingFree;
    report.sloped += candidate.floorNormal.z < 0.9999;
    report.refinementCandidates += candidate.refined && !candidate.supportEntityId;
    report.staticBrushCandidates += candidate.supportEntityId != 0;
  }
  report.coveredSpawns = 0;
  report.covered64 = 0;
  report.covered128 = 0;
  report.covered256 = 0;
  report.spawnsWithinPlanarRadius = 0;

  for (auto &spawn : report.spawns) {
    spawn.nearestCandidateId.reset();
    spawn.nearestPlanarDistance = -1.0;
    spawn.nearestVerticalDistance = -1.0;
    spawn.coveredCandidateId.reset();
    spawn.covered64.reset();
    spawn.covered128.reset();
    spawn.covered256.reset();
    double best = std::numeric_limits<double>::infinity();
    double bestVertical = std::numeric_limits<double>::infinity();

    for (const auto &candidate : report.candidates) {
      const double planar = std::hypot(spawn.origin.x - candidate.position.x,
                                       spawn.origin.y - candidate.position.y);
      const double vertical = std::abs(spawn.origin.z - candidate.position.z);

      if (!spawn.nearestCandidateId || planar < spawn.nearestPlanarDistance ||
          (planar == spawn.nearestPlanarDistance && vertical < spawn.nearestVerticalDistance)) {
        spawn.nearestCandidateId = candidate.id;
        spawn.nearestPlanarDistance = planar;
        spawn.nearestVerticalDistance = vertical;
      }

      if (vertical <= report.config.spawnVerticalTolerance) {
        if (!spawn.covered64 && planar <= 64.0) {
          spawn.covered64 = candidate.id;
        }

        if (!spawn.covered128 && planar <= 128.0) {
          spawn.covered128 = candidate.id;
        }

        if (!spawn.covered256 && planar <= 256.0) {
          spawn.covered256 = candidate.id;
        }

        if (planar <= report.config.spawnPlanarRadius &&
            (planar < best || (planar == best && vertical < bestVertical))) {
          best = planar;
          bestVertical = vertical;
          spawn.coveredCandidateId = candidate.id;
        }
      }
    }
    report.coveredSpawns += spawn.coveredCandidateId.has_value();
    report.covered64 += spawn.covered64.has_value();
    report.covered128 += spawn.covered128.has_value();
    report.covered256 += spawn.covered256.has_value();
    report.spawnsWithinPlanarRadius += spawn.nearestCandidateId &&
                                      spawn.nearestPlanarDistance <= report.config.spawnPlanarRadius;
  }
}

} // namespace

CandidateReport generateStaticBrushCandidates(
  const MapModel &map,
  const heightfield::Result &heightfield,
  CandidateConfig config
) {
  auto report = generateAdaptiveCandidates(map, heightfield, config);
  const auto brushes = detail::collectBrushes(map);

  if (brushes.empty()) {
    return report;
  }
  collision::CollisionQueries queries(map);
  const auto worldId = map.partition->models.front().id;
  report.staticBrushModels = std::count_if(brushes.begin(), brushes.end(), [](const Brush &brush) {
    return detail::suppliesSupport(brush.role);
  });
  const double lift = heightfield.config.clearanceLift;

  std::size_t nextCandidateId = report.candidates.size() + 1;

  // Revalidate existing world support with all enabled static brush colliders.
  for (auto &candidate : report.candidates) {
    candidate.standingFree &= stanceClear(queries, worldId, brushes, candidate.position,
                                          candidate.floorNormal, 1, lift);
    candidate.crouchingFree &= stanceClear(queries, worldId, brushes, candidate.position,
                                           candidate.floorNormal, 3, lift);
  }
  std::erase_if(report.candidates, [](const Candidate &candidate) {
    return !candidate.standingFree && !candidate.crouchingFree;
  });

  struct Probe {
    ObjectId modelId {};
    double x {};
    double y {};
  };
  std::vector<Probe> probed;

  const auto attempt = [&](const Brush &brush, double x, double y) {
    if (report.staticBrushSamples >= config.maximumStaticBrushSamples ||
        !std::isfinite(x) || !std::isfinite(y)) {
      return;
    }
    const Vec3 localXY { x - brush.entity->origin.x, y - brush.entity->origin.y, 0 };

    if (localXY.x < brush.model->mins.x || localXY.x > brush.model->maxs.x ||
        localXY.y < brush.model->mins.y || localXY.y > brush.model->maxs.y) {
      return;
    }

    if (std::any_of(probed.begin(), probed.end(), [&](const Probe &probe) {
          return probe.modelId == brush.model->id && probe.x == x && probe.y == y;
        })) {
      return;
    }
    probed.push_back({ brush.model->id, x, y });
    ++report.staticBrushSamples;
    const Vec3 start { localXY.x, localXY.y, brush.model->maxs.z + 64.0 };
    const Vec3 end { localXY.x, localXY.y, brush.model->mins.z - 8.0 };
    const auto trace = queries.traceSegment(brush.model->id, 0, start, end);

    if (trace.startSolid || !trace.impactPlane ||
        trace.impactPlane->normal.z < heightfield.config.minimumUpNormal) {
      return;
    }
    const auto normal = trace.impactPlane->normal;
    const Vec3 floor { x, y, trace.endPosition.z + brush.entity->origin.z };

    if (!finite(floor) || floor.z < brush.model->mins.z + brush.entity->origin.z - 1.0 ||
        floor.z > brush.model->maxs.z + brush.entity->origin.z + 1.0) {
      return;
    }
    const bool standing = stanceClear(queries, worldId, brushes, floor, normal, 1, lift);
    const bool crouching = stanceClear(queries, worldId, brushes, floor, normal, 3, lift);

    if (!standing && !crouching) {
      return;
    }
    // A contact must also be supported by the brush's player clip hull.
    std::uint32_t mask = 0;

    for (const std::size_t hull : { std::size_t { 1 }, std::size_t { 3 } }) {
      const double z = floor.z + playerOriginOffset(hull, normal) + lift;
      const Vec3 top { x, y, z };
      const Vec3 low { x, y, floor.z - 2.0 };
      const auto support = queries.traceSegment(brush.model->id, hull,
                                                 localPoint(top, brush.entity->origin),
                                                 localPoint(low, brush.entity->origin));

      if (!support.startSolid && support.impactPlane &&
          support.impactPlane->normal.z >= heightfield.config.minimumUpNormal) {
        mask |= static_cast<std::uint32_t>(1u << hull);
      }
    }

    if (mask == 0) {
      return;
    }
    const auto match = std::find_if(report.candidates.begin(), report.candidates.end(),
                                    [&](const Candidate &candidate) {
                                      return std::hypot(candidate.position.x - x, candidate.position.y - y) <= 1.0 &&
                                             std::abs(candidate.position.z - floor.z) <=
                                               heightfield.config.mergeHeightTolerance;
                                    });

    if (match != report.candidates.end()) {
      return;
    }

    if (report.candidates.size() >= config.maximumCandidates) {
      return;
    }
    report.candidates.push_back({
      .id = nextCandidateId++,
      .heightfieldIndex = 0,
      .refined = false,
      .sourceSpawnId = 0,
      .sampleIndex = report.staticBrushSamples,
      .supportModelId = brush.model->id,
      .supportEntityId = brush.entity->id,
      .conditionalSupport = brush.role == detail::BrushRole::TriggerSupport,
      .column = 0,
      .row = 0,
      .position = floor,
      .floorNormal = normal,
      .hullMask = mask,
      .standingFree = standing,
      .crouchingFree = crouching
    });
  };

  // Spawn-first sampling ensures late-numbered brushes such as model *180 get a budget.
  for (const auto &brush : brushes) {
    if (!detail::suppliesSupport(brush.role)) {
      continue;
    }

    for (const auto &spawn : report.spawns) {
      attempt(brush, spawn.origin.x, spawn.origin.y);
    }
  }

  for (const auto &brush : brushes) {
    if (report.staticBrushSamples >= config.maximumStaticBrushSamples) {
      break;
    }

    if (!detail::suppliesSupport(brush.role)) {
      continue;
    }
    const double width = brush.model->maxs.x - brush.model->mins.x;
    const double depth = brush.model->maxs.y - brush.model->mins.y;

    if (!std::isfinite(width) || !std::isfinite(depth) || width < 0 || depth < 0) {
      continue;
    }
    const auto columns = std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil(width / config.staticBrushStep)));
    const auto rows = std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil(depth / config.staticBrushStep)));

    if (columns > config.maximumStaticBrushSamples || rows > config.maximumStaticBrushSamples) {
      continue;
    }

    for (std::size_t row = 0; row < rows && report.staticBrushSamples < config.maximumStaticBrushSamples; ++row) {
      const double y = brush.model->mins.y +
                       (static_cast<double>(row) + 0.5) * depth / static_cast<double>(rows) +
                       brush.entity->origin.y;

      for (std::size_t column = 0; column < columns && report.staticBrushSamples < config.maximumStaticBrushSamples;
           ++column) {
        const double x = brush.model->mins.x +
                         (static_cast<double>(column) + 0.5) * width / static_cast<double>(columns) +
                         brush.entity->origin.x;
        attempt(brush, x, y);
      }
    }
  }

  updateCoverage(report);

  return report;
}

} // namespace aipb::map::navigation
