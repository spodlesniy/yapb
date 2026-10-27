#pragma once

#include <aipb/map/model.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace aipb::map::heightfield {

struct Config {
  double cellSize { 128.0 };
  double verticalStep { 32.0 };
  double minimumUpNormal { 0.7 };
  double mergeHeightTolerance { 1.0 };
  double clearanceLift { 0.5 };
  double surfaceHeightTolerance { 4.0 };
  double adaptiveThinSurfaceWidth { 64.0 };
  double adaptiveSampleSpacing { 64.0 };
  double adaptiveMinimumSpacing { 32.0 };
  std::size_t maximumAdaptiveLocations { 1024 };
  double ledgeInset { 24.0 };
  std::size_t maximumLedgeLocations { 512 };
  std::size_t maximumColumns { 16384 };
  std::size_t maximumVerticalSteps { 512 };
  std::size_t maximumSamples { 4000000 };
};

struct Candidate {
  std::size_t column {};
  std::size_t row {};
  std::uint32_t hullMask {};
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
  std::size_t thinIntervalHits {};
  std::size_t rejectedImpacts {};
  std::size_t rejectedInvalidHeight {};
  std::size_t rejectedOutsideWorld {};
  std::size_t rejectedOutsideRender {};
  std::size_t rejectedOwnSolid {};
  std::size_t rejectedUnverifiedPointHull {};
  std::size_t rejectedPointStartSolid {};
  std::size_t rejectedMissingPointImpact {};
  std::size_t rejectedPointNormal {};
  std::size_t rejectedSurfaceMismatch {};
  std::size_t rejectedNoClearance {};
  std::size_t slopeImpacts {};
  std::size_t acceptedSlopeContacts {};
  std::size_t correctedPointContacts {};
  std::size_t mergedContacts {};
  std::size_t columnsWithCandidates {};
  std::size_t multiLayerColumns {};
  std::size_t maximumLayers {};
  std::size_t adaptiveThinTriangles {};
  std::size_t adaptiveLocations {};
  std::size_t adaptiveCandidates {};
  std::size_t ledgeEdges {};
  std::size_t ledgeLocations {};
  std::size_t ledgeCandidates {};
  std::vector<Candidate> candidates;
};

// Evaluate bounded, explicit XY locations with exactly the same collision filters as build().
// Returned candidates use column as the input location index and row 0.
std::vector<Candidate> sampleLocations(
  const MapModel &map,
  const std::vector<Vec3> &locations,
  Config config = {}
);

// Provisional world-space floor surfaces; no navigation links or physics equivalence.
Result build(
  const MapModel &map,
  std::string artifactSha256 = {},
  Config config = {}
);

std::string toJson(const Result &result);

} // namespace aipb::map::heightfield
