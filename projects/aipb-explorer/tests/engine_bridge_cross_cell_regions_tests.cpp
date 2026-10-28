#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

#include <aipb/explorer/engine_bridge_cross_cell_regions.h>

using namespace aipb::explorer::bridge;

static void require(bool ok) {
  if (!ok) {
    throw std::runtime_error("cross-cell regions regression");
  }
}

static std::string window(int step, const char *kind, double x, double end, double y = 8) {
  char row[768] {};
  const bool good = std::string(kind) == "Progress";
  const int n = std::snprintf(row, sizeof(row),
    "%d,%d,%d,%s,%s,unknown,100,%.17g,%.17g,8,%.17g,%.17g,8,0,25,5,5,0\n",
    step, step + 3, step, kind, good ? "direct" : "confirming", x, y, end, y);
  require(n > 0 && static_cast<std::size_t>(n) < sizeof(row));
  return row;
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
    EvidenceTrialSet set {};
    auto a = trial("a");
    a.rows = { window(0, "Progress", 116, 139),
      window(4, "Progress", 141, 152) };
    auto b = trial("b");
    b.rows = { window(0, "Progress", 115, 135),
      window(4, "Progress", 140, 151) };
    auto wall = trial("wall");
    wall.rows = { window(0, "BlockedCandidate", 220, 220),
      window(4, "BlockedCandidate", 223, 223) };
    set.trials = { a, b, wall };
    set.rows = 6;
    CrossCellRegion out {};
    require(spanSourceEvidence(set, 124, 8, 8, 0, out));
    require(out.seedKind == RegionEvidenceKind::ProgressOnly);
    require(out.coarseCells == 2 && out.memberCells == 2);
    require(out.minCell.x == 3 && out.maxCell.x == 4);
    require(out.region.windows == 4 && out.region.strongProgressSources == 2);
    require(out.region.status == DirectionAssessmentStatus::ProgressEvidence);
    require(out.boundaryUnobserved > 0);
    require(spanSourceEvidence(set, 220, 8, 8, 0, out));
    require(out.seedKind == RegionEvidenceKind::RestrictionOnly);
    require(out.memberCells == 1 && out.region.strongRestrictionSources == 1);
    require(spanSourceEvidence(set, 180, 8, 8, 0, out));
    require(out.seedKind == RegionEvidenceKind::Unobserved && out.memberCells == 0);

    EvidenceTrialSet conflict {};
    auto positive = trial("progress");
    positive.rows = { window(0, "Progress", 115, 120),
      window(4, "Progress", 145, 150),
      window(8, "Progress", 180, 185) };
    auto negative = trial("restriction");
    negative.rows = { window(0, "BlockedCandidate", 145, 145) };
    conflict.trials = { positive, negative };
    conflict.rows = 4;
    require(spanSourceEvidence(conflict, 116, 8, 8, 0, out));
    require(out.memberCells == 1 && out.boundaryMixed > 0);
    require(spanSourceEvidence(conflict, 145, 8, 8, 0, out));
    require(out.seedKind == RegionEvidenceKind::Mixed && out.memberCells == 0);
    require(spanSourceEvidence(conflict, 182, 8, 8, 0, out));
    require(out.memberCells == 1);

    EvidenceTrialSet corner {};
    auto diagonal = trial("diagonal");
    diagonal.rows = { window(0, "Progress", 110, 115, 8),
      window(4, "Progress", 145, 150, 40) };
    corner.trials = { diagonal };
    corner.rows = 2;
    require(spanSourceEvidence(corner, 111, 8, 8, 0, out));
    require(out.memberCells == 1);

    EvidenceTrialSet negativeCoords {};
    auto neg = trial("negative");
    neg.rows = { window(0, "Progress", -131, -125),
      window(4, "Progress", -123, -115) };
    negativeCoords.trials = { neg };
    negativeCoords.rows = 2;
    require(spanSourceEvidence(negativeCoords, -130, 8, 8, 0, out));
    require(out.coarseCells == 2 && out.region.windows == 2);
    require(out.seedCoarse.x == -2 && out.seedCell.x == -5);

    out.memberCells = 77;
    require(!spanSourceEvidence(set, 1e40, 0, 0, 0, out));
    require(!spanSourceEvidence(set, 0, 0, 0, std::nan(""), out));
    require(out.memberCells == 77);
    return 0;
  }
  catch (const std::exception &e) {
    std::fprintf(stderr, "cross-cell regions: %s\n", e.what());
    return 1;
  }
}
