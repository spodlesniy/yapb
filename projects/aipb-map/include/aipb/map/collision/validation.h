#pragma once

#include <aipb/map/model.h>

#include <cstddef>
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

struct ValidationReport {
  std::string mapName;
  std::string sourceSha256;
  std::string artifactSha256;
  std::size_t modelCount {};
  std::size_t sampledSpawnOrigins {};
  std::size_t violations {};
  std::vector<SampleStatistics> samples;
  std::vector<std::string> examples;
};

// Deterministic consistency sampling, not verification against GoldSrc gameplay.
ValidationReport validateQueries(
  const MapModel &map,
  std::string artifactSha256 = {}
);

std::string validationJson(const ValidationReport &report);

} // namespace aipb::map::collision
