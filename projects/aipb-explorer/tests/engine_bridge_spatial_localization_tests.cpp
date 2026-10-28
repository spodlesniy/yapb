#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>

#include <aipb/explorer/engine_bridge_spatial_localization.h>

using namespace aipb::explorer::bridge;

static void require(bool valid) {
  if (!valid) {
    throw std::runtime_error("spatial localization regression");
  }
}

static std::string observation(int step, const char *classification,
  double startX, double startY, double endX, double endY, double z = 8.0) {
  char row[768] {};
  const bool progress = std::string(classification) == "Progress";
  const int length = std::snprintf(row, sizeof(row),
    "%d,%d,%d,%s,%s,unknown,100,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,0,25,4,4,0\n",
    step, step + 3, step, classification, progress ? "direct" : "confirming",
    startX, startY, z, endX, endY, z);
  require(length > 0 && static_cast<std::size_t>(length) < sizeof(row));
  return row;
}

static EvidenceTrial makeTrial(const char *name, std::string first, std::string second) {
  EvidenceTrial trial {};
  trial.name = name;
  trial.botIndex = 0;
  trial.sourceSteps = 100;
  trial.rows.push_back(std::move(first));
  trial.rows.push_back(std::move(second));
  return trial;
}

int main() {
  try {
    EvidenceTrialSet sources {};
    sources.trials.push_back(makeTrial("progress_a",
      observation(0, "Progress", 10, 8, 14, 8),
      observation(4, "Progress", 16, 8, 20, 8)));
    sources.trials.push_back(makeTrial("progress_b",
      observation(0, "Progress", 12, 9, 16, 9),
      observation(4, "Progress", 20, 9, 24, 9)));
    sources.trials.push_back(makeTrial("wall",
      observation(0, "BlockedCandidate", 100, 8, 100, 8),
      observation(4, "BlockedCandidate", 101, 8, 101, 8)));

    LocalizedSourceEvidence result {};
    require(localizeSourceEvidence(sources, 10, 8, 8, 0, result));
    require(result.coarse.status == DirectionAssessmentStatus::Conflicted);
    require(result.coarse.windows == 6);
    require(result.fineCell.x == 0 && result.fineCell.y == 0 && result.fineCell.z == 0);
    require(result.fine.status == DirectionAssessmentStatus::ProgressEvidence);
    require(result.fine.windows == 4 && result.fine.strongProgressSources == 2);
    require(result.fine.restrictionSources == 0);

    require(localizeSourceEvidence(sources, 100, 8, 8, 0, result));
    require(result.coarse.status == DirectionAssessmentStatus::Conflicted);
    require(result.fine.status == DirectionAssessmentStatus::Insufficient);
    require(result.fine.windows == 2 && result.fine.strongRestrictionSources == 1);
    require(result.fineCell.x == 3);

    require(localizeSourceEvidence(sources, 50, 8, 8, 0, result));
    require(result.coarse.status == DirectionAssessmentStatus::Conflicted);
    require(result.fine.status == DirectionAssessmentStatus::Unobserved);

    require(localizeSourceEvidence(sources, 10, 8, 8, 90, result));
    require(result.coarse.status == DirectionAssessmentStatus::Unobserved);
    require(result.fine.status == DirectionAssessmentStatus::Unobserved);

    sources.trials.push_back(makeTrial("diagonal",
      observation(0, "Progress", 10, 10, 110, 110),
      observation(4, "Progress", 12, 12, 108, 108)));
    require(localizeSourceEvidence(sources, 10, 110, 8, 0, result));
    require(result.coarse.windows == 8);
    require(result.fine.status == DirectionAssessmentStatus::Unobserved);
    require(localizeSourceEvidence(sources, 64, 64, 8, 0, result));
    require(result.fine.windows == 2 && result.fine.strongProgressSources == 1);
    require(result.fine.status == DirectionAssessmentStatus::Insufficient);

    EvidenceTrialSet negative {};
    negative.trials.push_back(makeTrial("negative",
      observation(0, "Progress", -60, -10, -58, -10, -90),
      observation(4, "Progress", -55, -10, -52, -10, -90)));
    require(localizeSourceEvidence(negative, -55, -10, -90, -360, result));
    require(result.fineCell.x == -2 && result.fineCell.y == -1 && result.fineCell.z == -3);
    require(result.fine.status == DirectionAssessmentStatus::Insufficient);
    require(result.fine.windows == 2);

    result.fine.windows = 77;
    require(!localizeSourceEvidence(sources, 1e40, 0, 0, 0, result));
    require(result.fine.windows == 77);
    require(!localizeSourceEvidence(sources, 0, 0, 0, std::nan(""), result));
    require(result.fine.windows == 77);
    return 0;
  }
  catch (const std::exception &error) {
    std::fprintf(stderr, "spatial localization: %s\n", error.what());
    return 1;
  }
}
