#include <cstdio>
#include <stdexcept>
#include <string>

#include <aipb/explorer/engine_bridge_source_consensus.h>

using namespace aipb::explorer::bridge;

static void require(bool ok) {
  if (!ok) {
    throw std::runtime_error("source consensus regression");
  }
}

static std::string row(int step, int x, const char *classification, int dy) {
  char out[768] {};
  const char *confirmation = std::strcmp(classification, "Progress") == 0
    ? "direct" : "confirming";
  const int n = std::snprintf(out, sizeof(out),
    "%d,%d,%d,%s,%s,unknown,100,%d,0,0,%d,%d,0,0,25,5,5,0\n",
    step, step + 3, step, classification, confirmation, x, x + 5, dy);
  require(n > 0 && static_cast<std::size_t>(n) < sizeof(out));
  return out;
}

static EvidenceTrial trial(const char *name, int start, int progress, int blocked) {
  EvidenceTrial t {};
  t.name = name;
  t.botIndex = 0;
  t.sourceSteps = 100;

  for (int i = 0; i < progress; ++i) {
    t.rows.push_back(row(i * 4, start + i * 7, "Progress", 0));
  }

  for (int i = 0; i < blocked; ++i) {
    t.rows.push_back(row((i + progress) * 4, start + (i + progress) * 7,
      "BlockedCandidate", 0));
  }

  return t;
}

int main() {
  try {
    EvidenceTrialSet sources {};
    sources.trials.push_back(trial("progress_a", 2, 8, 0));
    SourceConsensus report {};
    require(consolidateSourceEvidence(sources, 10, 0, 0, 0, report));
    require(report.status == DirectionAssessmentStatus::Insufficient);
    require(report.strongProgressSources == 1 && report.windows == 8);
    sources.trials.push_back(trial("weak", 65, 1, 0));
    require(consolidateSourceEvidence(sources, 10, 0, 0, 0, report));
    require(report.status == DirectionAssessmentStatus::Insufficient);
    require(report.weakSources == 1 && report.strongProgressSources == 1);
    sources.trials.push_back(trial("progress_b", 75, 2, 0));
    require(consolidateSourceEvidence(sources, 10, 0, 0, 0, report));
    require(report.status == DirectionAssessmentStatus::ProgressEvidence);
    require(report.strongProgressSources == 2);
    sources.trials.push_back(trial("restricted", 85, 0, 2));
    require(consolidateSourceEvidence(sources, 10, 0, 0, 0, report));
    require(report.status == DirectionAssessmentStatus::Conflicted);
    require(report.strongRestrictionSources == 1 && report.mixedSources == 0);
    EvidenceTrialSet mixed {};
    mixed.trials.push_back(trial("mixed", 5, 3, 3));
    require(consolidateSourceEvidence(mixed, 10, 0, 0, 0, report));
    require(report.status == DirectionAssessmentStatus::Conflicted);
    require(report.mixedSources == 1 && report.strongProgressSources == 0
      && report.strongRestrictionSources == 0);
    EvidenceTrialSet negative {};
    negative.trials.push_back(trial("negative_a", 5, 0, 2));
    negative.trials.push_back(trial("negative_b", 35, 0, 2));
    require(consolidateSourceEvidence(negative, 10, 0, 0, 0, report));
    require(report.status == DirectionAssessmentStatus::RestrictionEvidence);
    require(report.strongRestrictionSources == 2);
    require(consolidateSourceEvidence(negative, 10, 0, 0, 90, report));
    require(report.status == DirectionAssessmentStatus::Unobserved);
    report.windows = 77;
    require(!consolidateSourceEvidence(negative, 1e40, 0, 0, 0, report));
    require(report.windows == 77);
    return 0;
  }
  catch (const std::exception &error) {
    std::fprintf(stderr, "source consensus: %s\n", error.what());
    return 1;
  }
}
