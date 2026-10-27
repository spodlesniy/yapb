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

enum class SourceKind {
  WorldHeightfield,
  SpawnRefinement,
  StaticBrush
};

SourceKind sourceKind(const Candidate &candidate) {
  if (candidate.supportEntityId != 0) {
    if (candidate.refined || candidate.supportModelId == 0 ||
        candidate.sourceSpawnId != 0 || candidate.sampleIndex == 0) {
      throw std::invalid_argument("invalid static brush candidate provenance");
    }

    return SourceKind::StaticBrush;
  }

  if (candidate.refined) {
    if (candidate.sourceSpawnId == 0 || candidate.sampleIndex == 0) {
      throw std::invalid_argument("invalid spawn refinement candidate provenance");
    }

    return SourceKind::SpawnRefinement;
  }

  if (candidate.sourceSpawnId != 0 || candidate.sampleIndex != 0) {
    throw std::invalid_argument("invalid world heightfield candidate provenance");
  }

  return SourceKind::WorldHeightfield;
}

Json candidateJson(const Candidate &candidate) {
  const auto kind = sourceKind(candidate);
  const bool world = kind == SourceKind::WorldHeightfield;
  const bool sampled = kind != SourceKind::WorldHeightfield;
  const bool spawn = kind == SourceKind::SpawnRefinement;
  const bool brush = kind == SourceKind::StaticBrush;
  const char *label = world ? "world" : (spawn ? "spawn_refinement" : "static_brush");

  return Json::Object {
    { "id", count(candidate.id) },
    { "heightfield_index", world ? count(candidate.heightfieldIndex) : Json {} },
    { "refined", candidate.refined },
    { "source_spawn_id", spawn ? Json(std::to_string(candidate.sourceSpawnId)) : Json {} },
    { "sample_index", sampled ? count(candidate.sampleIndex) : Json {} },
    { "support_model_id", candidate.supportModelId ? Json(std::to_string(candidate.supportModelId)) : Json {} },
    { "support_entity_id", brush ? Json(std::to_string(candidate.supportEntityId)) : Json {} },
    { "support_kind", label },
    { "support_condition", candidate.conditionalSupport ? "until_trigger" : "fixed" },
    { "source_column", world ? count(candidate.column) : Json {} },
    { "source_row", world ? count(candidate.row) : Json {} },
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
    { "covered_candidate_id", optionalId(spawn.coveredCandidateId) },
    { "covered_64", optionalId(spawn.covered64) },
    { "covered_128", optionalId(spawn.covered128) },
    { "covered_256", optionalId(spawn.covered256) }
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
      config.maximumCandidates == 0 || !std::isfinite(config.refinementStep) ||
      config.refinementStep <= 0 || config.maximumRefinementLocations == 0 ||
      !std::isfinite(config.staticBrushStep) || config.staticBrushStep <= 0 ||
      config.maximumStaticBrushSamples == 0) {
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
    .covered64 = 0,
    .covered128 = 0,
    .covered256 = 0,
    .refinedSpawns = 0,
    .refinementLocations = 0,
    .refinementCandidates = 0,
    .staticBrushModels = 0,
    .staticBrushSamples = 0,
    .staticBrushCandidates = 0,
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
      .refined = false,
      .sourceSpawnId = 0,
      .sampleIndex = 0,
      .supportModelId = map.partition ? map.partition->models.front().id : 0,
      .supportEntityId = 0,
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
      .coveredCandidateId = {},
      .covered64 = {},
      .covered128 = {},
      .covered256 = {}
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
    for (const auto &candidate : report.candidates) {
      const double planar = std::hypot(candidate.position.x - entity.origin.x,
                                       candidate.position.y - entity.origin.y);
      const double vertical = std::abs(candidate.position.z - entity.origin.z);

      if (vertical <= config.spawnVerticalTolerance) {
        if (planar <= 64 && !entry.covered64) {
          entry.covered64 = candidate.id;
        }

        if (planar <= 128 && !entry.covered128) {
          entry.covered128 = candidate.id;
        }

        if (planar <= 256 && !entry.covered256) {
          entry.covered256 = candidate.id;
        }
      }
    }
    ++report.spawnCount;
    report.spawnsWithinPlanarRadius += entry.nearestCandidateId &&
                                      entry.nearestPlanarDistance <= config.spawnPlanarRadius;
    report.coveredSpawns += entry.coveredCandidateId.has_value();
    report.covered64 += entry.covered64.has_value();
    report.covered128 += entry.covered128.has_value();
    report.covered256 += entry.covered256.has_value();
    report.spawns.push_back(std::move(entry));
  }

  return report;
}

