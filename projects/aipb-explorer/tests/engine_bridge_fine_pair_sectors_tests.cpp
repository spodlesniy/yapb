#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

#include <aipb/explorer/engine_bridge_fine_pair_sectors.h>

using namespace aipb::explorer::bridge;

static void check(bool v) {
  if (!v) {
    throw std::runtime_error("fine pair sectors regression");
  }
}

static std::string row(int step, const char *kind, double heading, bool reverse = false) {
  char buffer[768] {};
  const int n = std::snprintf(buffer, sizeof(buffer),
    "%d,%d,%d,%s,%s,unknown,100,%.17g,8,8,%.17g,8,8,%.17g,25,5,5,0\n",
    step, step + 3, step, kind,
    std::string(kind) == "Progress" ? "direct" : "confirming",
    reverse ? 112.0 : 8.0, reverse ? 8.0 : 112.0, heading);
  check(n > 0 && static_cast<std::size_t>(n) < sizeof(buffer));
  return buffer;
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
    FinePairSectorAudit report {};
    EvidenceTrialSet sources {};
    EvidenceTrial one = trial("one");
    one.rows.push_back(row(0, "Progress", 0));
    one.rows.push_back(row(4, "Progress", 90));
    one.rows.push_back(row(8, "Progress", 180));
    sources.trials.push_back(one);
    check(auditFinePairSectors(sources, 60, 8, 8, report));
    check(report.distinctPairs == 2 && report.chains == 3);
    check(report.orderedPairWindows == 6 && report.multipleSectorPairs == 2);
    check(report.progressEvidencePairs == 0 && report.insufficientPairs == 2);
    check(report.details[0].outcome.progressBins == 0x15u);
    check(report.details[0].outcome.evidence.strongProgressSources == 1);

    EvidenceTrial two = trial("two");
    two.rows.push_back(row(0, "Progress", 270));
    two.rows.push_back(row(4, "Progress", 315));
    sources.trials.push_back(two);
    check(auditFinePairSectors(sources, 60, 8, 8, report));
    check(report.progressEvidencePairs == 2);
    check(report.details[0].outcome.evidence.strongProgressSources == 2);

    EvidenceTrial reverse = trial("reverse");
    reverse.rows.push_back(row(0, "BlockedCandidate", 0, true));
    reverse.rows.push_back(row(4, "BlockedCandidate", 90, true));
    sources.trials.push_back(reverse);
    check(auditFinePairSectors(sources, 60, 8, 8, report));
    check(report.distinctPairs == 4 && report.progressEvidencePairs == 2);
    check(report.insufficientPairs == 2 && report.crossSectorConflictPairs == 0);

    EvidenceTrial opposing = trial("opposing");
    opposing.rows.push_back(row(0, "DeflectedCandidate", 45));
    sources.trials.push_back(opposing);
    check(auditFinePairSectors(sources, 60, 8, 8, report));
    check(report.conflictedPairs == 2 && report.crossSectorConflictPairs == 2);
    check(report.details[0].outcome.evidence.restrictionSources == 1);

    EvidenceTrialSet mixed {};
    EvidenceTrial trace = trial("mixed");
    trace.rows.push_back(row(0, "Progress", 0));
    trace.rows.push_back(row(4, "BlockedCandidate", 90));
    mixed.trials.push_back(trace);
    check(auditFinePairSectors(mixed, 60, 8, 8, report));
    check(report.conflictedPairs == 2);
    check(report.details[0].outcome.evidence.mixedSources == 1);
    check(report.details[0].outcome.evidence.observedSources == 1);

    EvidenceTrialSet restrictions {};
    EvidenceTrial r1 = trial("r1");
    r1.rows.push_back(row(0, "BlockedCandidate", 0));
    r1.rows.push_back(row(4, "BlockedCandidate", 90));
    restrictions.trials.push_back(r1);
    EvidenceTrial r2 = trial("r2");
    r2.rows.push_back(row(0, "DeflectedCandidate", 180));
    r2.rows.push_back(row(4, "DeflectedCandidate", 270));
    restrictions.trials.push_back(r2);
    check(auditFinePairSectors(restrictions, 60, 8, 8, report));
    check(report.restrictionEvidencePairs == 2);

    EvidenceTrialSet none {};
    EvidenceTrial shortRecord = trial("short");
    shortRecord.rows.push_back(row(0, "Progress", 0));
    none.trials.push_back(shortRecord);
    check(auditFinePairSectors(none, 1e3, 1e3, 1e3, report));
    check(report.distinctPairs == 0);
    report.distinctPairs = 99;
    check(!auditFinePairSectors(sources, 1e40, 8, 8, report));
    check(report.distinctPairs == 99);
    return 0;
  }
  catch (const std::exception &e) {
    std::fprintf(stderr, "fine pair sectors: %s\n", e.what());
    return 1;
  }
}
