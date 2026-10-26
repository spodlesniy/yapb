#include <aipb/map/heightfield/builder.h>
#include <aipb/map/collision/queries.h>

#include "json.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace aipb::map::heightfield {
namespace {

using io::Json;

std::size_t cellCount(
  double extent,
  double step
) {
  const double cells = std::ceil(extent / step);

  if (!std::isfinite(cells) || cells < 1 || cells > static_cast<double>(std::numeric_limits<std::size_t>::max())) {
    throw std::invalid_argument("heightfield dimensions exceed supported range");
  }

  return static_cast<std::size_t>(cells);
}

Json vectorJson(const Vec3 &point) {
  return Json::Array { point.x, point.y, point.z };
}

Json countJson(std::size_t count) {
  return Json(static_cast<double>(count));
}

Json candidateJson(const Candidate &candidate) {
  return Json::Object {
    { "column", countJson(candidate.column) },
    { "row", countJson(candidate.row) },
    { "hull", countJson(candidate.hull) },
    { "position", vectorJson(candidate.position) },
    { "floor_normal", vectorJson(candidate.floorNormal) },
    { "standing_free", candidate.standingFree },
    { "crouching_free", candidate.crouchingFree }
  };
}

} // namespace

Result build(
  const MapModel &map,
  std::string artifactSha256,
  Config config
) {
  if (!std::isfinite(config.cellSize) || config.cellSize <= 0 ||
      !std::isfinite(config.verticalStep) || config.verticalStep <= 0 ||
      !std::isfinite(config.minimumUpNormal) || config.minimumUpNormal <= 0 ||
      config.minimumUpNormal > 1 || config.maximumColumns == 0 || config.maximumVerticalSteps == 0) {
    throw std::invalid_argument("invalid heightfield configuration");
  }
  collision::CollisionQueries queries(map);
  const auto &world = map.partition->models.front();
  const auto extentX = world.maxs.x - world.mins.x;
  const auto extentY = world.maxs.y - world.mins.y;
  const auto extentZ = world.maxs.z - world.mins.z;

  if (!std::isfinite(extentX) || !std::isfinite(extentY) || !std::isfinite(extentZ) ||
      extentX <= 0 || extentY <= 0 || extentZ <= 0) {
    throw std::invalid_argument("heightfield world bounds must have positive finite extents");
  }
  const auto columns = cellCount(extentX, config.cellSize);
  const auto rows = cellCount(extentY, config.cellSize);
  const auto verticalSteps = cellCount(extentZ, config.verticalStep) + 2;

  if (columns > config.maximumColumns || rows > config.maximumColumns / columns ||
      verticalSteps > config.maximumVerticalSteps) {
    throw std::invalid_argument("heightfield sampling budget exceeded; increase cell size or vertical step");
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
    .rejectedImpacts = 0,
    .columnsWithCandidates = 0,
    .multiLayerColumns = 0,
    .maximumLayers = 0,
    .candidates = {}
  };
  const auto top = world.maxs.z + config.verticalStep;
  const auto bottom = world.mins.z - config.verticalStep;

  if (!std::isfinite(top) || !std::isfinite(bottom) || !(top > bottom)) {
    throw std::invalid_argument("invalid extended heightfield bounds");
  }

  for (std::size_t row = 0; row < rows; ++row) {
    const auto lowY = world.mins.y + static_cast<double>(row) * config.cellSize;
    const auto highY = std::min(world.maxs.y, lowY + config.cellSize);
    const auto y = lowY + (highY - lowY) * 0.5;

    for (std::size_t column = 0; column < columns; ++column) {
      const auto lowX = world.mins.x + static_cast<double>(column) * config.cellSize;
      const auto highX = std::min(world.maxs.x, lowX + config.cellSize);
      const auto x = lowX + (highX - lowX) * 0.5;
      std::size_t maximumColumnLayers = 0;
      bool anyCandidate = false;

      for (const std::size_t hull : std::array<std::size_t, 2> { 1, 3 }) {
        std::size_t hullLayers = 0;
        double upper = top;

        while (upper > bottom) {
          const auto lower = std::max(bottom, upper - config.verticalStep);
          const Vec3 highPoint { x, y, upper };
          const Vec3 lowPoint { x, y, lower };
          ++result.scannedIntervals;
          const auto upperContents = queries.pointContents(world.id, hull, highPoint);
          const auto lowerContents = queries.pointContents(world.id, hull, lowPoint);

          if (upperContents != collision::kContentsSolid && lowerContents == collision::kContentsSolid) {
            const auto hit = queries.traceSegment(world.id, hull, highPoint, lowPoint);

            if (!hit.startSolid && hit.impactPlane && hit.impactPlane->normal.z >= config.minimumUpNormal) {
              const auto position = hit.endPosition;
              result.candidates.push_back({
                column,
                row,
                hull,
                position,
                hit.impactPlane->normal,
                queries.canOccupy(world.id, collision::PlayerStance::Standing, position),
                queries.canOccupy(world.id, collision::PlayerStance::Crouching, position)
              });
              ++hullLayers;
              ++result.floorHits;
              anyCandidate = true;
            } else {
              ++result.rejectedImpacts;
            }
          }
          upper = lower;
        }
        maximumColumnLayers = std::max(maximumColumnLayers, hullLayers);
      }

      if (anyCandidate) {
        ++result.columnsWithCandidates;
      }

      if (maximumColumnLayers > 1) {
        ++result.multiLayerColumns;
      }
      result.maximumLayers = std::max(result.maximumLayers, maximumColumnLayers);
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
    { "schema_version", "0.1.0" },
    { "map_name", result.mapName },
    { "source_sha256", result.sourceSha256 },
    { "artifact_sha256", result.artifactSha256 },
    { "config", Json::Object {
        { "cell_size", result.config.cellSize },
        { "vertical_step", result.config.verticalStep },
        { "minimum_up_normal", result.config.minimumUpNormal },
        { "maximum_columns", countJson(result.config.maximumColumns) },
        { "maximum_vertical_steps", countJson(result.config.maximumVerticalSteps) }
      } },
    { "columns", countJson(result.columns) },
    { "rows", countJson(result.rows) },
    { "scanned_intervals", countJson(result.scannedIntervals) },
    { "floor_hits", countJson(result.floorHits) },
    { "rejected_impacts", countJson(result.rejectedImpacts) },
    { "columns_with_candidates", countJson(result.columnsWithCandidates) },
    { "multi_layer_columns", countJson(result.multiLayerColumns) },
    { "maximum_layers", countJson(result.maximumLayers) },
    { "candidates", std::move(candidates) }
  }) + "\n";
}

} // namespace aipb::map::heightfield
