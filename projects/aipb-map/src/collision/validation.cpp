#include <aipb/map/collision/validation.h>
#include <aipb/map/collision/queries.h>

#include "json.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace aipb::map::collision {
namespace {

using io::Json;

Vec3 midpoint(
  Vec3 a,
  Vec3 b
) {
  return { (a.x + b.x) * 0.5, (a.y + b.y) * 0.5, (a.z + b.z) * 0.5 };
}

void violation(
  ValidationReport &report,
  ObjectId id,
  std::size_t hull,
  const char *reason
) {
  ++report.violations;

  if (report.examples.size() < 20) {
    report.examples.push_back("model " + std::to_string(id) + ", hull " + std::to_string(hull) + ": " + reason);
  }
}

Json encode(const SampleStatistics &value) {
  return Json::Object {
    { "scope", value.world ? "world" : "brush" },
    { "hull", static_cast<double>(value.hull) },
    { "models", static_cast<double>(value.models) },
    { "points", static_cast<double>(value.points) },
    { "solid", static_cast<double>(value.solid) },
    { "empty", static_cast<double>(value.empty) },
    { "other", static_cast<double>(value.other) },
    { "stationary_traces", static_cast<double>(value.stationaryTraces) },
    { "segment_traces", static_cast<double>(value.segmentTraces) },
    { "clear_segments", static_cast<double>(value.clearSegments) },
    { "solid_starts", static_cast<double>(value.solidStarts) },
    { "impacts", static_cast<double>(value.impacts) }
  };
}

struct ProbeDirection {
  const char *name;
  Vec3 offset;
};

constexpr std::array<ProbeDirection, 6> kSpawnDirections {
  ProbeDirection { "x_positive", { 32, 0, 0 } },
  ProbeDirection { "x_negative", { -32, 0, 0 } },
  ProbeDirection { "y_positive", { 0, 32, 0 } },
  ProbeDirection { "y_negative", { 0, -32, 0 } },
  ProbeDirection { "down", { 0, 0, -96 } },
  ProbeDirection { "up", { 0, 0, 96 } }
};

bool spawnEntity(const MapEntity &entity) {
  return entity.type == "info_player_start" || entity.type == "info_player_deathmatch" ||
         entity.type == "info_vip_start";
}

SpawnProbeReport probeSpawns(
  const MapModel &map,
  const CollisionQueries &queries
) {
  SpawnProbeReport report;

  for (std::size_t hull = 0; hull < 4; ++hull) {
    SpawnHullMetrics metrics;
    metrics.hull = hull;
    report.hulls.push_back(metrics);
  }
  const auto world = map.partition->models.front().id;

  for (const auto &entity : map.entities) {
    if (!spawnEntity(entity)) {
      continue;
    }
    const bool standingBlocked = queries.pointContents(world, 1, entity.origin) == kContentsSolid;
    const bool crouchingBlocked = queries.pointContents(world, 3, entity.origin) == kContentsSolid;
    report.standingBlockedCrouchingFree += standingBlocked && !crouchingBlocked;
    report.standingFreeCrouchingBlocked += !standingBlocked && crouchingBlocked;

    for (std::size_t hull = 0; hull < 4; ++hull) {
      auto &metrics = report.hulls[hull];
      SpawnHullSample sample;
      sample.entityId = entity.id;
      sample.entityType = entity.type;
      sample.origin = entity.origin;
      sample.hull = hull;
      sample.contents = queries.pointContents(world, hull, entity.origin);
      ++metrics.origins;
      metrics.solidOrigins += sample.contents == kContentsSolid;
      metrics.emptyOrigins += sample.contents == kContentsEmpty;
      metrics.otherOrigins += sample.contents != kContentsSolid && sample.contents != kContentsEmpty;

      for (std::size_t direction = 0; direction < kSpawnDirections.size(); ++direction) {
        const auto &probe = kSpawnDirections[direction];
        const Vec3 end {
          entity.origin.x + probe.offset.x,
          entity.origin.y + probe.offset.y,
          entity.origin.z + probe.offset.z
        };
        const auto trace = queries.traceSegment(world, hull, entity.origin, end);
        sample.traces.push_back({ probe.name, trace });

        if (direction < 4) {
          metrics.horizontalStartSolid += trace.startSolid;

          if (!trace.startSolid && trace.impactPlane) {
            ++metrics.horizontalImpacts;
          } else if (!trace.startSolid) {
            ++metrics.horizontalClear;
          }
        } else if (direction == 4) {
          metrics.downwardStartSolid += trace.startSolid;
          metrics.floorHits += !trace.startSolid && trace.impactPlane && trace.impactPlane->normal.z > 0;
        } else {
          metrics.upwardStartSolid += trace.startSolid;
          metrics.ceilingHits += !trace.startSolid && trace.impactPlane && trace.impactPlane->normal.z < 0;
        }
      }
      report.samples.push_back(std::move(sample));
    }
  }

  return report;
}

Json encode(const Vec3 &point) {
  return Json::Array { point.x, point.y, point.z };
}

Json encode(const TraceResult &trace) {
  Json impact;

  if (trace.impactPlane) {
    impact = Json::Object {
      { "normal", encode(trace.impactPlane->normal) },
      { "distance", trace.impactPlane->distance },
      { "type", static_cast<double>(trace.impactPlane->type) }
    };
  }

  return Json::Object {
    { "fraction", trace.fraction },
    { "end", encode(trace.endPosition) },
    { "start_solid", trace.startSolid },
    { "all_solid", trace.allSolid },
    { "in_open", trace.inOpen },
    { "in_liquid", trace.inLiquid },
    { "impact_plane", std::move(impact) }
  };
}

Json encode(const SpawnHullMetrics &metrics) {
  return Json::Object {
    { "hull", static_cast<double>(metrics.hull) },
    { "origins", static_cast<double>(metrics.origins) },
    { "solid_origins", static_cast<double>(metrics.solidOrigins) },
    { "empty_origins", static_cast<double>(metrics.emptyOrigins) },
    { "other_origins", static_cast<double>(metrics.otherOrigins) },
    { "horizontal_clear", static_cast<double>(metrics.horizontalClear) },
    { "horizontal_impacts", static_cast<double>(metrics.horizontalImpacts) },
    { "horizontal_start_solid", static_cast<double>(metrics.horizontalStartSolid) },
    { "floor_hits", static_cast<double>(metrics.floorHits) },
    { "ceiling_hits", static_cast<double>(metrics.ceilingHits) },
    { "downward_start_solid", static_cast<double>(metrics.downwardStartSolid) },
    { "upward_start_solid", static_cast<double>(metrics.upwardStartSolid) }
  };
}

Json encode(const SpawnHullSample &sample) {
  Json::Array traces;

  for (const auto &probe : sample.traces) {
    traces.push_back(Json::Object {
      { "direction", probe.direction },
      { "trace", encode(probe.result) }
    });
  }

  return Json::Object {
    { "entity_id", std::to_string(sample.entityId) },
    { "entity_type", sample.entityType },
    { "origin", encode(sample.origin) },
    { "hull", static_cast<double>(sample.hull) },
    { "contents", static_cast<double>(sample.contents) },
    { "traces", std::move(traces) }
  };
}

Json encode(const SpawnProbeReport &report) {
  Json::Array hulls;
  Json::Array samples;

  for (const auto &metrics : report.hulls) {
    hulls.push_back(encode(metrics));
  }

  for (const auto &sample : report.samples) {
    samples.push_back(encode(sample));
  }

  return Json::Object {
    { "horizontal_distance", static_cast<double>(report.horizontalDistance) },
    { "vertical_distance", static_cast<double>(report.verticalDistance) },
    { "standing_blocked_crouching_free", static_cast<double>(report.standingBlockedCrouchingFree) },
    { "standing_free_crouching_blocked", static_cast<double>(report.standingFreeCrouchingBlocked) },
    { "hulls", std::move(hulls) },
    { "samples", std::move(samples) }
  };
}

} // namespace

