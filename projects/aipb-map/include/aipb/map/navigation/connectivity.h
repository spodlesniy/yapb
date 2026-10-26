#pragma once

#include <aipb/map/navigation/candidates.h>
#include <aipb/map/navigation/jump_physics.h>
#include <aipb/map/collision/queries.h>

#include <cstddef>
#include <string>
#include <vector>

namespace aipb::map::navigation {

enum class TraversalKind { Walk, Jump, CrouchJump };

struct ConnectivityConfig {
  double maximumPlanarDistance { 192.0 };
  double maximumStepUp { 18.0 };
  double maximumStepDown { 36.0 };
  double minimumPlanarDistance { 1.0 };
  double groundTolerance { 10.0 };
  double clearanceLift { 1.0 };
  std::size_t maximumNeighborTrials { 32 };
  std::size_t maximumOutgoingEdges { 8 };
  std::size_t maximumDirectedChecks { 1000000 };
  std::size_t maximumEdges { 150000 };
  double maximumJumpDistance { 160.0 };
  double maximumJumpRise { 44.0 };
  double maximumCrouchJumpRise { 62.0 };
  JumpPhysics jumpPhysics {};
  std::size_t maximumJumpNeighborTrials { 24 };
  std::size_t maximumJumpEdgesPerCandidate { 4 };
  std::size_t maximumJumpChecks { 150000 };
};

struct ProvisionalEdge {
  std::size_t id {};
  std::size_t from {};
  std::size_t to {};
  double planarDistance {};
  double heightChange {};
  collision::PlayerStance stance { collision::PlayerStance::Standing };
  TraversalKind traversal { TraversalKind::Walk };
  bool conditionalSupport {};
  double estimatedFlightSeconds {};
  double estimatedHorizontalSpeed {};
  bool canStandAfterLanding { true };
};

struct ConnectivityReport {
  std::string mapName;
  std::string sourceSha256;
  std::string artifactSha256;
  std::string candidatesSha256;
  ConnectivityConfig config {};
  std::size_t candidateCount {};
  std::size_t testedDirections {};
  std::size_t rejectedHeight {};
  std::size_t rejectedStance {};
  std::size_t rejectedCollision {};
  std::size_t rejectedGround {};
  std::size_t standingEdges {};
  std::size_t crouchingEdges {};
  std::size_t jumpEdges {};
  std::size_t crouchJumpEdges {};
  std::size_t jumpTrials {};
  std::size_t rejectedJumpCollision {};
  std::size_t rejectedJumpLanding {};
  std::size_t rejectedJumpPhysics {};
  std::size_t crouchedLandings {};
  std::size_t crouchedLandingsWithoutStanding {};
  std::size_t conditionalEdges {};
  std::size_t conditionalIslands {};
  std::size_t weakComponents {};
  std::size_t largestComponent {};
  std::size_t isolatedCandidates {};
  std::vector<ProvisionalEdge> edges;
};

// Static, bounded, directed collision checks; not a claim of GoldSrc reachability.
ConnectivityReport buildConnectivity(
  const MapModel &map,
  const CandidateReport &candidates,
  ConnectivityConfig config = {}
);

// Adds bounded candidate-to-candidate jump hypotheses and recomputes component diagnostics.
void appendJumpTransitions(
  const MapModel &map,
  const CandidateReport &candidates,
  ConnectivityReport &report
);

std::string connectivityJson(const ConnectivityReport &report);

} // namespace aipb::map::navigation
