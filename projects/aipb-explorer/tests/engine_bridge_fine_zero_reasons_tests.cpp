#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

#include <aipb/explorer/engine_bridge_fine_zero_reasons.h>

using namespace aipb::explorer::bridge;

static void check(bool b) {
  if (!b) {
    throw std::runtime_error("fine zero reasons regression");
  }
}

static std::string row(
  int step,
  double sx,
  double sy,
  double sz,
  double ex,
  double ey,
  double ez,
  double heading = 0
) {
  char out[768] {};
  const int n = std::snprintf(out, sizeof(out),
    "%d,%d,%d,Progress,direct,unknown,100,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,25,5,5,0\n",
    step, step + 3, step, sx, sy, sz, ex, ey, ez, heading);
  check(n > 0 && static_cast<std::size_t>(n) < sizeof(out));
  return out;
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
    EvidenceTrial one = trial("one");
    one.rows.push_back(row(0, 700, 8, 8, 710, 8, 8));
    one.rows.push_back(row(4, 8, 8, 8, 8, 8, 8));
    one.rows.push_back(row(8, 8, 8, 8, 20, 8, 8));
    one.rows.push_back(row(12, 24, 8, 8, 32, 8, 8));
    one.rows.push_back(row(16, 32, 8, 8, 40, 8, 8));
    one.rows.push_back(row(20, 8, 8, 8, 112, 8, 8));
    one.rows.push_back(row(24, 8, 0, 8, 112, 0, 8));
    sources.trials.push_back(one);
    EvidenceTrial two = trial("two");
    two.rows.push_back(row(0, 700, 8, 8, 710, 8, 8, 90));
    two.rows.push_back(row(4, 8, 8, 8, 20, 8, 8, 90));
    sources.trials.push_back(two);

    FineZeroAudit audit {};
    check(auditFineZeroReasons(sources, 60, 8, 8, audit));
    check(audit.coverage.records == 9 && audit.coverage.zeroFaceRecords == 7);
    check(audit.total.outside == 2 && audit.total.stationary == 1);
    check(audit.total.sameFineCell == 2 && audit.total.boundaryContact == 2);
    check(audit.total.other == 0 && audit.total.total() == 7);
    check(audit.bins[0].total() == 5 && audit.bins[2].total() == 2);
    check(audit.coverage.ambiguousRecords == 1);
    check(audit.coverage.twoFaceChains == 0 && audit.coverage.threePlusChains == 1);
    check(audit.examples.size() == 7);
    check(audit.examples[0].source == 0 && audit.examples[0].firstStep == 0);
    check(audit.examples[0].reason == FineZeroReason::OutsideWindow);

    // Empty, invalid, and remote queries leave the output transactional.
    audit.total.outside = 77;
    check(!auditFineZeroReasons(sources, 1e40, 8, 8, audit));
    check(!auditFineZeroReasons(sources, 60, 8, std::nan(""), audit));
    EvidenceTrialSet empty {};
    check(!auditFineZeroReasons(empty, 60, 8, 8, audit));
    check(audit.total.outside == 77);
    return 0;
  }
  catch (const std::exception &e) {
    std::fprintf(stderr, "fine zero reasons: %s\n", e.what());
    return 1;
  }
}
