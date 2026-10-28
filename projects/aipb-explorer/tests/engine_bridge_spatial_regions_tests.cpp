#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

#include <aipb/explorer/engine_bridge_spatial_regions.h>

using namespace aipb::explorer::bridge;

static void require(bool valid) {
  if (!valid) {
    throw std::runtime_error("spatial region regression");
  }
}

static std::string window(int step, const char *classification,
  double x, double endX, double y = 8.0, double heading = 0.0) {
  char text[768] {};
  const bool progress = std::string(classification) == "Progress";
  const int count = std::snprintf(text, sizeof(text),
    "%d,%d,%d,%s,%s,unknown,100,%.17g,%.17g,8,%.17g,%.17g,8,%.9g,25,5,5,0\n",
    step, step + 3, step, classification, progress ? "direct" : "confirming",
    x, y, endX, y, heading);
  require(count > 0 && static_cast<std::size_t>(count) < sizeof(text));
  return text;
}

static EvidenceTrial trial(const char *name) {
  EvidenceTrial t {};
  t.name = name;
  t.botIndex = 0;
  t.sourceSteps = 100;
  return t;
}

int main() {
  try {
    EvidenceTrialSet sources {};
    auto a = trial("progress_a");
    a.rows = { window(0, "Progress", 8, 12),
      window(4, "Progress", 40, 44),
      window(8, "Progress", 14, 18),
      window(12, "Progress", 48, 52) };
    auto b = trial("progress_b");
    b.rows = { window(0, "Progress", 18, 22),
      window(4, "Progress", 54, 58),
      window(8, "Progress", 20, 24),
      window(12, "Progress", 42, 46) };
    auto wall = trial("wall");
    wall.rows = { window(0, "BlockedCandidate", 100, 100),
      window(4, "BlockedCandidate", 104, 104) };
    sources.trials = { a, b, wall };
    sources.rows = 10;

    RegionEvidence result {};
    require(groupNeighboringEvidence(sources, 12, 8, 8, 0, result));
    require(result.coarse.status == DirectionAssessmentStatus::Conflicted);
    require(result.seedKind == RegionEvidenceKind::ProgressOnly);
    require(result.memberCells == 2);
    require(result.minCell.x == 0 && result.maxCell.x == 1);
    require(result.region.windows == 8 && result.region.strongProgressSources == 2);
    require(result.region.status == DirectionAssessmentStatus::ProgressEvidence);
    require(result.boundaryMixed == 0 && result.boundaryUnobserved > 0);
    require(result.region.restrictionSources == 0);

    require(groupNeighboringEvidence(sources, 104, 8, 8, 0, result));
    require(result.seedKind == RegionEvidenceKind::RestrictionOnly);
    require(result.memberCells == 1 && result.region.windows == 2);
    require(result.region.status == DirectionAssessmentStatus::Insufficient);
    require(result.region.strongRestrictionSources == 1);
    require(result.boundaryCoarseEdge > 0);

    require(groupNeighboringEvidence(sources, 76, 8, 8, 0, result));
    require(result.coarse.status == DirectionAssessmentStatus::Conflicted);
    require(result.seedKind == RegionEvidenceKind::Unobserved && result.memberCells == 0);

    // A diagonal-only neighbor is not part of a six-connected component.
    EvidenceTrialSet diagonal {};
    auto d = trial("diagonal");
    d.rows = { window(0, "Progress", 10, 14),
      window(4, "Progress", 44, 48, 44) };
    diagonal.trials = { d };
    diagonal.rows = 2;
    require(groupNeighboringEvidence(diagonal, 10, 8, 8, 0, result));
    require(result.memberCells == 1 && result.region.windows == 1);

    // A mixed subcell is an impassable *evidence-grouping* boundary.
    EvidenceTrialSet conflict {};
    auto mixed = trial("mixed");
    mixed.rows = { window(0, "Progress", 10, 14),
      window(4, "Progress", 40, 44),
      window(8, "Progress", 76, 80) };
    auto blocked = trial("blocked");
    blocked.rows = { window(0, "BlockedCandidate", 42, 42) };
    conflict.trials = { mixed, blocked };
    conflict.rows = 4;
    require(groupNeighboringEvidence(conflict, 10, 8, 8, 0, result));
    require(result.memberCells == 1 && result.boundaryMixed > 0);
    require(groupNeighboringEvidence(conflict, 44, 8, 8, 0, result));
    require(result.memberCells == 0 && result.seedKind == RegionEvidenceKind::Mixed);
    require(groupNeighboringEvidence(conflict, 76, 8, 8, 0, result));
    require(result.memberCells == 1 && result.seedKind == RegionEvidenceKind::ProgressOnly);

    // The same segment can appear in adjacent fine cells but is one window.
    EvidenceTrialSet crossing {};
    auto ca = trial("cross_a");
    ca.rows = { window(0, "Progress", 18, 45),
      window(4, "Progress", 48, 51) };
    auto cb = trial("cross_b");
    cb.rows = { window(0, "Progress", 16, 43),
      window(4, "Progress", 52, 56) };
    crossing.trials = { ca, cb };
    crossing.rows = 4;
    require(groupNeighboringEvidence(crossing, 18, 8, 8, 0, result));
    require(result.memberCells == 2 && result.region.windows == 4);
    require(result.region.status == DirectionAssessmentStatus::ProgressEvidence);

    // Negative coordinates use floor division, and invalid queries are atomic.
    EvidenceTrialSet negative {};
    auto n = trial("negative");
    n.rows = { window(0, "Progress", -60, -56, -10),
      window(4, "Progress", -50, -46, -10) };
    negative.trials = { n };
    negative.rows = 2;
    require(groupNeighboringEvidence(negative, -59, -10, 8, -360, result));
    require(result.coarseCell.x == -1 && result.seedCell.x == -2);
    require(result.memberCells == 1 && result.region.windows == 2);
    result.memberCells = 57;
    require(!groupNeighboringEvidence(sources, 1e40, 8, 8, 0, result));
    require(result.memberCells == 57);
    require(!groupNeighboringEvidence(sources, 8, 8, 8, std::nan(""), result));
    require(result.memberCells == 57);

    return 0;
  }
  catch (const std::exception &error) {
    std::fprintf(stderr, "spatial regions: %s\n", error.what());
    return 1;
  }
}