CandidateReport generateAdaptiveCandidates(
  const MapModel &map,
  const heightfield::Result &heightfield,
  CandidateConfig config
) {
  auto report = generateCandidates(map, heightfield, config);
  const auto &world = map.partition->models.front();
  std::vector<Vec3> points;
  std::vector<ObjectId> sourceIds;
  std::vector<std::size_t> sampleIds;
  const double extent = config.spawnPlanarRadius;
  const auto radiusSteps = static_cast<std::size_t>(std::ceil(extent / config.refinementStep));

  if (radiusSteps > 64) {
    throw std::invalid_argument("refinement step too small for bounded neighborhood");
  }

  for (const auto &spawn : report.spawns) {
    if (spawn.coveredCandidateId) {
      continue;
    }
    ++report.refinedSpawns;
    std::size_t sample = 0;

    for (std::size_t yi = 0; yi <= radiusSteps * 2; ++yi) {
      const double y = spawn.origin.y + (static_cast<double>(yi) - static_cast<double>(radiusSteps)) * config.refinementStep;

      for (std::size_t xi = 0; xi <= radiusSteps * 2; ++xi) {
        const double x = spawn.origin.x + (static_cast<double>(xi) - static_cast<double>(radiusSteps)) * config.refinementStep;
        ++sample;

        if (x < world.mins.x || x > world.maxs.x || y < world.mins.y || y > world.maxs.y ||
            std::hypot(x - spawn.origin.x, y - spawn.origin.y) > extent) {
          continue;
        }

        if (points.size() >= config.maximumRefinementLocations) {
          break;
        }
        points.push_back({ x, y, spawn.origin.z });
        sourceIds.push_back(spawn.entityId);
        sampleIds.push_back(sample);
      }

      if (points.size() >= config.maximumRefinementLocations) {
        break;
      }
    }
  }
  report.refinementLocations = points.size();
  const auto additional = heightfield::sampleLocations(map, points, heightfield.config);

  for (const auto &source : additional) {
    if (report.candidates.size() >= config.maximumCandidates) {
      break;
    }
    const auto i = source.column;
    const auto &point = points[i];
    const auto match = std::find_if(report.candidates.begin(), report.candidates.end(), [&](const Candidate &candidate) {
      return std::hypot(candidate.position.x - point.x, candidate.position.y - point.y) <= 1.0 &&
             std::abs(candidate.position.z - source.position.z) <= heightfield.config.mergeHeightTolerance;
    });

    if (match != report.candidates.end()) {
      continue;
    }
    report.candidates.push_back({
      .id = report.candidates.size() + 1,
      .heightfieldIndex = 0,
      .refined = true,
      .sourceSpawnId = sourceIds[i],
      .sampleIndex = sampleIds[i],
      .supportModelId = world.id,
      .supportEntityId = 0,
      .column = 0,
      .row = 0,
      .position = source.position,
      .floorNormal = source.floorNormal,
      .hullMask = source.hullMask,
      .standingFree = source.standingFree,
      .crouchingFree = source.crouchingFree
    });
    ++report.refinementCandidates;
    report.standingOnly += source.standingFree && !source.crouchingFree;
    report.crouchingOnly += !source.standingFree && source.crouchingFree;
    report.bothStances += source.standingFree && source.crouchingFree;
    report.sloped += source.floorNormal.z < 0.9999;
  }

  for (auto &spawn : report.spawns) {
    double bestCovered = std::numeric_limits<double>::infinity();
    double bestHeight = std::numeric_limits<double>::infinity();
    spawn.covered64.reset();
    spawn.covered128.reset();
    spawn.covered256.reset();

    for (const auto &candidate : report.candidates) {
      const double planar = std::hypot(candidate.position.x - spawn.origin.x,
                                       candidate.position.y - spawn.origin.y);
      const double vertical = std::abs(candidate.position.z - spawn.origin.z);

      if (!spawn.nearestCandidateId || planar < spawn.nearestPlanarDistance ||
          (planar == spawn.nearestPlanarDistance && vertical < spawn.nearestVerticalDistance)) {
        spawn.nearestCandidateId = candidate.id;
        spawn.nearestPlanarDistance = planar;
        spawn.nearestVerticalDistance = vertical;
      }

      if (vertical <= config.spawnVerticalTolerance) {
        if (planar <= 64 && !spawn.covered64) {
          spawn.covered64 = candidate.id;
        }

        if (planar <= 128 && !spawn.covered128) {
          spawn.covered128 = candidate.id;
        }

        if (planar <= 256 && !spawn.covered256) {
          spawn.covered256 = candidate.id;
        }

        if (planar <= config.spawnPlanarRadius &&
            (planar < bestCovered || (planar == bestCovered && vertical < bestHeight))) {
          spawn.coveredCandidateId = candidate.id;
          bestCovered = planar;
          bestHeight = vertical;
        }
      }
    }
  }
  report.coveredSpawns = 0;
  report.covered64 = 0;
  report.covered128 = 0;
  report.covered256 = 0;
  report.spawnsWithinPlanarRadius = 0;

  for (const auto &spawn : report.spawns) {
    report.coveredSpawns += spawn.coveredCandidateId.has_value();
    report.covered64 += spawn.covered64.has_value();
    report.covered128 += spawn.covered128.has_value();
    report.covered256 += spawn.covered256.has_value();
    report.spawnsWithinPlanarRadius += spawn.nearestCandidateId &&
                                      spawn.nearestPlanarDistance <= config.spawnPlanarRadius;
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
    { "schema_version", "0.3.2" },
    { "source_heightfield_schema", "0.3.2" },
    { "map_name", report.mapName },
    { "source_sha256", report.sourceSha256 },
    { "artifact_sha256", report.artifactSha256 },
    { "config", Json::Object {
        { "spawn_planar_radius", report.config.spawnPlanarRadius },
        { "spawn_vertical_tolerance", report.config.spawnVerticalTolerance },
        { "maximum_candidates", count(report.config.maximumCandidates) },
        { "refinement_step", report.config.refinementStep },
        { "maximum_refinement_locations", count(report.config.maximumRefinementLocations) },
        { "static_brush_step", report.config.staticBrushStep },
        { "maximum_static_brush_samples", count(report.config.maximumStaticBrushSamples) }
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
    { "covered_64", count(report.covered64) },
    { "covered_128", count(report.covered128) },
    { "covered_256", count(report.covered256) },
    { "refined_spawns", count(report.refinedSpawns) },
    { "refinement_locations", count(report.refinementLocations) },
    { "refinement_candidates", count(report.refinementCandidates) },
    { "static_brush_models", count(report.staticBrushModels) },
    { "static_brush_samples", count(report.staticBrushSamples) },
    { "static_brush_candidates", count(report.staticBrushCandidates) },
    { "candidates", std::move(candidates) },
    { "spawns", std::move(spawns) }
  }) + "\n";
}

} // namespace aipb::map::navigation
