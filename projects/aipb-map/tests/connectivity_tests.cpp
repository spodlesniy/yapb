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
  require(json.find("\"schema_version\":\"0.2.0\"") != std::string::npos, "wrong schema");

  candidates.candidates[1].standingFree = false;
  candidates.candidates[1].crouchingFree = false;

  try {
    (void)buildConnectivity(map, candidates);
    throw std::runtime_error("invalid candidate accepted");
  } catch (const std::invalid_argument &) {
  }
}


void testJumpTransitions(
  double height,
  TraversalKind expected
) {
  auto map = fixture();
  auto &p = *map.partition;
  p.planes.push_back({ { 1, 0, 0 }, 80, 0 });
  p.planes.push_back({ { 0, 0, 1 }, height, 2 });
  p.planes.push_back({ { 1, 0, 0 }, 64, 0 });
  p.planes.push_back({ { 0, 0, 1 }, height + 36, 2 });
  p.planes.push_back({ { 1, 0, 0 }, 64, 0 });
  p.planes.push_back({ { 0, 0, 1 }, height + 18, 2 });
  p.renderNodes.push_back({ 3, 2, -1 });
  p.renderNodes.push_back({ 4, -1, -2 });
  p.clipNodes.push_back({ 5, 3, -1 });
  p.clipNodes.push_back({ 6, -1, -2 });
  p.clipNodes.push_back({ 7, 5, -1 });
  p.clipNodes.push_back({ 8, -1, -2 });
  p.models.push_back({
    .id = 2,
    .mins = { 80, 0, -10 },
    .maxs = { 160, 128, height },
    .origin = {},
    .headNodes = { 1, 2, 2, 4 }
  });
  MapEntity brush;
  brush.id = 2;
  brush.type = "func_wall";
  brush.partitionModelId = 2;
  map.entities.push_back(brush);
  validate(map);

  CandidateReport candidates;
  candidates.mapName = map.name;
  candidates.sourceSha256 = map.sourceSha256;
  candidates.artifactSha256 = std::string(64, 'b');
  candidates.candidates = {
    makeCandidate(11, { 32, 32, 0 }),
    makeCandidate(19, { 128, 32, height })
  };
  candidates.candidates[1].supportModelId = 2;
  candidates.candidates[1].supportEntityId = 2;
  const auto report = buildConnectivity(map, candidates);
  require(report.edges.size() == 1, "expected one directed jump");
  require(report.edges[0].from == 11 && report.edges[0].to == 19, "jump direction incorrect");
  require(report.edges[0].traversal == expected, "incorrect jump variant");
  require(report.jumpEdges + report.crouchJumpEdges == 1, "jump counters incorrect");
  require(report.weakComponents == 1 && report.isolatedCandidates == 0, "jump did not connect islands");
  require(report.candidatesSha256.size() == 64, "missing candidate fingerprint");
  const auto json = connectivityJson(report);
  require(json.find(expected == TraversalKind::Jump ? "\"traversal\":\"jump\"" :
                   "\"traversal\":\"crouch_jump\"") != std::string::npos,
          "jump kind omitted from JSON");
}

} // namespace

int main() {
  testConnectivity();
  testJumpTransitions(32.0, TraversalKind::Jump);
  testJumpTransitions(52.0, TraversalKind::CrouchJump);

  return 0;
}
