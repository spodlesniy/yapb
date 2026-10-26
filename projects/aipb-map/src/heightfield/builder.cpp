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

  for (std::size_t row = 0; row < rows; ++row) {
    const double lowY = world.mins.y + static_cast<double>(row) * config.cellSize;
    const double y = lowY + (std::min(world.maxs.y, lowY + config.cellSize) - lowY) * 0.5;

    for (std::size_t column = 0; column < columns; ++column) {
      const double lowX = world.mins.x + static_cast<double>(column) * config.cellSize;
      const double x = lowX + (std::min(world.maxs.x, lowX + config.cellSize) - lowX) * 0.5;
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

              if (normal.z < config.minimumUpNormal) {
                ++result.rejectedImpacts;
              } else {
                const double physicalZ = hit.endPosition.z - offset(hull, normal);
                const Vec3 physical { x, y, physicalZ };
                const Vec3 lifted { x, y, hit.endPosition.z + config.clearanceLift };

                if (!finite(physical)) {
                  ++result.rejectedInvalidHeight;
                } else if (physicalZ < world.mins.z || physicalZ > world.maxs.z) {
                  ++result.rejectedOutsideWorld;
                } else if (renderAvailable &&
                           (physicalZ < renderMinZ || physicalZ > renderMaxZ)) {
                  ++result.rejectedOutsideRender;
                } else if (queries.pointContents(world.id, hull, lifted) == collision::kContentsSolid) {
                  ++result.rejectedOwnSolid;
                } else {
                  // A point hull query independently checks that the normalized contact borders world solid.
                  const Vec3 above { x, y, physicalZ + config.clearanceLift };
                  const Vec3 below { x, y, physicalZ - config.clearanceLift };
                  const auto pointTrace = queries.traceSegment(world.id, 0, above, below);

                  if (pointTrace.startSolid || !pointTrace.impactPlane ||
                      pointTrace.impactPlane->normal.z < config.minimumUpNormal ||
                      std::abs(pointTrace.endPosition.z - physicalZ) > config.mergeHeightTolerance + config.clearanceLift) {
                    ++result.rejectedUnverifiedPointHull;
                  } else {
                    ++result.floorHits;
                    const auto match = std::find_if(local.begin(), local.end(), [&](const Candidate &candidate) {
                      return std::abs(candidate.position.z - physicalZ) <= config.mergeHeightTolerance;
                    });

                    if (match != local.end()) {
                      match->hullMask |= 1u << hull;
                      ++result.mergedContacts;
                    } else {
                      local.push_back({
                        column,
                        row,
                        static_cast<std::uint32_t>(1u << hull),
                        physical,
                        normal,
                        queries.canOccupy(world.id, collision::PlayerStance::Standing, { x, y, physicalZ + 36.0 + config.clearanceLift }),
                        queries.canOccupy(world.id, collision::PlayerStance::Crouching, { x, y, physicalZ + 18.0 + config.clearanceLift })
                      });
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
  }

  return result;
}

std::string toJson(const Result &result) {
  Json::Array candidates;
  candidates.reserve(result.candidates.size());

  for (const auto &candidate : result.candidates) {
    candidates.push_back(candidateJson(candidate));
  }

  return io::writeJson(Json::Object {
    { "format", "aipb-heightfield-candidates" },
    { "schema_version", "0.2.0" },
    { "map_name", result.mapName },
    { "source_sha256", result.sourceSha256 },
    { "artifact_sha256", result.artifactSha256 },
    { "config", Json::Object {
        { "cell_size", result.config.cellSize },
        { "vertical_step", result.config.verticalStep },
        { "minimum_up_normal", result.config.minimumUpNormal },
        { "merge_height_tolerance", result.config.mergeHeightTolerance },
        { "clearance_lift", result.config.clearanceLift },
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
    { "merged_contacts", number(result.mergedContacts) },
    { "columns_with_candidates", number(result.columnsWithCandidates) },
    { "multi_layer_columns", number(result.multiLayerColumns) },
    { "maximum_layers", number(result.maximumLayers) },
    { "candidates", std::move(candidates) }
  }) + "\n";
}

} // namespace aipb::map::heightfield
