#include <cstdio>
#include <stdexcept>

#include <aipb/explorer/engine_bridge_trial_corroboration.h>

using namespace aipb::explorer::bridge;

static void require(bool value) {
  if (!value) {
    throw std::runtime_error("trial corroboration regression");
  }
}

static void source(const char *path, int x, int bot) {
  std::FILE *f = openTraceFile(path, "wt");
  require(f != nullptr);
  require(std::fprintf(f, "aipb-explorer-observations-v2,de_dust2,%d,100\n", bot) > 0);
  require(std::fputs(kObservationCsvColumns, f) >= 0);
  require(std::fprintf(f, "0,3,0,Progress,direct,unknown,100,%d,0,0,%d,0,0,0,25,5,5,0\n", x, x + 5) > 0);
  require(std::fprintf(f, "4,7,4,Progress,direct,unknown,100,%d,0,0,%d,0,0,0,25,5,5,0\n", x + 6, x + 11) > 0);
  require(std::fclose(f) == 0);
}

int main() {
  const char *a = "trial_a.observations.csv";
  const char *b = "trial_b.observations.csv";
  const char *snapshot = "trial_ab.trials.csv";
  const char *duplicate = "trial_duplicate.trials.csv";
  std::remove(a); std::remove(b); std::remove(snapshot); std::remove(duplicate);

  try {
    source(a, 0, 0);
    source(b, 20, 1);
    EvidenceTrialReport written {};
    require(writeEvidenceTrialSet(a, "trial_a", b, "trial_b", snapshot, "de_dust2", written));
    require(written.trials == 2 && written.rows == 4);
    require(!writeEvidenceTrialSet(a, "trial_a", b, "trial_b", snapshot, "de_dust2", written));
    require(!writeEvidenceTrialSet(a, "trial_a", a, "trial_a", duplicate, "de_dust2", written));
    EvidenceTrialSet set {};
    require(readEvidenceTrialSet(snapshot, "de_dust2", set));
    require(!readEvidenceTrialSet(snapshot, "de_inferno", set));
    TrialCorroboration result {};
    require(corroborateTrialDirection(set, 32, 0, 0, 0, result));
    require(result.sources == 2 && result.observedSources == 2);
    require(result.progressSources == 2 && result.observations == 4);
    require(result.status == DirectionAssessmentStatus::ProgressEvidence);
    require(corroborateTrialDirection(set, 32, 0, 0, 90, result));
    require(result.status == DirectionAssessmentStatus::Unobserved);
    EvidenceTrialSet onlyOne {};
    require(readRawEvidenceTrial(a, "de_dust2", "trial_a", onlyOne));
    require(corroborateTrialDirection(onlyOne, 2, 0, 0, 0, result));
    require(result.observations == 2 && result.progressSources == 1);
    require(result.status == DirectionAssessmentStatus::Insufficient);
    require(!appendEvidenceTrials(onlyOne, onlyOne));
    result.observations = 42;
    require(!corroborateTrialDirection(set, 1e40, 0, 0, 0, result));
    require(result.observations == 42);
    std::remove(a); std::remove(b); std::remove(snapshot); std::remove(duplicate);
    return 0;
  }
  catch (const std::exception &error) {
    std::fprintf(stderr, "trial corroboration: %s\n", error.what());
    std::remove(a); std::remove(b); std::remove(snapshot); std::remove(duplicate);
    return 1;
  }
}
