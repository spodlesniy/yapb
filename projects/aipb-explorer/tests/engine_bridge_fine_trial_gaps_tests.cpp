#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

#include <aipb/explorer/engine_bridge_fine_trial_gaps.h>

using namespace aipb::explorer::bridge;

static void check(bool value) {
  if (!value) {
    throw std::runtime_error("fine trial gaps regression");
  }
}

static std::string row(
  int step,
  const char *kind,
  double sx,
  double ex
) {
  char buffer[512] {};
  const int n = std::snprintf(buffer, sizeof(buffer),
    "%d,%d,%d,%s,%s,unknown,100,%.17g,9,9,%.17g,9,9,0,25,5,5,0\n",
    step, step + 3, step, kind,
    std::string(kind) == "Progress" ? "direct" : "confirming", sx, ex);
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
    EvidenceTrialSet sources {};
    EvidenceTrial a = trial("a");
    a.rows.push_back(row(0, "Progress", 9, 13));
    a.rows.push_back(row(4, "Progress", 10, 14));
    a.rows.push_back(row(8, "BlockedCandidate", 9, 9));
    a.rows.push_back(row(12, "BlockedCandidate", 9, 9));
    sources.trials.push_back(a);

    FineTrialGapAudit audit {};
    check(auditFineTrialGaps(sources, 9, 9, 9, audit));
    check(audit.groups == 2 && audit.movingGroups == 1);
    check(audit.stationaryOnlyGroups == 1 && audit.movingSources == 1);
    check(audit.insufficientGroups == 1);
    check(audit.details[0].movement.strongProgressSources == 1);
    check(audit.details[0].missingStrongSources == 1);
    check(audit.details[1].stationarySources == 1);
    check(audit.details[1].stationaryWindows == 2);

    EvidenceTrial b = trial("b");
    b.rows.push_back(row(0, "Progress", 9, 13));
    b.rows.push_back(row(4, "Progress", 10, 14));
    sources.trials.push_back(b);
    check(auditFineTrialGaps(sources, 9, 9, 9, audit));
    check(audit.progressEvidenceGroups == 1 && audit.insufficientGroups == 0);
    check(audit.details[0].movement.strongProgressSources == 2);
    check(audit.details[0].missingStrongSources == -1);

    EvidenceTrial c = trial("c");
    c.rows.push_back(row(0, "BlockedCandidate", 11, 15));
    sources.trials.push_back(c);
    check(auditFineTrialGaps(sources, 9, 9, 9, audit));
    check(audit.conflictedGroups == 1 && audit.progressEvidenceGroups == 0);
    check(audit.details[0].movement.weakSources == 1);

    audit.groups = 77;
    check(!auditFineTrialGaps(sources, 1e40, 9, 9, audit));
    check(!auditFineTrialGaps(sources, 9, 9, std::nan(""), audit));
    check(audit.groups == 77);
    return 0;
  }
  catch (const std::exception &error) {
    std::fprintf(stderr, "fine trial gaps: %s\n", error.what());
    return 1;
  }
}
