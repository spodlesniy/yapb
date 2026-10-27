#include <aipb/map/heightfield/builder.h>
#include <aipb/map/collision/queries.h>

#include "json.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <cstdint>
#include <utility>
#include <vector>
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

// Supplement the coarse grid only at narrow render surfaces; collision is still authoritative.
void sampleThinWorldSurfaces(
  const MapModel &map,
  const ScanContext &context,
  Result &result
) {
  const auto &config = context.config;
  const auto &world = context.world;
  const auto verticalIntervals = count(context.top - context.bottom, config.verticalStep);

  if (verticalIntervals > config.maximumSamples / 2 ||
      result.scannedIntervals >= config.maximumSamples) {
    return;
  }
  const auto budget = std::min(config.maximumAdaptiveLocations,
    (config.maximumSamples - result.scannedIntervals) / (verticalIntervals * 2));

  if (budget == 0) {
    return;
  }
  using Cell = std::pair<std::size_t, std::size_t>;
  std::map<Cell, std::vector<Vec3>> covered;
  const auto cellOf = [&](Vec3 v) -> Cell {
    return {
      static_cast<std::size_t>(std::floor((v.x - world.mins.x) / config.adaptiveMinimumSpacing)),
      static_cast<std::size_t>(std::floor((v.y - world.mins.y) / config.adaptiveMinimumSpacing))
    };
  };
  const auto record = [&](Vec3 point) {
    covered[cellOf(point)].push_back(point);
  };
  const auto isCovered = [&](Vec3 point) {
    const auto cell = cellOf(point);

    for (std::size_t dx = 0; dx < 3; ++dx) {
      for (std::size_t dy = 0; dy < 3; ++dy) {
        if ((dx == 0 && cell.first == 0) || (dy == 0 && cell.second == 0)) {
          continue;
        }
        const Cell neighbor {
          cell.first + dx - 1,
          cell.second + dy - 1
        };
        const auto it = covered.find(neighbor);

        if (it == covered.end()) {
          continue;
        }
        for (const auto &existing : it->second) {
          if (std::hypot(existing.x - point.x, existing.y - point.y) < config.adaptiveMinimumSpacing &&
              std::abs(existing.z - point.z) <= config.surfaceHeightTolerance * 2) {
            return true;
          }
        }
      }
    }

    return false;
  };

  for (const auto &candidate : result.candidates) {
    record(candidate.position);
  }

  for (const auto &mesh : map.meshes) {
    if (mesh.id != world.id) {
      continue;
    }
    for (const auto &triangle : mesh.triangles) {
      const auto a = mesh.vertices[triangle.a];
      const auto b = mesh.vertices[triangle.b];
      const auto c = mesh.vertices[triangle.c];
      const Vec3 ab { b.x - a.x, b.y - a.y, b.z - a.z };
      const Vec3 ac { c.x - a.x, c.y - a.y, c.z - a.z };
      const Vec3 cross {
        ab.y * ac.z - ab.z * ac.y,
        ab.z * ac.x - ab.x * ac.z,
        ab.x * ac.y - ab.y * ac.x
      };
      const double planarAreaTwice = std::abs(cross.z);
      const double crossLength = std::hypot(std::hypot(cross.x, cross.y), cross.z);

      if (crossLength <= 0 || std::abs(cross.z) / crossLength < config.minimumUpNormal) {
        continue;
      }
      const Vec3 vertices[] { a, b, c };
      std::size_t longest = 0;
      double longestLength = 0;

      for (std::size_t i = 0; i < 3; ++i) {
        const auto &p = vertices[i];
        const auto &q = vertices[(i + 1) % 3];
        const double length = std::hypot(q.x - p.x, q.y - p.y);

        if (length > longestLength) {
          longest = i;
          longestLength = length;
        }
      }

      if (longestLength == 0 || planarAreaTwice / longestLength > config.adaptiveThinSurfaceWidth) {
        continue;
      }
      ++result.adaptiveThinTriangles;
      const auto &p = vertices[longest];
      const auto &q = vertices[(longest + 1) % 3];
      const auto &inset = vertices[(longest + 2) % 3];
      const auto segments = std::min<std::size_t>(16,
        std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil(longestLength / config.adaptiveSampleSpacing))));

      for (std::size_t i = 0; i < segments && result.adaptiveLocations < budget; ++i) {
        const double t = (static_cast<double>(i) + 0.5) / static_cast<double>(segments);
        const Vec3 point {
          (p.x * (1 - t) + q.x * t) * 0.75 + inset.x * 0.25,
          (p.y * (1 - t) + q.y * t) * 0.75 + inset.y * 0.25,
          (p.z * (1 - t) + q.z * t) * 0.75 + inset.z * 0.25
        };

        if (!finite(point) || point.x < world.mins.x || point.x >= world.maxs.x ||
            point.y < world.mins.y || point.y >= world.maxs.y || isCovered(point)) {
          continue;
        }
        const auto column = std::min(result.columns - 1,
          static_cast<std::size_t>(std::floor((point.x - world.mins.x) / config.cellSize)));
        const auto row = std::min(result.rows - 1,
          static_cast<std::size_t>(std::floor((point.y - world.mins.y) / config.cellSize)));
        Result sampled;
        ++result.adaptiveLocations;
        scanColumn(context, sampled, point.x, point.y, column, row);
        result.scannedIntervals += sampled.scannedIntervals;

        for (const auto &candidate : sampled.candidates) {
          if (isCovered(candidate.position)) {
            continue;
          }
          record(candidate.position);
          result.candidates.push_back(candidate);
          ++result.adaptiveCandidates;
        }
      }

      if (result.adaptiveLocations >= budget) {
        return;
      }
    }
  }
}

