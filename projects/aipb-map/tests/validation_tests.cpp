#include <aipb/map/collision/validation.h>
#include <aipb/map/io/artifact.h>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
using namespace aipb::map;
using namespace aipb::map::collision;

void check(
  bool valid,
  const char *reason
) {
  if (!valid) {
    throw std::runtime_error(reason);
  }
}

MapModel fixture() {
  MapModel model;
  model.name = "validation_fixture";
  model.sourceFormat = "synthetic";
  model.sourceSha256 = std::string(64, 'a');
  model.units = "goldsrc_units";
  MapEntity spawn;
  spawn.id = 1;
  spawn.type = "info_player_start";
  spawn.origin = { 5, 5, 5 };
  model.entities.push_back(spawn);
  model.partition = CollisionPartition {
    .planes = { { { 1, 0, 0 }, 0, 0 } },
    .renderNodes = { { 0, -1, -2 } },
    .clipNodes = { { 0, -1, -2 } },
    .leaves = { { -1 }, { -2 } },
    .models = {
      { 1, { -10, -10, -10 }, { 10, 10, 10 }, {}, { 0, 0, 0, 0 } },
      { 2, { -10, -10, -10 }, { 10, 10, 10 }, {}, { 0, 0, 0, 0 } }
    }
  };
  validate(model);

  return model;
}

void testValidation() {
  const auto map = fixture();
  const auto original = map;
  const auto result = validateQueries(map, std::string(64, 'b'));
  check(map == original, "validation modified map");
  check(result.modelCount == 2 && result.sampledSpawnOrigins == 1, "model or spawn counts");
  check(result.samples.size() == 8 && result.violations == 0, "unexpected query validation findings");
  check(result.samples[0].world && result.samples[0].points == 8, "world point sampling");
  check(!result.samples[4].world && result.samples[4].points == 7, "brush point sampling");
  const auto json = validationJson(result);
  check(json == validationJson(result) && json.back() == '\n', "JSON not deterministic");
  check(json.find("\"schema_version\":\"0.2.0\"") != std::string::npos, "JSON schema");
  check(json.find("\"violations\":0") != std::string::npos, "JSON findings");
  check(result.spawnProbes.hulls.size() == 4 && result.spawnProbes.samples.size() == 4, "spawn hull coverage");
  check(result.spawnProbes.hulls[1].horizontalClear == 3 &&
        result.spawnProbes.hulls[1].horizontalImpacts == 1, "short horizontal probes");
  check(result.spawnProbes.samples[1].traces.size() == 6, "directions not recorded");
  check(json.find("\"spawn_probes\"") != std::string::npos, "JSON gameplay probes missing");

}


void testSpawnGeometry() {
  auto map = fixture();
  auto &partition = *map.partition;
  partition.planes = {
    { { 1, 0, 0 }, 16, 0 },
    { { 0, 0, 1 }, 0, 2 },
    { { 0, 0, 1 }, 40, 2 },
    { { 0, 0, 1 }, 64, 2 }
  };
  partition.renderNodes.clear();
  partition.clipNodes = {
    { 0, -2, 1 },
    { 1, 2, -2 },
    { 2, -2, -1 },
    { 0, -2, 4 },
    { 1, 5, -2 },
    { 3, -2, -1 }
  };
  partition.leaves = { { -1 } };
  partition.models.resize(1);
  partition.models[0].headNodes = { -1, 0, 0, 3 };
  map.entities[0].origin = { 0, 0, 16 };
  MapEntity second;
  second.id = 2;
  second.type = "info_player_deathmatch";
  second.origin = { 0, 0, 48 };
  map.entities.push_back(second);
  MapEntity other;
  other.id = 3;
  other.type = "func_door";
  other.origin = { 0, 0, 16 };
  map.entities.push_back(other);
  validate(map);
  const auto report = validateQueries(map, std::string(64, 'b'));
  check(report.violations == 0 && report.sampledSpawnOrigins == 2, "spawn selection");
  check(report.spawnProbes.samples.size() == 8, "spawn hull samples");
  check(report.spawnProbes.standingBlockedCrouchingFree == 1 &&
        report.spawnProbes.standingFreeCrouchingBlocked == 0, "standing/crouching clearance difference");
  const auto &standing = report.spawnProbes.samples[1];
  const auto &crouching = report.spawnProbes.samples[3];
  check(standing.entityId == 1 && standing.contents == -1, "standing spawn query");
  check(standing.traces.size() == 6 && crouching.traces.size() == 6, "directional traces");
  check(standing.traces[0].direction == "x_positive" &&
        standing.traces[0].result.fraction == 0.5, "positive X collision");
  check(standing.traces[1].result.fraction == 1.0, "negative X should be clear");
  check(std::abs(standing.traces[4].result.fraction - 1.0 / 6.0) < 1e-12 &&
        standing.traces[4].result.impactPlane &&
        standing.traces[4].result.impactPlane->normal.z > 0, "floor trace");
  check(standing.traces[5].result.fraction == 0.25 &&
        standing.traces[5].result.impactPlane &&
        standing.traces[5].result.impactPlane->normal.z < 0, "standing ceiling trace");
  check(crouching.traces[5].result.fraction == 0.5, "crouched ceiling trace");
  check(report.spawnProbes.hulls[1].floorHits == 1 &&
        report.spawnProbes.hulls[1].ceilingHits == 1 &&
        report.spawnProbes.hulls[1].solidOrigins == 1, "standing metrics");
  check(report.spawnProbes.hulls[3].floorHits == 2 &&
        report.spawnProbes.hulls[3].ceilingHits == 2 &&
        report.spawnProbes.hulls[3].solidOrigins == 0, "crouching metrics");
  const auto json = validationJson(report);
  check(json == validationJson(report) &&
        json.find("\"standing_blocked_crouching_free\":1") != std::string::npos,
        "gameplay JSON reproducibility");
}

} // namespace

int main(
  int argc,
  char *argv[]
) {
  try {
    if (argc == 3 && std::string_view(argv[1]) == "--write-fixture") {
      io::saveMap(fixture(), argv[2]);

      return 0;
    }
    check(argc == 1, "invalid validation test arguments");
    testValidation();
    testSpawnGeometry();
    std::cout << "D207.2 gameplay-oriented collision validation checks passed\n";

    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';

    return 1;
  }
}
