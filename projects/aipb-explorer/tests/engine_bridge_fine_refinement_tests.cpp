#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

#include <aipb/explorer/engine_bridge_fine_refinement.h>

using namespace aipb::explorer::bridge;

static void check(bool value) {
  if (!value) {
    throw std::runtime_error("fine refinement regression");
  }
}

static std::string row(
  int step,
  const char *kind,
  double sx,
  double sy,
  double sz,
  double ex,
  double ey,
  double ez,
  double yaw
) {
  char output[768] {};
  const int n = std::snprintf(output, sizeof(output),
    "%d,%d,%d,%s,%s,unknown,100,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,25,5,5,0\n",
    step, step + 3, step, kind,
    std::string(kind) == "Progress" ? "direct" : "confirming",
    sx, sy, sz, ex, ey, ez, yaw);
  check(n > 0 && static_cast<std::size_t>(n) < sizeof(output));
  return output;
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
    EvidenceTrial first = trial("first");
    first.rows.push_back(row(0, "BlockedCandidate", 8, 8, 8, 8, 8, 8, 0));
    first.rows.push_back(row(4, "BlockedCandidate", 8, 8, 8, 8, 8, 8, 0));
    first.rows.push_back(row(8, "Progress", 9, 9, 9, 13, 9, 9, 0));
    first.rows.push_back(row(12, "BlockedCandidate", 10, 9, 9, 14, 9, 9, 0));
    first.rows.push_back(row(16, "Progress", 9, 9, 9, 9, 13, 9, 0));
    first.rows.push_back(row(20, "Progress", -7, 9, 9, -3, 9, 9, 90));
    first.rows.push_back(row(24, "Progress", 9, 9, 9, 9, 9, 13, 0));
    sources.trials.push_back(first);
    EvidenceTrial second = trial("second");
    second.rows.push_back(row(0, "Progress", 10, 9, 9, 14, 9, 9, 0));
    sources.trials.push_back(second);

    FineRefinementAudit report {};
    check(auditFineRefinement(sources, 8, 8, 8, report));
    check(report.targets.zeros.coverage.zeroFaceRecords == 8);
    check(report.groupedWindows == 8 && report.distinctSources == 2);
    check(report.groups == 5 && report.mixedGroups == 1);
    check(report.stationaryGroups == 1 && report.multipleSourceGroups == 1);
    check(report.details[0].mixedOutcomes() && report.details[0].sources == 2);
    check(report.details[0].progressWindows == 2 && report.details[0].restrictionWindows == 1);
    check(report.details[0].startSubcell.x == 1 && report.details[0].travelBin == 0);
    bool sawStationary = false;
    bool sawNegative = false;
    bool sawVertical = false;

    for (const FineRefinementGroup &group : report.details) {
      if (group.travelBin == kFineRefinementStationary) {
        sawStationary = true;
        check(group.windows == 2 && group.sources == 1);
      }

      if (group.startSubcell.x == -1) {
        sawNegative = true;
        check(group.headingBin == 2 && group.travelBin == 0);
      }

      if (group.travelBin == kFineRefinementUp) {
        sawVertical = true;
        check(group.windows == 1);
      }
    }

    check(sawStationary && sawNegative && sawVertical);
    report.groups = 77;
    check(!auditFineRefinement(sources, 1e40, 8, 8, report));
    check(report.groups == 77);
    EvidenceTrialSet empty {};
    check(!auditFineRefinement(empty, 8, 8, 8, report));
    check(report.groups == 77);
    return 0;
  }
  catch (const std::exception &error) {
    std::fprintf(stderr, "fine refinement: %s\n", error.what());
    return 1;
  }
}
