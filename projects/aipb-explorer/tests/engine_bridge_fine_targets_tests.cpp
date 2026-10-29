#include <cstdio>
#include <stdexcept>
#include <string>

#include <aipb/explorer/engine_bridge_fine_targets.h>

using namespace aipb::explorer::bridge;

static void require(bool ok) {
  if (!ok) {
    throw std::runtime_error("fine targets regression");
  }
}

static std::string row(int step, double sx, double ex, double yaw) {
  char out[512] {};
  const int n = std::snprintf(out, sizeof(out),
    "%d,%d,%d,Progress,direct,unknown,100,%.17g,8,8,%.17g,8,8,%.17g,25,5,5,0\n",
    step, step + 3, step, sx, ex, yaw);
  require(n > 0 && static_cast<std::size_t>(n) < sizeof(out));
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
    EvidenceTrial a = trial("a");
    a.rows.push_back(row(0, 8, 8, 0));
    a.rows.push_back(row(4, 8, 8, 0));
    a.rows.push_back(row(8, 8, 20, 90));
    a.rows.push_back(row(12, 40, 48, 0));
    sources.trials.push_back(a);
    EvidenceTrial b = trial("b");
    b.rows.push_back(row(0, 8, 16, 0));
    sources.trials.push_back(b);
    FineTargetAudit report {};
    require(auditFineTargets(sources, 60, 8, 8, report));
    require(report.zeros.coverage.zeroFaceRecords == 5);
    require(report.groupedWindows == 5 && report.distinctSources == 2);
    require(report.groups == 3 && report.targets.size() == 3);
    require(report.targets[0].headingBin == 2 && report.targets[0].sources == 1);
    require(report.targets[1].cell.x == 1 && report.targets[1].sources == 1);
    require(report.targets[2].headingBin == 0 && report.targets[2].sources == 2);
    require(report.targets[2].windows == 3 && report.targets[2].stationary == 2);
    report.groups = 77;
    require(!auditFineTargets(sources, 10000000, 8, 8, report));
    require(report.groups == 77);
    return 0;
  }
  catch (const std::exception &e) {
    std::fprintf(stderr, "fine targets: %s\n", e.what());
    return 1;
  }
}
