#include <aipb/map/heightfield/builder.h>

#include <stdexcept>
#include <string>

namespace {
using namespace aipb::map;
using namespace aipb::map::heightfield;

void check(
  bool condition,
  const char *reason
) {
  if (!condition) {
    throw std::runtime_error(reason);
  }
}

MapModel fixture() {
  MapModel map;
  map.name = "stacked_world";
  map.sourceFormat = "synthetic";
  map.sourceSha256 = std::string(64, 'a');
  map.units = "goldsrc_units";
  map.partition = CollisionPartition {
    .planes = {
      { { 0, 0, 1 }, 0, 2 },
      { { 0, 0, 1 }, 48, 2 },
      { { 0, 0, 1 }, 64, 2 }
    },
    .renderNodes = {},
    .clipNodes = {
      { 2, -1, 1 },
      { 1, -2, 2 },
      { 0, -1, -2 }
    },
    .leaves = { { -1 } },
    .models = {
      { 1, { 0, 0, -16 }, { 64, 64, 96 }, {}, { -1, 0, 0, 0 } }
    }
  };
  validate(map);

  return map;
}

void testStacked() {
  const auto map = fixture();
  const auto snapshot = map;
  Config config;
  config.cellSize = 64;
  config.verticalStep = 16;
  const auto result = build(map, std::string(64, 'b'), config);
  check(map == snapshot, "heightfield modified map");
  check(result.columns == 1 && result.rows == 1, "unexpected grid dimensions");
  check(result.candidates.size() == 4 && result.maximumLayers == 2 &&
        result.multiLayerColumns == 1, "two separated floors per hull not found");
  check(result.columnsWithCandidates == 1 && result.floorHits == 4, "incorrect candidate counts");
  check(result.candidates[0].position.z == 64 && result.candidates[1].position.z == 0,
        "floor order must be descending");
  check(result.candidates[2].hull == 3 && result.candidates[3].hull == 3, "crouching layers missing");
  const auto json = toJson(result);
  check(json == toJson(build(map, std::string(64, 'b'), config)), "non-deterministic output");
  check(json.find("\"schema_version\":\"0.1.0\"") != std::string::npos, "incorrect JSON version");
  check(json.find("\"multi_layer_columns\":1") != std::string::npos, "missing multi-layer statistic");
  config.maximumColumns = 0;

  try {
    (void)build(map, {}, config);
    throw std::runtime_error("invalid sampling budget accepted");
  } catch (const std::invalid_argument &) {
  }
}

} // namespace

int main() {
  testStacked();

  return 0;
}
