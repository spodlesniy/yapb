#include <aipb/map/collision/validation.h>
#include <aipb/map/io/artifact.h>

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
  check(json.find("\"schema_version\":\"0.1.0\"") != std::string::npos, "JSON schema");
  check(json.find("\"violations\":0") != std::string::npos, "JSON findings");
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
    std::cout << "D207.1 validation checks passed\n";

    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';

    return 1;
  }
}
