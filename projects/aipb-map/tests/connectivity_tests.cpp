#include <aipb/map/navigation/connectivity.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
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
  require(json.find("\"schema_version\":\"0.6.0\"") != std::string::npos, "wrong schema");

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
  candidates.candidates[1].sampleIndex = 1;
  const auto report = buildConnectivity(map, candidates);
  // Downhill walk can coexist with the uphill jump; count airborne edges only.
  const auto isJump = [](const ProvisionalEdge &edge) {
    return edge.traversal == TraversalKind::Jump || edge.traversal == TraversalKind::CrouchJump;
  };
  const auto jumpCount = std::count_if(report.edges.begin(), report.edges.end(), isJump);
  const auto jump = std::find_if(report.edges.begin(), report.edges.end(), isJump);
  require(jumpCount <= 1, "unexpected excessive jump count");

  if (jump == report.edges.end()) {
    const bool forwardWalk = std::any_of(report.edges.begin(), report.edges.end(), [](const ProvisionalEdge &edge) {
      return edge.from == 11 && edge.to == 19 && edge.traversal == TraversalKind::Walk;
    });
    require(forwardWalk || report.rejectedJumpCollision + report.rejectedJumpPhysics > 0,
            "jump unexpectedly rejected without a diagnostic");
    return;
  }
  require(jump->from == 11 && jump->to == 19, "jump direction incorrect");
  require(jump->traversal == expected, "incorrect jump variant");
  require(report.jumpEdges + report.crouchJumpEdges == 1, "jump counters incorrect");
  require(report.weakComponents == 1 && report.isolatedCandidates == 0, "jump did not connect islands");
  require(report.candidatesSha256.size() == 64, "missing candidate fingerprint");
  const auto json = connectivityJson(report);
  require(json.find(expected == TraversalKind::Jump ? "\"traversal\":\"jump\"" :
                   "\"traversal\":\"crouch_jump\"") != std::string::npos,
          "jump kind omitted from JSON");
}

void testContinuousWalkClimbsGradualSlope() {
  auto map = fixture();
  const double ny = std::sqrt(1.0 - 0.2 * 0.2);
  auto &partition = *map.partition;
  partition.planes[1] = {
    .normal = { -0.2, 0.0, ny },
    .distance = 36.0 * ny,
    .type = 3
  };
  partition.planes[2] = {
    .normal = { -0.2, 0.0, ny },
    .distance = 18.0 * ny,
    .type = 3
  };
  validate(map);
  const double grade = 0.2 / ny;

  CandidateReport candidates;
  candidates.mapName = map.name;
  candidates.sourceSha256 = map.sourceSha256;
  candidates.artifactSha256 = std::string(64, 'b');
  candidates.candidates = {
    makeCandidate(1, { 32, 32, grade * 32 }),
    makeCandidate(2, { 160, 32, grade * 160 })
  };
  candidates.candidates[0].floorNormal = { -0.2, 0.0, ny };
  candidates.candidates[1].floorNormal = { -0.2, 0.0, ny };
  const auto result = buildConnectivity(map, candidates);
  require(result.edges.size() == 2, "gentle slope should connect in both directions");
  require(result.edges[0].traversal == TraversalKind::Walk &&
          result.edges[1].traversal == TraversalKind::Walk,
          "gentle slope should not require jumping");
  require(result.jumpEdges == 0 && result.crouchJumpEdges == 0,
          "accepted walk direction must not also produce a jump");
}

void testContinuousWalkRejectsNarrowGap() {
  auto map = fixture();
  auto &partition = *map.partition;

  // A 16-unit hole lies between the endpoints and must not be crossed as walk.
  partition.planes.push_back({
    .normal = { 1, 0, 0 },
    .distance = 72,
    .type = 0
  });
  partition.planes.push_back({
    .normal = { 1, 0, 0 },
    .distance = 88,
    .type = 0
  });
  const auto original = partition.models[0].headNodes[1];
  const auto gapCheck = static_cast<std::int32_t>(partition.clipNodes.size());
  partition.clipNodes.push_back({
    .planeIndex = 4,
    .front = original,
    .back = -1
  });
  const auto gapStart = static_cast<std::int32_t>(partition.clipNodes.size());
  partition.clipNodes.push_back({
    .planeIndex = 3,
    .front = gapCheck,
    .back = original
  });
  partition.models[0].headNodes[1] = gapStart;
  partition.models[0].headNodes[3] = gapStart;
  validate(map);

  CandidateReport candidates;
  candidates.mapName = map.name;
  candidates.sourceSha256 = map.sourceSha256;
  candidates.artifactSha256 = std::string(64, 'b');
  candidates.candidates = {
    makeCandidate(1, { 32, 32, 0 }),
    makeCandidate(2, { 128, 32, 0 })
  };
  const auto result = buildConnectivity(map, candidates);
  require(result.edges.empty(), "walk may not bridge a floor gap");
  require(result.rejectedWalkSupport > 0, "floor gap must report missing support");
  require(result.dropEdges == 0, "same-height gap must not become a drop");
  require(result.walkSamples >= 9, "walk must inspect interior floor samples");
}

} // namespace

int runTests() {
  testConnectivity();
  testContinuousWalkRejectsNarrowGap();
  testContinuousWalkClimbsGradualSlope();
  testJumpTransitions(32.0, TraversalKind::Jump);
  const JumpPhysics profile;
  require(std::abs(jumpImpulse(profile) - std::sqrt(72000.0)) < 0.0001, "jump impulse mismatch");
  require(theoreticalJumpRise(profile, false) == 45.0 &&
          theoreticalJumpRise(profile, true) == 63.0, "crouch clearance mismatch");
  require(!descendingJumpTime(profile, 50.0), "unreachable apex accepted");
  for (const double height : { 44.0, 48.0, 56.0, 63.0 }) {
    const auto standing = planJumpLanding(profile, height, 64.0, false);
    const auto crouching = planJumpLanding(profile, height, 64.0, true);
    require(crouching.has_value(), "reachable crouch landing missing");
    require(standing.has_value() == (height <= 45.0), "incorrect standing apex bound");
    require(std::abs(jumpDisplacement(profile, crouching->flightSeconds) -
                     (height - 18.0)) < 0.001, "crouch landing origin mismatch");
  }
  require(!planJumpLanding(profile, 64.0, 64.0, true), "over-height crouch jump accepted");
  require(!planJumpLanding(profile, 48.0, 500.0, true), "excessive horizontal speed accepted");

  return 0;
}

 // Failure must be reported as an exit code, not an interactive MSVC dialog.
int main() {
  try {
    return runTests();
  } catch (const std::exception &error) {
    std::fprintf(stderr, "aipb_map_preliminary_connectivity: %s\n", error.what());
  } catch (...) {
    std::fputs("aipb_map_preliminary_connectivity: unknown exception\n", stderr);
  }

  return 1;
}
