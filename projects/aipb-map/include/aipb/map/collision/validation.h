#pragma once

#include <aipb/map/collision/queries.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace aipb::map::collision {

struct SampleStatistics {
  bool world {};
  std::size_t hull {};
  std::size_t models {};
  std::size_t points {};
  std::size_t solid {};
  std::size_t empty {};
  std::size_t other {};
  std::size_t stationaryTraces {};
  std::size_t segmentTraces {};
  std::size_t clearSegments {};
  std::size_t solidStarts {};
  std::size_t impacts {};
};

struct DirectionalTrace {
  std::string direction;
  TraceResult result {};
};

struct SpawnHullSample {
  ObjectId entityId {};
  std::string entityType;
  Vec3 origin {};
  std::size_t hull {};
  std::int32_t contents {};
  std::vector<DirectionalTrace> traces;
};

struct SpawnHullMetrics {
  std::size_t hull {};
  std::size_t origins {};
  std::size_t solidOrigins {};
  std::size_t emptyOrigins {};
  std::size_t otherOrigins {};
  std::size_t horizontalClear {};
  std::size_t horizontalImpacts {};
  std::size_t horizontalStartSolid {};
  std::size_t floorHits {};
  std::size_t ceilingHits {};
  std::size_t downwardStartSolid {};
  std::size_t upwardStartSolid {};
};

struct SpawnProbeReport {
  std::size_t horizontalDistance { 32 };
  std::size_t verticalDistance { 96 };
  std::size_t standingBlockedCrouchingFree {};
  std::size_t standingFreeCrouchingBlocked {};
  std::vector<SpawnHullMetrics> hulls;
  std::vector<SpawnHullSample> samples;
};

struct ValidationReport {
  std::string mapName;
  std::string sourceSha256;
  std::string artifactSha256;
  std::size_t modelCount {};
  std::size_t sampledSpawnOrigins {};
  std::size_t violations {};
  std::vector<SampleStatistics> samples;
  std::vector<std::string> examples;
  SpawnProbeReport spawnProbes {};
};

// Deterministic consistency sampling, not verification against GoldSrc gameplay.
ValidationReport validateQueries(
  const MapModel &map,
  std::string artifactSha256 = {}
);

std::string validationJson(const ValidationReport &report);

} // namespace aipb::map::collision
