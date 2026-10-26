#include <aipb/map/navigation/candidates.h>

#include <algorithm>
#include <stdexcept>
#include <string>

namespace {
using namespace aipb::map;
using namespace aipb::map::navigation;

void check(
  bool condition,
  const char *reason
) {
  if (!condition) {
    throw std::runtime_error(reason);
  }
}

void testStaticBrush() {
  MapModel map;
  map.name = "brush_support";
  map.sourceFormat = "synthetic";
  map.sourceSha256 = std::string(64, 'a');
  map.units = "goldsrc_units";
  // World floor z=-336, func_wall top z=-272, standing origin z=-236.
  map.partition = CollisionPartition {
    .planes = {
      { { 0, 0, 1 }, -336, 2 },
      { { 0, 0, 1 }, -300, 2 },
      { { 0, 0, 1 }, -318, 2 },
      { { 0, 0, 1 }, 0, 2 },
      { { 0, 0, 1 }, 36, 2 },
      { { 0, 0, 1 }, 18, 2 }
    },
    .renderNodes = {
      { 0, -1, -2 },
      { 3, -1, -2 }
    },
    .clipNodes = {
      { 1, -1, -2 },
      { 2, -1, -2 },
      { 4, -1, -2 },
      { 5, -1, -2 }
    },
    .leaves = {
      { -1 },
      { -2 }
    },
    .models = {
      {
        .id = 1,
        .mins = { -256, -256, -400 },
        .maxs = { 256, 256, 128 },
        .origin = {},
        .headNodes = { 0, 0, 0, 1 }
      },
      {
        .id = 2,
        .mins = { -64, -64, -40 },
        .maxs = { 64, 64, 0 },
        .origin = {},
        .headNodes = { 1, 2, 2, 3 }
      }
    }
  };
  MapEntity world;
  world.id = 1;
  world.type = "worldspawn";
  world.partitionModelId = 1;
  map.entities.push_back(world);
  MapEntity brush;
  brush.id = 2;
  brush.type = "func_wall";
  brush.origin = { -1300, -2000, -272 };
  brush.partitionModelId = 2;
  map.entities.push_back(brush);
  MapEntity spawn;
  spawn.id = 3;
  spawn.type = "info_player_start";
  spawn.origin = { -1300, -2000, -236 };
  map.entities.push_back(spawn);
  heightfield::Result heightfield;
  heightfield.mapName = map.name;
  heightfield.sourceSha256 = map.sourceSha256;
  heightfield.artifactSha256 = std::string(64, 'b');
  heightfield.columns = 1;
  heightfield.rows = 1;
  const auto snapshot = map;
  const auto result = generateStaticBrushCandidates(map, heightfield);
  check(map == snapshot, "brush sampling modified source map");
  check(result.staticBrushModels == 1 && result.staticBrushCandidates > 0, "missing static brush surface");
  check(result.coveredSpawns == 1, "spawn on brush remains uncovered");
  check(!result.spawns.empty() && result.spawns[0].coveredCandidateId.has_value(), "missing brush coverage");

  const auto found = std::find_if(result.candidates.begin(), result.candidates.end(), [&](const auto &item) {
    return item.id == *result.spawns[0].coveredCandidateId;
  });
  check(found != result.candidates.end(), "covered candidate ID not found");
  const auto &candidate = *found;
  check(candidate.supportModelId == ObjectId { 2 } &&
        candidate.supportEntityId == ObjectId { 2 }, "incorrect brush provenance");
  check(candidate.position.z == -272 && (candidate.standingFree || candidate.crouchingFree),
        "incorrect brush contact/clearance");
  const auto json = candidatesJson(result);
  check(json.find("\"support_kind\":\"static_brush\"") != std::string::npos, "brush kind missing");
  check(json.find("\"heightfield_index\":null") != std::string::npos, "brush heightfield index not null");
  check(json.find("\"sample_index\":1") != std::string::npos, "brush sample index missing");
  check(json == candidatesJson(generateStaticBrushCandidates(map, heightfield)),
        "non-deterministic brush report");

  // Only Trigger supplies conditional support, while mutable breakables do not.
  map.entities[1].type = "func_breakable";
  map.entities[1].properties["spawnflags"] = "1";
  const auto trigger = generateStaticBrushCandidates(map, heightfield);
  check(trigger.staticBrushCandidates > 0 && trigger.candidates.front().conditionalSupport,
        "trigger-only breakable must be conditional support");
  check(candidatesJson(trigger).find("\\"support_condition\\":\\"until_trigger\\"") != std::string::npos,
        "conditional support serialization missing");

  for (const char *flags : { "0", "2", "4", "256", "3", "257", "invalid" }) {
    map.entities[1].properties["spawnflags"] = flags;
    const auto mutableResult = generateStaticBrushCandidates(map, heightfield);
    check(mutableResult.staticBrushCandidates == 0, "player-breakable object became support");
  }
  map.entities[1].properties["spawnflags"] = "5";
  check(generateStaticBrushCandidates(map, heightfield).staticBrushCandidates > 0,
        "Only Trigger must take priority over Pressure");

  // A dynamic brush does not provide static support.
  map.entities[1].type = "func_door";
  const auto excluded = generateStaticBrushCandidates(map, heightfield);
  check(excluded.staticBrushModels == 0 && excluded.staticBrushCandidates == 0,
        "dynamic brush treated as static");
}

} // namespace

int main() {
  testStaticBrush();

  return 0;
}
