#include <aipb/map/heightfield/builder.h>
#include <aipb/map/collision/queries.h>

#include "json.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace aipb::map::heightfield {
namespace {
using io::Json;

std::size_t count(double size, double step) {
  const auto n = std::ceil(size / step);

  if (!std::isfinite(n) || n < 1 || n > static_cast<double>(std::numeric_limits<std::size_t>::max() - 4)) {
    throw std::invalid_argument("heightfield dimensions exceed supported range");
  }

  return static_cast<std::size_t>(n);
}

Json number(std::size_t n) {
  return static_cast<double>(n);
}

Json vec(Vec3 v) {
  return Json::Array { v.x, v.y, v.z };
}

Json candidateJson(const Candidate &c) {
  return Json::Object {
    { "column", number(c.column) },
    { "row", number(c.row) },
    { "hull_mask", number(c.hullMask) },
    { "position", vec(c.position) },
    { "floor_normal", vec(c.floorNormal) },
    { "standing_free", c.standingFree },
    { "crouching_free", c.crouchingFree }
  };
}

double offset(std::size_t hull, Vec3 n) {
  const double radius = 16.0;
  const double halfHeight = hull == 1 ? 36.0 : 18.0;

  return (radius * (std::abs(n.x) + std::abs(n.y)) + halfHeight * n.z) / n.z;
}

bool finite(Vec3 p) {
  return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}


struct ScanContext {
  const collision::CollisionQueries &queries;
  const CollisionPartitionModel &world;
  const Config &config;
  double top {};
  double bottom {};
  bool renderAvailable {};
  double renderMinZ {};
  double renderMaxZ {};
};

void scanColumn(
  const ScanContext &context,
  Result &result,
  double x,
  double y,
  std::size_t column,
  std::size_t row
) {
  const auto &queries = context.queries;
  const auto &world = context.world;
  const auto &config = context.config;
  const double top = context.top;
  const double bottom = context.bottom;
  const bool renderAvailable = context.renderAvailable;
  const double renderMinZ = context.renderMinZ;
  const double renderMaxZ = context.renderMaxZ;
  std::vector<Candidate> local;

  for (const auto hull : std::array<std::size_t, 2> { 1, 3 }) {
    double upper = top;

    while (upper > bottom) {
      const auto lower = std::max(bottom, upper - config.verticalStep);
      const Vec3 start { x, y, upper };
      const Vec3 end { x, y, lower };
      ++result.scannedIntervals;
      const auto startContents = queries.pointContents(world.id, hull, start);
      const auto endContents = queries.pointContents(world.id, hull, end);
      // Trace even when both endpoints are empty: a thin solid interval can lie between them.
      if (startContents != collision::kContentsSolid) {
        const auto hit = queries.traceSegment(world.id, hull, start, end);

        if (hit.impactPlane) {
          if (endContents != collision::kContentsSolid) {
            ++result.thinIntervalHits;
          }
          const auto normal = hit.impactPlane->normal;
          const bool slope = normal.z < 0.9999;
          result.slopeImpacts += slope;

          if (normal.z < config.minimumUpNormal) {
            ++result.rejectedImpacts;
          } else {
            const double estimateZ = hit.endPosition.z - offset(hull, normal);
            const double footprintOffset = 16.0 * (std::abs(normal.x) + std::abs(normal.y)) / normal.z;
            const double tolerance = config.surfaceHeightTolerance + footprintOffset;

            if (!std::isfinite(estimateZ) || !std::isfinite(tolerance)) {
              ++result.rejectedInvalidHeight;
            } else {
              const Vec3 lifted { x, y, hit.endPosition.z + config.clearanceLift };

              if (queries.pointContents(world.id, hull, lifted) == collision::kContentsSolid) {
                ++result.rejectedOwnSolid;
              } else {
                // Start at the clip contact's free side so slope support can be found below it.
                const Vec3 pointStart { x, y, hit.endPosition.z + config.clearanceLift };
                const Vec3 pointEnd { x, y, estimateZ - tolerance - config.clearanceLift };
                const auto pointTrace = queries.traceSegment(world.id, 0, pointStart, pointEnd);

                if (pointTrace.startSolid) {
                  ++result.rejectedPointStartSolid;
                  ++result.rejectedUnverifiedPointHull;
                } else if (!pointTrace.impactPlane) {
                  ++result.rejectedMissingPointImpact;
                  ++result.rejectedUnverifiedPointHull;
                } else if (pointTrace.impactPlane->normal.z < config.minimumUpNormal) {
                  ++result.rejectedPointNormal;
                  ++result.rejectedUnverifiedPointHull;
                } else if (std::abs(pointTrace.endPosition.z - estimateZ) > tolerance) {
                  ++result.rejectedSurfaceMismatch;
                  ++result.rejectedUnverifiedPointHull;
                } else {
                  // Hull 0 provides the physical contact height and plane normal.
                  const auto physical = pointTrace.endPosition;
                  const auto pointNormal = pointTrace.impactPlane->normal;
                  const double standingZ = physical.z + offset(1, pointNormal) + config.clearanceLift;
                  const double crouchingZ = physical.z + offset(3, pointNormal) + config.clearanceLift;

                  if (!finite(physical) || !std::isfinite(standingZ) || !std::isfinite(crouchingZ)) {
                    ++result.rejectedInvalidHeight;
                  } else if (physical.z < world.mins.z || physical.z > world.maxs.z) {
                    ++result.rejectedOutsideWorld;
                  } else if (renderAvailable && (physical.z < renderMinZ || physical.z > renderMaxZ)) {
                    ++result.rejectedOutsideRender;
                  } else {
                    const bool standingFree = queries.canOccupy(
                      world.id, collision::PlayerStance::Standing, { x, y, standingZ });
                    const bool crouchingFree = queries.canOccupy(
                      world.id, collision::PlayerStance::Crouching, { x, y, crouchingZ });

                    if (!standingFree && !crouchingFree) {
                      ++result.rejectedNoClearance;
                    } else {
                      ++result.floorHits;
                      result.acceptedSlopeContacts += slope;
                      result.correctedPointContacts +=
                        std::abs(physical.z - estimateZ) > config.mergeHeightTolerance;
                      const auto match = std::find_if(local.begin(), local.end(), [&](const Candidate &candidate) {
                        return std::abs(candidate.position.z - physical.z) <= config.mergeHeightTolerance;
                      });

                      if (match != local.end()) {
                        match->hullMask |= static_cast<std::uint32_t>(1u << hull);
                        match->standingFree |= standingFree;
                        match->crouchingFree |= crouchingFree;
                        ++result.mergedContacts;
                      } else {
                        local.push_back({
                          column,
                          row,
                          static_cast<std::uint32_t>(1u << hull),
                          physical,
                          pointNormal,
                          standingFree,
                          crouchingFree
                        });
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
      upper = lower;
    }
  }

  if (!local.empty()) {
    ++result.columnsWithCandidates;
  }
  if (local.size() > 1) {
    ++result.multiLayerColumns;
  }
  result.maximumLayers = std::max(result.maximumLayers, local.size());
  std::sort(local.begin(), local.end(), [](const Candidate &a, const Candidate &b) {
    return a.position.z > b.position.z;
  });

  for (const auto &candidate : local) {
    result.candidates.push_back(candidate);
  }

}

} // namespace

Result build(
  const MapModel &map,
  std::string artifactSha256,
  Config config
) {
  if (!std::isfinite(config.cellSize) || config.cellSize <= 0 ||
      !std::isfinite(config.verticalStep) || config.verticalStep <= 0 ||
      !std::isfinite(config.minimumUpNormal) || config.minimumUpNormal <= 0 || config.minimumUpNormal > 1 ||
      !std::isfinite(config.mergeHeightTolerance) || config.mergeHeightTolerance < 0 ||
      !std::isfinite(config.clearanceLift) || config.clearanceLift <= 0 ||
      !std::isfinite(config.surfaceHeightTolerance) || config.surfaceHeightTolerance < 0 ||
      config.maximumColumns == 0 || config.maximumVerticalSteps == 0 || config.maximumSamples == 0) {
    throw std::invalid_argument("invalid heightfield configuration");
  }
  collision::CollisionQueries queries(map);
  const auto &world = map.partition->models.front();
  const auto dx = world.maxs.x - world.mins.x;
  const auto dy = world.maxs.y - world.mins.y;
  const auto dz = world.maxs.z - world.mins.z;
  const auto top = world.maxs.z + config.verticalStep;
  const auto bottom = world.mins.z - config.verticalStep;

  if (!std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(dz) ||
      dx <= 0 || dy <= 0 || dz <= 0 || !std::isfinite(top) || !std::isfinite(bottom)) {
    throw std::invalid_argument("invalid world bounds");
  }
  const auto columns = count(dx, config.cellSize);
  const auto rows = count(dy, config.cellSize);
  const auto steps = count(top - bottom, config.verticalStep);

  if (columns > config.maximumColumns || rows > config.maximumColumns / columns ||
      steps > config.maximumVerticalSteps ||
      steps > config.maximumSamples / columns / rows) {
    throw std::invalid_argument("heightfield sampling budget exceeded");
  }
  Result result {
    .mapName = map.name,
    .sourceSha256 = map.sourceSha256,
    .artifactSha256 = std::move(artifactSha256),
    .config = config,
    .columns = columns,
    .rows = rows,
    .scannedIntervals = 0,
    .floorHits = 0,
    .thinIntervalHits = 0,
    .rejectedImpacts = 0,
    .rejectedInvalidHeight = 0,
    .rejectedOutsideWorld = 0,
    .rejectedOutsideRender = 0,
    .rejectedOwnSolid = 0,
    .rejectedUnverifiedPointHull = 0,
    .rejectedPointStartSolid = 0,
    .rejectedMissingPointImpact = 0,
    .rejectedPointNormal = 0,
    .rejectedSurfaceMismatch = 0,
    .rejectedNoClearance = 0,
    .slopeImpacts = 0,
    .acceptedSlopeContacts = 0,
    .correctedPointContacts = 0,
    .mergedContacts = 0,
    .columnsWithCandidates = 0,
    .multiLayerColumns = 0,
    .maximumLayers = 0,
    .candidates = {}
  };
  // World BSP render geometry is an independent bound on contact altitude.
  bool renderAvailable = false;
  double renderMinZ = std::numeric_limits<double>::infinity();
  double renderMaxZ = -std::numeric_limits<double>::infinity();

  for (const auto &mesh : map.meshes) {
    if (mesh.id != world.id) {
      continue;
    }

    for (const auto &v : mesh.vertices) {
      renderAvailable = true;
      renderMinZ = std::min(renderMinZ, v.z);
      renderMaxZ = std::max(renderMaxZ, v.z);
    }
  }

  const ScanContext context {
    .queries = queries,
    .world = world,
    .config = config,
    .top = top,
    .bottom = bottom,
    .renderAvailable = renderAvailable,
    .renderMinZ = renderMinZ,
    .renderMaxZ = renderMaxZ
  };

  for (std::size_t row = 0; row < rows; ++row) {
    const double lowY = world.mins.y + static_cast<double>(row) * config.cellSize;
    const double y = lowY + (std::min(world.maxs.y, lowY + config.cellSize) - lowY) * 0.5;

    for (std::size_t column = 0; column < columns; ++column) {
      const double lowX = world.mins.x + static_cast<double>(column) * config.cellSize;
      const double x = lowX + (std::min(world.maxs.x, lowX + config.cellSize) - lowX) * 0.5;
      scanColumn(context, result, x, y, column, row);
    }
  }

  return result;
}

std::vector<Candidate> sampleLocations(
  const MapModel &map,
  const std::vector<Vec3> &locations,
  Config config
) {
  if (!std::isfinite(config.verticalStep) || config.verticalStep <= 0 ||
      !std::isfinite(config.minimumUpNormal) || config.minimumUpNormal <= 0 || config.minimumUpNormal > 1 ||
      !std::isfinite(config.mergeHeightTolerance) || config.mergeHeightTolerance < 0 ||
      !std::isfinite(config.clearanceLift) || config.clearanceLift <= 0 ||
      !std::isfinite(config.surfaceHeightTolerance) || config.surfaceHeightTolerance < 0 ||
      config.maximumVerticalSteps == 0 || config.maximumSamples == 0 ||
      locations.size() > config.maximumColumns || locations.size() > config.maximumSamples) {
    throw std::invalid_argument("invalid local heightfield sampling budget or configuration");
  }
  collision::CollisionQueries queries(map);
  const auto &world = map.partition->models.front();
  const double top = world.maxs.z + config.verticalStep;
  const double bottom = world.mins.z - config.verticalStep;

  if (!finite(world.mins) || !finite(world.maxs) || !std::isfinite(top) || !std::isfinite(bottom) ||
      top <= bottom || count(top - bottom, config.verticalStep) > config.maximumVerticalSteps) {
    throw std::invalid_argument("invalid local heightfield world bounds");
  }
  bool renderAvailable = false;
  double renderMinZ = std::numeric_limits<double>::infinity();
  double renderMaxZ = -std::numeric_limits<double>::infinity();

  for (const auto &mesh : map.meshes) {
    if (mesh.id != world.id) {
      continue;
    }

    for (const auto &vertex : mesh.vertices) {
      renderAvailable = true;
      renderMinZ = std::min(renderMinZ, vertex.z);
      renderMaxZ = std::max(renderMaxZ, vertex.z);
    }
  }
  const ScanContext context {
    .queries = queries,
    .world = world,
    .config = config,
    .top = top,
    .bottom = bottom,
    .renderAvailable = renderAvailable,
    .renderMinZ = renderMinZ,
    .renderMaxZ = renderMaxZ
  };
  Result result {};
  const auto intervals = count(top - bottom, config.verticalStep);

  if (!locations.empty() && intervals > config.maximumSamples / locations.size()) {
    throw std::invalid_argument("local heightfield sampling budget exceeded");
  }

  for (std::size_t i = 0; i < locations.size(); ++i) {
    const auto point = locations[i];

    if (!finite(point) || point.x < world.mins.x || point.x > world.maxs.x ||
        point.y < world.mins.y || point.y > world.maxs.y) {
      throw std::invalid_argument("local heightfield XY position outside world bounds");
    }
    scanColumn(context, result, point.x, point.y, i, 0);
  }

  return result.candidates;
}

std::string toJson(const Result &result) {
  Json::Array candidates;
  candidates.reserve(result.candidates.size());

  for (const auto &candidate : result.candidates) {
    candidates.push_back(candidateJson(candidate));
  }

  return io::writeJson(Json::Object {
    { "format", "aipb-heightfield-candidates" },
    { "schema_version", "0.3.0" },
    { "map_name", result.mapName },
    { "source_sha256", result.sourceSha256 },
    { "artifact_sha256", result.artifactSha256 },
    { "config", Json::Object {
        { "cell_size", result.config.cellSize },
        { "vertical_step", result.config.verticalStep },
        { "minimum_up_normal", result.config.minimumUpNormal },
        { "merge_height_tolerance", result.config.mergeHeightTolerance },
        { "clearance_lift", result.config.clearanceLift },
        { "surface_height_tolerance", result.config.surfaceHeightTolerance },
        { "maximum_columns", number(result.config.maximumColumns) },
        { "maximum_vertical_steps", number(result.config.maximumVerticalSteps) },
        { "maximum_samples", number(result.config.maximumSamples) }
      } },
    { "columns", number(result.columns) },
    { "rows", number(result.rows) },
    { "scanned_intervals", number(result.scannedIntervals) },
    { "floor_hits", number(result.floorHits) },
    { "thin_interval_hits", number(result.thinIntervalHits) },
    { "rejected_impacts", number(result.rejectedImpacts) },
    { "rejected_invalid_height", number(result.rejectedInvalidHeight) },
    { "rejected_outside_world", number(result.rejectedOutsideWorld) },
    { "rejected_outside_render", number(result.rejectedOutsideRender) },
    { "rejected_own_solid", number(result.rejectedOwnSolid) },
    { "rejected_unverified_point_hull", number(result.rejectedUnverifiedPointHull) },
    { "rejected_point_start_solid", number(result.rejectedPointStartSolid) },
    { "rejected_missing_point_impact", number(result.rejectedMissingPointImpact) },
    { "rejected_point_normal", number(result.rejectedPointNormal) },
    { "rejected_surface_mismatch", number(result.rejectedSurfaceMismatch) },
    { "rejected_no_clearance", number(result.rejectedNoClearance) },
    { "slope_impacts", number(result.slopeImpacts) },
    { "accepted_slope_contacts", number(result.acceptedSlopeContacts) },
    { "corrected_point_contacts", number(result.correctedPointContacts) },
    { "merged_contacts", number(result.mergedContacts) },
    { "columns_with_candidates", number(result.columnsWithCandidates) },
    { "multi_layer_columns", number(result.multiLayerColumns) },
    { "maximum_layers", number(result.maximumLayers) },
    { "candidates", std::move(candidates) }
  }) + "\n";
}

} // namespace aipb::map::heightfield
