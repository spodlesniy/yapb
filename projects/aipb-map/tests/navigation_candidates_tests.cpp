#include <aipb/map/navigation/candidates.h>

#include <stdexcept>
#include <string>

namespace {
using namespace aipb::map;
using namespace aipb::map::navigation;

void check(
  bool condition,
  const char *message
) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void testCandidates() {
  MapModel map;
  map.name = "nav_fixture";
  map.sourceSha256 = std::string(64, 'a');
  MapEntity first;
  first.id = 1;
  first.type = "info_player_start";
  first.origin = { 32, 32, 10 };
  map.entities.push_back(first);
  MapEntity second;
  second.id = 2;
  second.type = "info_player_deathmatch";
  second.origin = { 400, 32, 10 };
  map.entities.push_back(second);
  MapEntity third;
  third.id = 3;
  third.type = "info_vip_start";
  third.origin = { 32, 32, 150 };
  map.entities.push_back(third);
  heightfield::Result heightfield;
  heightfield.mapName = map.name;
  heightfield.sourceSha256 = map.sourceSha256;
  heightfield.artifactSha256 = std::string(64, 'b');
  heightfield.columns = 3;
  heightfield.rows = 1;
  heightfield.candidates = {
    { .column = 0, .row = 0, .hullMask = 10, .position = { 32, 32, 0 },
      .floorNormal = { 0, 0, 1 }, .standingFree = true, .crouchingFree = true },
    { .column = 0, .row = 0, .hullMask = 8, .position = { 32, 32, 130 },
      .floorNormal = { 0, 0, 1 }, .standingFree = false, .crouchingFree = true },
    { .column = 1, .row = 0, .hullMask = 2, .position = { 160, 32, 0 },
      .floorNormal = { 0, 0, 1 }, .standingFree = true, .crouchingFree = false }
  };
  const auto original = heightfield;
  CandidateConfig config;
  config.spawnPlanarRadius = 100;
  config.spawnVerticalTolerance = 32;
  const auto report = generateCandidates(map, heightfield, config);
  check(heightfield.candidates.size() == original.candidates.size() &&
        heightfield.candidates[0].position == original.candidates[0].position,
        "heightfield modified");
  check(report.candidates.size() == 3 && report.occupiedColumns == 2 &&
        report.multiLayerColumns == 1, "candidate counts incorrect");
  check(report.bothStances == 1 && report.standingOnly == 1 &&
        report.crouchingOnly == 1, "stance stats incorrect");
  check(report.spawnCount == 3 && report.coveredSpawns == 2 &&
        report.spawnsWithinPlanarRadius == 2, "spawn coverage incorrect");
  check(report.spawns[0].coveredCandidateId == 1 &&
        !report.spawns[1].coveredCandidateId &&
        report.spawns[2].coveredCandidateId == 2, "spawn matching incorrect");
  const auto json = candidatesJson(report);
  check(json == candidatesJson(generateCandidates(map, heightfield, config)),
        "non-deterministic candidate JSON");
  check(json.find("\"schema_version\":\"0.1.0\"") != std::string::npos, "report version incorrect");
  heightfield.candidates[0].standingFree = false;
  heightfield.candidates[0].crouchingFree = false;

  try {
    (void)generateCandidates(map, heightfield);
    throw std::runtime_error("blocked candidate accepted");
  } catch (const std::invalid_argument &) {
  }
}

} // namespace

int main() {
  testCandidates();

  return 0;
}
