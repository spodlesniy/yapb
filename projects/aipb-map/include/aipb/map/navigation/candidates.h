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
};

struct Candidate {
  std::size_t id {};
  std::size_t heightfieldIndex {};
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
  std::vector<Candidate> candidates;
  std::vector<SpawnCoverage> spawns;
};

// Deterministic provisional nodes; spatial proximity is not reachability.
CandidateReport generateCandidates(
  const MapModel &map,
  const heightfield::Result &heightfield,
  CandidateConfig config = {}
);

std::string candidatesJson(const CandidateReport &report);

} // namespace aipb::map::navigation
