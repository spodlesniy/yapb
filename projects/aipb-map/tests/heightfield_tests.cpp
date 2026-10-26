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
      { { 0, 0, 1 }, 64, 2 },
      { { 0, 0, 1 }, 48, 2 },
      { { 0, 0, 1 }, 0, 2 },
      { { 0, 0, 1 }, 100, 2 },
      { { 0, 0, 1 }, 84, 2 },
      { { 0, 0, 1 }, 36, 2 },
      { { 0, 0, 1 }, 82, 2 },
      { { 0, 0, 1 }, 66, 2 },
      { { 0, 0, 1 }, 18, 2 }
    },
    .renderNodes = {
      { 0, -1, 1 },
      { 1, -2, 2 },
      { 2, -1, -2 }
    },
    .clipNodes = {
      { 3, -1, 1 },
      { 4, -2, 2 },
      { 5, -1, -2 },
      { 6, -1, 4 },
      { 7, -2, 5 },
      { 8, -1, -2 }
    },
    .leaves = {
      { -1 },
      { -2 }
    },
    .models = {
      { 1, { 0, 0, -16 }, { 64, 64, 160 }, {}, { 0, 0, 0, 3 } }
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
  check(result.columns == 1 && result.rows == 1, "incorrect grid");
  check(result.candidates.size() == 2 && result.maximumLayers == 2 &&
        result.multiLayerColumns == 1, "two merged floors not found");
  check(result.floorHits == 4 && result.mergedContacts == 2, "contact merge counts");
  check(result.candidates[0].position.z == 64 && result.candidates[1].position.z == 0,
        "normalized floor heights");
  check(result.candidates[0].hullMask == 10 && result.candidates[1].hullMask == 10,
        "standing crouching mask");
  check(result.rejectedOutsideWorld == 0 && result.rejectedUnverifiedPointHull == 0,
        "unexpected rejection");
  const auto json = toJson(result);
  check(json == toJson(build(map, std::string(64, 'b'), config)), "non-deterministic output");
  check(json.find("\"schema_version\":\"0.3.0\"") != std::string::npos, "incorrect JSON version");
  check(json.find("\"merged_contacts\":2") != std::string::npos, "missing merged contacts");
  check(result.candidates[0].standingFree || result.candidates[0].crouchingFree, "no clearance");
  config.maximumColumns = 0;

  try {
    (void)build(map, {}, config);
    throw std::runtime_error("invalid sampling budget accepted");
  } catch (const std::invalid_argument &) {
  }
}

void testSlope() {
  auto map = fixture();
  auto &partition = *map.partition;
  // A single inclined solid plane; hull-0 and clip-hull offsets reflect different actor extents.
  partition.planes = {
    { { 0.6, 0, 0.8 }, 0, 3 },
    { { 0.6, 0, 0.8 }, 45.6, 3 },
    { { 0.6, 0, 0.8 }, 31.2, 3 }
  };
  partition.renderNodes = { { 0, -1, -2 } };
  partition.clipNodes = { { 1, -1, -2 }, { 1, -1, -2 }, { 2, -1, -2 } };
  partition.models[0].headNodes = { 0, 0, 0, 2 };
  partition.models[0].mins = { 0, 0, -64 };
  partition.models[0].maxs = { 64, 64, 128 };
  validate(map);
  Config config;
  config.cellSize = 64;
  config.verticalStep = 16;
  const auto result = build(map, {}, config);
  check(result.slopeImpacts > 0, "inclined contacts absent");
  check(result.acceptedSlopeContacts > 0, "inclined contacts rejected");
  check(!result.candidates.empty(), "no slope candidates");

  for (const auto &candidate : result.candidates) {
    check(candidate.floorNormal.z >= config.minimumUpNormal, "bad slope normal");
    check(candidate.standingFree || candidate.crouchingFree, "blocked candidate admitted");
  }
}

} // namespace

int main() {
  testStacked();
  testSlope();

  return 0;
}
