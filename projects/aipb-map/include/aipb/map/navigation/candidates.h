#pragma once

#include <aipb/map/heightfield/builder.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace aipb::map::navigation {

struct CandidateConfig {
  double spawnPlanarRadius { 256.0 };
  double spawnVerticalTolerance { 96.0 };
  std::size_t maximumCandidates { 100000 };
  double refinementStep { 32.0 };
  std::size_t maximumRefinementLocations { 256 };
  double staticBrushStep { 64.0 };
  std::size_t maximumStaticBrushSamples { 4096 };
  double brushApproachOffset { 24.0 };
  double brushApproachSpacing { 64.0 };
  std::size_t maximumBrushApproachLocations { 512 };
};

struct Candidate {
  std::size_t id {};
  std::size_t heightfieldIndex {};
  bool refined {};
  ObjectId sourceSpawnId {};
  std::size_t sampleIndex {};
  ObjectId supportModelId {};
  ObjectId supportEntityId {};
  ObjectId approachBrushEntityId {};
  bool conditionalSupport {};
  std::size_t column {};
  std::size_t row {};
  Vec3 position {};
  Vec3 floorNormal {};
  std::uint32_t hullMask {};
  bool standingFree {};
  bool crouchingFree {};
};

struct SpawnCoverage {
  ObjectId entityId {};
  std::string entityType;
  Vec3 origin {};
  std::optional<std::size_t> nearestCandidateId {};
  double nearestPlanarDistance { -1.0 };
  double nearestVerticalDistance { -1.0 };
  std::optional<std::size_t> coveredCandidateId {};
  std::optional<std::size_t> covered64 {};
  std::optional<std::size_t> covered128 {};
  std::optional<std::size_t> covered256 {};
};

struct CandidateReport {
  std::string mapName;
  std::string sourceSha256;
  std::string artifactSha256;
  CandidateConfig config {};
  std::size_t gridColumns {};
  std::size_t gridRows {};
  std::size_t occupiedColumns {};
  std::size_t multiLayerColumns {};
  std::size_t standingOnly {};
  std::size_t crouchingOnly {};
  std::size_t bothStances {};
  std::size_t sloped {};
  std::size_t spawnCount {};
  std::size_t spawnsWithinPlanarRadius {};
  std::size_t coveredSpawns {};
  std::size_t covered64 {};
  std::size_t covered128 {};
  std::size_t covered256 {};
  std::size_t refinedSpawns {};
  std::size_t refinementLocations {};
  std::size_t refinementCandidates {};
  std::size_t staticBrushModels {};
  std::size_t staticBrushSamples {};
  std::size_t staticBrushCandidates {};
  std::size_t brushApproachLocations {};
  std::size_t brushApproachCandidates {};
  std::vector<Candidate> candidates;
  std::vector<SpawnCoverage> spawns;
};

// Deterministic provisional nodes; spatial proximity is not reachability.
CandidateReport generateCandidates(
  const MapModel &map,
  const heightfield::Result &heightfield,
  CandidateConfig config = {}
);

// Build once, then sample bounded local XY positions only for uncovered spawns.
CandidateReport generateAdaptiveCandidates(
  const MapModel &map,
  const heightfield::Result &heightfield,
  CandidateConfig config = {}
);

// Add fixed func_wall brush support and combined static clearance to the adaptive report.
CandidateReport generateStaticBrushCandidates(
  const MapModel &map,
  const heightfield::Result &heightfield,
  CandidateConfig config = {}
);

std::string candidatesJson(const CandidateReport &report);

} // namespace aipb::map::navigation
