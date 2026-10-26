#include <aipb/map/navigation/candidates.h>

#include "json.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace aipb::map::navigation {
namespace {

using io::Json;

bool finite(Vec3 v) {
  return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

bool isSpawn(const MapEntity &entity) {
  return entity.type == "info_player_start" ||
         entity.type == "info_player_deathmatch" ||
         entity.type == "info_vip_start";
}

Json count(std::size_t value) {
  return static_cast<double>(value);
}

Json vectorJson(Vec3 value) {
  return Json::Array { value.x, value.y, value.z };
}

Json optionalId(const std::optional<std::size_t> &value) {
  if (value) {
    return count(*value);
  }

  return Json {};
}

Json candidateJson(const Candidate &candidate) {
  return Json::Object {
    { "id", count(candidate.id) },
    { "heightfield_index", count(candidate.heightfieldIndex) },
    { "source_column", count(candidate.column) },
    { "source_row", count(candidate.row) },
    { "position", vectorJson(candidate.position) },
    { "floor_normal", vectorJson(candidate.floorNormal) },
    { "hull_mask", count(candidate.hullMask) },
    { "standing_free", candidate.standingFree },
    { "crouching_free", candidate.crouchingFree }
  };
}

Json spawnJson(const SpawnCoverage &spawn) {
  return Json::Object {
    { "entity_id", std::to_string(spawn.entityId) },
    { "entity_type", spawn.entityType },
    { "origin", vectorJson(spawn.origin) },
    { "nearest_candidate_id", optionalId(spawn.nearestCandidateId) },
    { "nearest_planar_distance", spawn.nearestPlanarDistance },
    { "nearest_vertical_distance", spawn.nearestVerticalDistance },
    { "covered_candidate_id", optionalId(spawn.coveredCandidateId) }
  };
}

} // namespace

CandidateReport generateCandidates(
  const MapModel &map,
  const heightfield::Result &heightfield,
  CandidateConfig config
) {
  if (!std::isfinite(config.spawnPlanarRadius) || config.spawnPlanarRadius <= 0 ||
      !std::isfinite(config.spawnVerticalTolerance) || config.spawnVerticalTolerance < 0 ||
      config.maximumCandidates == 0) {
    throw std::invalid_argument("invalid navigation candidate configuration");
  }

  if (heightfield.mapName != map.name || heightfield.sourceSha256 != map.sourceSha256) {
    throw std::invalid_argument("heightfield map identity does not match source map");
  }

  if (heightfield.columns == 0 || heightfield.columns > 16384 ||
      heightfield.rows == 0 || heightfield.rows > 16384 / heightfield.columns ||
      heightfield.candidates.size() > config.maximumCandidates) {
    throw std::invalid_argument("heightfield candidate or grid limit exceeded");
  }

  CandidateReport report {
    .mapName = map.name,
    .sourceSha256 = map.sourceSha256,
    .artifactSha256 = heightfield.artifactSha256,
    .config = config,
    .gridColumns = heightfield.columns,
    .gridRows = heightfield.rows,
    .occupiedColumns = 0,
    .multiLayerColumns = 0,
    .standingOnly = 0,
    .crouchingOnly = 0,
    .bothStances = 0,
    .sloped = 0,
    .spawnCount = 0,
    .spawnsWithinPlanarRadius = 0,
    .coveredSpawns = 0,
    .candidates = {},
    .spawns = {}
  };
  report.candidates.reserve(heightfield.candidates.size());
  std::vector<std::size_t> perColumn(heightfield.columns * heightfield.rows, 0);

  for (std::size_t i = 0; i < heightfield.candidates.size(); ++i) {
    const auto &source = heightfield.candidates[i];
    constexpr std::uint32_t supportedMask = (1u << 1) | (1u << 3);

    if (source.column >= heightfield.columns || source.row >= heightfield.rows ||
        source.hullMask == 0 || (source.hullMask & ~supportedMask) != 0 ||
        !finite(source.position) || !finite(source.floorNormal) ||
        source.floorNormal.z < heightfield.config.minimumUpNormal ||
        (!source.standingFree && !source.crouchingFree)) {
      throw std::invalid_argument("invalid heightfield candidate metadata");
    }
    const auto index = source.row * heightfield.columns + source.column;
    ++perColumn[index];
    report.standingOnly += source.standingFree && !source.crouchingFree;
    report.crouchingOnly += !source.standingFree && source.crouchingFree;
    report.bothStances += source.standingFree && source.crouchingFree;
    report.sloped += source.floorNormal.z < 0.9999;
    report.candidates.push_back({
      .id = i + 1,
      .heightfieldIndex = i,
      .column = source.column,
      .row = source.row,
      .position = source.position,
      .floorNormal = source.floorNormal,
      .hullMask = source.hullMask,
      .standingFree = source.standingFree,
      .crouchingFree = source.crouchingFree
    });
  }

  for (const auto total : perColumn) {
    report.occupiedColumns += total != 0;
    report.multiLayerColumns += total > 1;
  }

  for (const auto &entity : map.entities) {
    if (!isSpawn(entity)) {
      continue;
    }

    if (!finite(entity.origin)) {
      throw std::invalid_argument("spawn origin must be finite");
    }

    SpawnCoverage entry {
      .entityId = entity.id,
      .entityType = entity.type,
      .origin = entity.origin,
      .nearestCandidateId = {},
      .nearestPlanarDistance = -1.0,
      .nearestVerticalDistance = -1.0,
      .coveredCandidateId = {}
    };
    double closestCoveredPlanar = std::numeric_limits<double>::infinity();
    double closestCoveredVertical = std::numeric_limits<double>::infinity();

    for (const auto &candidate : report.candidates) {
      const double planar = std::hypot(candidate.position.x - entity.origin.x,
                                       candidate.position.y - entity.origin.y);
      const double vertical = std::abs(candidate.position.z - entity.origin.z);

      if (!entry.nearestCandidateId || planar < entry.nearestPlanarDistance ||
          (planar == entry.nearestPlanarDistance && vertical < entry.nearestVerticalDistance)) {
        entry.nearestCandidateId = candidate.id;
        entry.nearestPlanarDistance = planar;
        entry.nearestVerticalDistance = vertical;
      }

      if (planar <= config.spawnPlanarRadius && vertical <= config.spawnVerticalTolerance &&
          (planar < closestCoveredPlanar ||
           (planar == closestCoveredPlanar && vertical < closestCoveredVertical))) {
        entry.coveredCandidateId = candidate.id;
        closestCoveredPlanar = planar;
        closestCoveredVertical = vertical;
      }
    }
    ++report.spawnCount;
    report.spawnsWithinPlanarRadius += entry.nearestCandidateId &&
                                      entry.nearestPlanarDistance <= config.spawnPlanarRadius;
    report.coveredSpawns += entry.coveredCandidateId.has_value();
    report.spawns.push_back(std::move(entry));
  }

  return report;
}

std::string candidatesJson(const CandidateReport &report) {
  Json::Array candidates;
  Json::Array spawns;
  candidates.reserve(report.candidates.size());
  spawns.reserve(report.spawns.size());

  for (const auto &candidate : report.candidates) {
    candidates.push_back(candidateJson(candidate));
  }

  for (const auto &spawn : report.spawns) {
    spawns.push_back(spawnJson(spawn));
  }

  return io::writeJson(Json::Object {
    { "format", "aipb-navigation-candidates" },
    { "schema_version", "0.1.0" },
    { "source_heightfield_schema", "0.3.0" },
    { "map_name", report.mapName },
    { "source_sha256", report.sourceSha256 },
    { "artifact_sha256", report.artifactSha256 },
    { "config", Json::Object {
        { "spawn_planar_radius", report.config.spawnPlanarRadius },
        { "spawn_vertical_tolerance", report.config.spawnVerticalTolerance },
        { "maximum_candidates", count(report.config.maximumCandidates) }
      } },
    { "grid_columns", count(report.gridColumns) },
    { "grid_rows", count(report.gridRows) },
    { "occupied_columns", count(report.occupiedColumns) },
    { "multi_layer_columns", count(report.multiLayerColumns) },
    { "candidate_count", count(report.candidates.size()) },
    { "standing_only", count(report.standingOnly) },
    { "crouching_only", count(report.crouchingOnly) },
    { "both_stances", count(report.bothStances) },
    { "sloped", count(report.sloped) },
    { "spawn_count", count(report.spawnCount) },
    { "spawns_within_planar_radius", count(report.spawnsWithinPlanarRadius) },
    { "covered_spawns", count(report.coveredSpawns) },
    { "candidates", std::move(candidates) },
    { "spawns", std::move(spawns) }
  }) + "\n";
}

} // namespace aipb::map::navigation