ValidationReport validateQueries(
  const MapModel &map,
  std::string artifactSha256
) {
  CollisionQueries queries(map);
  ValidationReport report {
    .mapName = map.name,
    .sourceSha256 = map.sourceSha256,
    .artifactSha256 = std::move(artifactSha256),
    .modelCount = 0,
    .sampledSpawnOrigins = 0,
    .violations = 0,
    .samples = {},
    .examples = {},
    .spawnProbes = {}
  };
  const auto &models = map.partition->models;
  report.modelCount = models.size();
  std::array<std::array<SampleStatistics, 4>, 2> counters {};

  for (std::size_t scope = 0; scope < counters.size(); ++scope) {
    for (std::size_t hull = 0; hull < 4; ++hull) {
      counters[scope][hull].world = scope == 0;
      counters[scope][hull].hull = hull;
    }
  }

  for (const auto &model : models) {
    const auto center = midpoint(model.mins, model.maxs);
    const std::array<Vec3, 7> positions {
      center,
      model.mins,
      model.maxs,
      { model.mins.x, center.y, center.z },
      { model.maxs.x, center.y, center.z },
      { center.x, model.mins.y, center.z },
      { center.x, model.maxs.y, center.z }
    };

    for (std::size_t hull = 0; hull < 4; ++hull) {
      auto &metric = counters[model.id == 1 ? 0 : 1][hull];
      ++metric.models;

      for (const auto &point : positions) {
        const auto contents = queries.pointContents(model.id, hull, point);
        ++metric.points;
        metric.solid += contents == kContentsSolid;
        metric.empty += contents == kContentsEmpty;
        metric.other += contents != kContentsSolid && contents != kContentsEmpty;
        const auto still = queries.traceSegment(model.id, hull, point, point);
        ++metric.stationaryTraces;

        if ((contents == kContentsSolid) != still.startSolid ||
            (contents == kContentsSolid) != still.allSolid ||
            still.fraction != (contents == kContentsSolid ? 0.0 : 1.0)) {
          violation(report, model.id, hull, "stationary trace disagrees with point contents");
        }

        if (hull == 1 || hull == 3) {
          const auto stance = hull == 1 ? PlayerStance::Standing : PlayerStance::Crouching;

          if (queries.canOccupy(model.id, stance, point) != (contents != kContentsSolid)) {
            violation(report, model.id, hull, "player occupancy disagrees with point contents");
          }
        }
      }

      for (std::size_t i = 0; i + 1 < positions.size(); ++i) {
        const auto start = positions[i];
        const auto end = positions[i + 1];
        const auto trace = queries.traceSegment(model.id, hull, start, end);
        ++metric.segmentTraces;
        metric.solidStarts += trace.startSolid;
        metric.impacts += trace.impactPlane.has_value();

        if (!trace.startSolid && !trace.impactPlane) {
          ++metric.clearSegments;
        }

        if (!std::isfinite(trace.fraction) || trace.fraction < 0 || trace.fraction > 1 ||
            !std::isfinite(trace.endPosition.x) || !std::isfinite(trace.endPosition.y) ||
            !std::isfinite(trace.endPosition.z) ||
            trace.startSolid != (queries.pointContents(model.id, hull, start) == kContentsSolid)) {
          violation(report, model.id, hull, "invalid trace result");
        }

        if (hull == 1 || hull == 3) {
          const auto stance = hull == 1 ? PlayerStance::Standing : PlayerStance::Crouching;
          const auto expected = !trace.startSolid && !trace.impactPlane;

          if (queries.canTraverse(model.id, stance, start, end) != expected) {
            violation(report, model.id, hull, "player traversal disagrees with trace");
          }
        }
      }
    }
  }

  // Spawn origin probes are model-local world tests; no brush transform is inferred.
  if (!models.empty()) {
    const auto world = models.front().id;

    for (const auto &entity : map.entities) {
      if (entity.type != "info_player_start" && entity.type != "info_player_deathmatch" &&
          entity.type != "info_vip_start") {
        continue;
      }
      ++report.sampledSpawnOrigins;

      for (std::size_t hull = 0; hull < 4; ++hull) {
        auto &metric = counters[0][hull];
        const auto contents = queries.pointContents(world, hull, entity.origin);
        ++metric.points;
        metric.solid += contents == kContentsSolid;
        metric.empty += contents == kContentsEmpty;
        metric.other += contents != kContentsSolid && contents != kContentsEmpty;
      }
    }
  }

  report.spawnProbes = probeSpawns(map, queries);

  for (const auto &group : counters) {
    for (const auto &metric : group) {
      report.samples.push_back(metric);
    }
  }

  return report;
}

std::string validationJson(const ValidationReport &report) {
  Json::Array samples;
  Json::Array examples;

  for (const auto &entry : report.samples) {
    samples.push_back(encode(entry));
  }

  for (const auto &entry : report.examples) {
    examples.emplace_back(entry);
  }

  return io::writeJson(Json::Object {
    { "format", "aipb-collision-validation" },
    { "schema_version", "0.2.0" },
    { "map_name", report.mapName },
    { "source_sha256", report.sourceSha256 },
    { "artifact_sha256", report.artifactSha256 },
    { "model_count", static_cast<double>(report.modelCount) },
    { "spawn_origins", static_cast<double>(report.sampledSpawnOrigins) },
    { "violations", static_cast<double>(report.violations) },
    { "samples", std::move(samples) },
    { "examples", std::move(examples) },
    { "spawn_probes", encode(report.spawnProbes) }
  }) + "\n";
}

} // namespace aipb::map::collision