// Probe inside exposed walkable world boundaries, including platforms over deep gaps.
// A mesh edge is exposed if only one upward-facing triangle uses it.
void sampleLedgeWorldSurfaces(
  const MapModel &map,
  const ScanContext &context,
  Result &result
) {
  using EdgeKey = std::pair<std::uint32_t, std::uint32_t>;
  struct Edge {
    std::size_t count {};
    Vec3 opposite {};
  };
  const auto &config = context.config;
  const auto &world = context.world;
  const auto intervals = count(context.top - context.bottom, config.verticalStep);

  if (intervals > config.maximumSamples / 2 || result.scannedIntervals >= config.maximumSamples) {
    return;
  }
  const auto budget = std::min(config.maximumLedgeLocations,
    (config.maximumSamples - result.scannedIntervals) / (intervals * 2));

  if (budget == 0) {
    return;
  }
  const double spacing = config.adaptiveMinimumSpacing;
  using Cell = std::pair<std::int64_t, std::int64_t>;
  std::map<Cell, std::vector<Vec3>> coverage;
  const auto cellOf = [spacing](Vec3 pos) {
    return Cell {
      static_cast<std::int64_t>(std::floor(pos.x / spacing)),
      static_cast<std::int64_t>(std::floor(pos.y / spacing))
    };
  };
  const auto record = [&](Vec3 pos) {
    coverage[cellOf(pos)].push_back(pos);
  };
  const auto covered = [&](Vec3 pos) {
    const auto cell = cellOf(pos);

    for (std::int64_t dy = -1; dy <= 1; ++dy) {
      for (std::int64_t dx = -1; dx <= 1; ++dx) {
        const auto it = coverage.find({ cell.first + dx, cell.second + dy });

        if (it == coverage.end()) {
          continue;
        }
        for (const auto &other : it->second) {
          if (std::hypot(pos.x - other.x, pos.y - other.y) < spacing &&
              std::abs(pos.z - other.z) <= config.surfaceHeightTolerance * 2) {
            return true;
          }
        }
      }
    }

    return false;
  };
  for (const auto &candidate : result.candidates) {
    record(candidate.position);
  }
  for (const auto &mesh : map.meshes) {
    if (mesh.id != world.id) {
      continue;
    }
    std::map<EdgeKey, Edge> edges;

    for (const auto &triangle : mesh.triangles) {
      const auto a = mesh.vertices[triangle.a];
      const auto b = mesh.vertices[triangle.b];
      const auto c = mesh.vertices[triangle.c];
      const Vec3 ab { b.x - a.x, b.y - a.y, b.z - a.z };
      const Vec3 ac { c.x - a.x, c.y - a.y, c.z - a.z };
      const Vec3 normal {
        ab.y * ac.z - ab.z * ac.y,
        ab.z * ac.x - ab.x * ac.z,
        ab.x * ac.y - ab.y * ac.x
      };
      const double length = std::hypot(std::hypot(normal.x, normal.y), normal.z);

      if (length <= 0 || std::abs(normal.z) / length < config.minimumUpNormal) {
        continue;
      }
      const std::uint32_t ids[] { triangle.a, triangle.b, triangle.c };

      for (std::size_t i = 0; i < 3; ++i) {
        const EdgeKey key = std::minmax(ids[i], ids[(i + 1) % 3]);
        auto &edge = edges[key];
        ++edge.count;
        edge.opposite = mesh.vertices[ids[(i + 2) % 3]];
      }
    }

    for (const auto &[key, edge] : edges) {
      if (result.ledgeLocations >= budget) {
        return;
      }

      if (edge.count != 1) {
        continue;
      }
      const auto a = mesh.vertices[key.first];
      const auto b = mesh.vertices[key.second];
      const auto p = edge.opposite;
      const double length = std::hypot(b.x - a.x, b.y - a.y);
      const double height = length > 0 ?
        std::abs((b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x)) / length : 0;

      if (height <= 0) {
        continue;
      }
      ++result.ledgeEdges;
      const auto segments = std::min<std::size_t>(16, std::max<std::size_t>(1,
        static_cast<std::size_t>(std::ceil(length / config.adaptiveSampleSpacing))));
      const double inset = std::min(0.5, config.ledgeInset / height);

      for (std::size_t i = 0; i < segments && result.ledgeLocations < budget; ++i) {
        const double t = (static_cast<double>(i) + 0.5) / static_cast<double>(segments);
        const Vec3 midpoint {
          a.x + (b.x - a.x) * t,
          a.y + (b.y - a.y) * t,
          a.z + (b.z - a.z) * t
        };
        const Vec3 pos {
          midpoint.x + (p.x - midpoint.x) * inset,
          midpoint.y + (p.y - midpoint.y) * inset,
          midpoint.z + (p.z - midpoint.z) * inset
        };

        if (!finite(pos) || pos.x < world.mins.x || pos.x >= world.maxs.x ||
            pos.y < world.mins.y || pos.y >= world.maxs.y || covered(pos)) {
          continue;
        }
        const auto col = std::min(result.columns - 1,
          static_cast<std::size_t>(std::floor((pos.x - world.mins.x) / config.cellSize)));
        const auto row = std::min(result.rows - 1,
          static_cast<std::size_t>(std::floor((pos.y - world.mins.y) / config.cellSize)));
        Result sampled;
        scanColumn(context, sampled, pos.x, pos.y, col, row);
        ++result.ledgeLocations;
        result.scannedIntervals += sampled.scannedIntervals;

        for (const auto &candidate : sampled.candidates) {
          if (covered(candidate.position)) {
            continue;
          }
          record(candidate.position);
          result.candidates.push_back(candidate);
          ++result.ledgeCandidates;
        }
      }
    }
  }
}

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
      !std::isfinite(config.adaptiveThinSurfaceWidth) || config.adaptiveThinSurfaceWidth <= 0 ||
      !std::isfinite(config.adaptiveSampleSpacing) || config.adaptiveSampleSpacing <= 0 ||
      !std::isfinite(config.adaptiveMinimumSpacing) || config.adaptiveMinimumSpacing <= 0 ||
      !std::isfinite(config.ledgeInset) || config.ledgeInset <= 0 ||
      config.maximumLedgeLocations == 0 ||
      config.maximumAdaptiveLocations == 0 ||
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

  sampleThinWorldSurfaces(map, context, result);
  sampleLedgeWorldSurfaces(map, context, result);

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
    { "schema_version", "0.3.2" },
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
        { "adaptive_thin_surface_width", result.config.adaptiveThinSurfaceWidth },
        { "adaptive_sample_spacing", result.config.adaptiveSampleSpacing },
        { "adaptive_minimum_spacing", result.config.adaptiveMinimumSpacing },
        { "maximum_adaptive_locations", number(result.config.maximumAdaptiveLocations) },
        { "ledge_inset", result.config.ledgeInset },
        { "maximum_ledge_locations", number(result.config.maximumLedgeLocations) },
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
    { "adaptive_thin_triangles", number(result.adaptiveThinTriangles) },
    { "adaptive_locations", number(result.adaptiveLocations) },
    { "adaptive_candidates", number(result.adaptiveCandidates) },
    { "ledge_edges", number(result.ledgeEdges) },
    { "ledge_locations", number(result.ledgeLocations) },
    { "ledge_candidates", number(result.ledgeCandidates) },
    { "candidates", std::move(candidates) }
  }) + "\n";
}

} // namespace aipb::map::heightfield
