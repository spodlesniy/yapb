#include <aipb/map/navigation/connectivity.h>

#include <stdexcept>
#include <string>

namespace {
using namespace aipb::map;
using namespace aipb::map::navigation;

void require(
  bool condition,
  const char *reason
) {
  if (!condition) {
    throw std::runtime_error(reason);
  }
}

MapModel fixture() {
  MapModel map;
  map.name = "connectivity_fixture";
  map.sourceFormat = "synthetic";
  map.sourceSha256 = std::string(64, 'a');
  map.units = "goldsrc_units";
  map.partition = CollisionPartition {
    .planes = {
      { { 0, 0, 1 }, 0, 2 },
      { { 0, 0, 1 }, 36, 2 },
      { { 0, 0, 1 }, 18, 2 }
    },
    .renderNodes = {
      { 0, -1, -2 }
    },
    .clipNodes = {
      { 1, -1, -2 },
      { 2, -1, -2 }
    },
    .leaves = {
      { -1 },
      { -2 }
    },
    .models = {
      {
        .id = 1,
        .mins = { 0, 0, -64 },
        .maxs = { 256, 128, 128 },
        .origin = {},
        .headNodes = { 0, 0, 0, 1 }
      }
    }
  };
  validate(map);

  return map;
}

Candidate makeCandidate(
  std::size_t id,
  Vec3 position
) {
  Candidate result;
  result.id = id;
  result.position = position;
  result.floorNormal = { 0, 0, 1 };
  result.hullMask = 10;
  result.standingFree = true;
  result.crouchingFree = true;
  result.supportModelId = 1;

  return result;
}

void testConnectivity() {
  const auto map = fixture();
  CandidateReport candidates;
  candidates.mapName = map.name;
  candidates.sourceSha256 = map.sourceSha256;
  candidates.artifactSha256 = std::string(64, 'b');
  candidates.candidates = {
    makeCandidate(1, { 32, 32, 0 }),
    makeCandidate(4, { 96, 32, 0 }),
    makeCandidate(9, { 160, 32, 100 })
  };
  const auto snapshot = candidates;
  const auto result = buildConnectivity(map, candidates);
  require(result.candidateCount == 3 && result.testedDirections > 0, "no candidate neighbor checks");
  require(result.edges.size() == 2, "expected directed horizontal connections");
  require(result.edges[0].from == 1 && result.edges[0].to == 4, "first directed link incorrect");
  require(result.edges[1].from == 4 && result.edges[1].to == 1, "reverse link incorrect");
  require(result.rejectedHeight > 0 && result.weakComponents == 2 &&
          result.largestComponent == 2 && result.isolatedCandidates == 1,
          "height and component checks incorrect");
  require(candidates.candidates.size() == snapshot.candidates.size() &&
          candidates.candidates[1].id == snapshot.candidates[1].id,
          "candidate input modified");
  const auto json = connectivityJson(result);
  require(json == connectivityJson(buildConnectivity(map, candidates)), "non-deterministic connectivity JSON");
  require(json.find("\"schema_version\":\"0.1.0\"") != std::string::npos, "wrong schema");

  candidates.candidates[1].standingFree = false;
  candidates.candidates[1].crouchingFree = false;

  try {
    (void)buildConnectivity(map, candidates);
    throw std::runtime_error("invalid candidate accepted");
  } catch (const std::invalid_argument &) {
  }
}

} // namespace

int main() {
  testConnectivity();

  return 0;
}
