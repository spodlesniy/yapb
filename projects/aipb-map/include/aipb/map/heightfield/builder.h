#pragma once

#include <aipb/map/model.h>

#include <cstddef>
#include <string>
#include <vector>

namespace aipb::map::heightfield {

struct Config {
  double cellSize { 128.0 };
  double verticalStep { 32.0 };
  double minimumUpNormal { 0.7 };
  std::size_t maximumColumns { 16384 };
  std::size_t maximumVerticalSteps { 512 };
};

struct Candidate {
  std::size_t column {};
  std::size_t row {};
  std::size_t hull {};
  Vec3 position {};
  Vec3 floorNormal {};
  bool standingFree {};
  bool crouchingFree {};
};

struct Result {
  std::string mapName;
  std::string sourceSha256;
  std::string artifactSha256;
  Config config {};
  std::size_t columns {};
  std::size_t rows {};
  std::size_t scannedIntervals {};
  std::size_t floorHits {};
  std::size_t rejectedImpacts {};
  std::size_t columnsWithCandidates {};
  std::size_t multiLayerColumns {};
  std::size_t maximumLayers {};
  std::vector<Candidate> candidates;
};

// Static BSP world-only origins: not navigation links or proof of walkability.
Result build(
  const MapModel &map,
  std::string artifactSha256 = {},
  Config config = {}
);

// Stable diagnostic output, independent of the map artifact schema.
std::string toJson(const Result &result);

} // namespace aipb::map::heightfield
