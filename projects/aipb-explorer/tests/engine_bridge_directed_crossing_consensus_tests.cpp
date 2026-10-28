#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>

#include <aipb/explorer/engine_bridge_directed_crossing_consensus.h>

using namespace aipb::explorer::bridge;

static void require(bool ok) {
  if (!ok) {
    throw std::runtime_error("directed crossing consensus regression");
  }
}

static std::string row(
  int step,
  const char *kind,
  double start,
  double end,
  double y,
  double heading
) {
  char output[768] {};
  const bool progress = std::string(kind) == "Progress";
  const int written = std::snprintf(output, sizeof(output),
    "%d,%d,%d,%s,%s,unknown,100,%.17g,%.17g,8,%.17g,%.17g,8,%.17g,25,5,5,0\n",
    step, step + 3, step, kind, progress ? "direct" : "confirming",
    start, y, end, y, heading);
  require(written > 0 && static_cast<std::size_t>(written) < sizeof(output));
  return output;
}

static EvidenceTrial trial(
  const char *name,
  const char *kind,
  int windows,
  bool positive,
  double offset = 0,
  double y = 8,
  double heading = 0
) {
  EvidenceTrial source {};
  source.name = name;
  source.botIndex = 0;
  source.sourceSteps = 100;

  for (int i = 0; i < windows; ++i) {
    const double start = (positive ? 120 : 140) + offset + i * 0.1;
    const double end = (positive ? 136 : 120) + offset + i * 0.1;
    source.rows.push_back(row(i * 4, kind, start, end, y, heading));
  }

  return source;
}

int main() {
  try {
    EvidenceTrialSet sources {};
    sources.trials.push_back(trial("repeated", "Progress", 5, true));
    DirectedCrossingConsensusAudit audit {};
    require(auditDirectedCrossingConsensus(sources, 124, 8, 8, 0, audit));
    require(audit.directions.discovered.faces == 1 && audit.details.size() == 1);
    require(audit.details[0].positive.status == DirectionAssessmentStatus::Insufficient);
    require(audit.details[0].positive.strongProgressSources == 1);
    require(audit.details[0].positive.progressWindows == 5);
    require(audit.details[0].negative.status == DirectionAssessmentStatus::Unobserved);
    require(audit.insufficientDirections == 1 && audit.unobservedDirections == 1);

    // One weak second source is not a second strong independent vote.
    sources.trials.push_back(trial("weak", "Progress", 1, true, 1));
    require(auditDirectedCrossingConsensus(sources, 124, 8, 8, 0, audit));
    require(audit.details[0].positive.status == DirectionAssessmentStatus::Insufficient);
    require(audit.details[0].positive.weakSources == 1);
    sources.trials.push_back(trial("confirmed", "Progress", 2, true, 2));
    require(auditDirectedCrossingConsensus(sources, 124, 8, 8, 0, audit));
    require(audit.details[0].positive.status == DirectionAssessmentStatus::ProgressEvidence);
    require(audit.details[0].positive.strongProgressSources == 2);
    require(audit.details[0].positive.observedSources == 3);
    require(audit.progressEvidenceDirections == 1 && audit.unobservedDirections == 1);

    // Opposite travel signs have isolated status and vote counts even in one yaw bin.
    sources.trials.push_back(trial("reverse_a", "BlockedCandidate", 2, false, 0));
    sources.trials.push_back(trial("reverse_b", "DeflectedCandidate", 2, false, 1));
    require(auditDirectedCrossingConsensus(sources, 124, 8, 8, 0, audit));
    require(audit.details[0].positive.status == DirectionAssessmentStatus::ProgressEvidence);
    require(audit.details[0].negative.status == DirectionAssessmentStatus::RestrictionEvidence);
    require(audit.details[0].negative.strongRestrictionSources == 2);
    require(audit.details[0].negative.restrictionWindows == 4);
    require(audit.progressEvidenceDirections == 1 && audit.restrictionEvidenceDirections == 1);

    // Even a weak opposing window prevents agreement for that sign only.
    sources.trials.push_back(trial("opposing", "BlockedCandidate", 1, true, 3));
    require(auditDirectedCrossingConsensus(sources, 124, 8, 8, 0, audit));
    require(audit.details[0].positive.status == DirectionAssessmentStatus::Conflicted);
    require(audit.details[0].positive.weakSources == 2);
    require(audit.details[0].negative.status == DirectionAssessmentStatus::RestrictionEvidence);
    require(audit.conflictedDirections == 1 && audit.restrictionEvidenceDirections == 1);

    // Mixed outcomes within one trial never constitute two independent sources.
    EvidenceTrialSet mixed {};
    EvidenceTrial mixedTrial = trial("mixed", "Progress", 2, true);
    mixedTrial.rows.push_back(row(8, "BlockedCandidate", 121, 137, 8, 0));
    mixed.trials.push_back(std::move(mixedTrial));
    require(auditDirectedCrossingConsensus(mixed, 124, 8, 8, 0, audit));
    require(audit.details[0].positive.status == DirectionAssessmentStatus::Conflicted);
    require(audit.details[0].positive.mixedSources == 1);
    require(audit.details[0].positive.strongProgressSources == 0);
    require(audit.details[0].negative.status == DirectionAssessmentStatus::Unobserved);

    // A second physical face cannot borrow votes from the first face.
    EvidenceTrialSet distinct {};
    distinct.trials.push_back(trial("face_a", "Progress", 2, true));
    distinct.trials.push_back(trial("face_b", "Progress", 2, true, 1, 40));
    require(auditDirectedCrossingConsensus(distinct, 124, 8, 8, 0, audit));
    require(audit.details.size() == 2 && audit.insufficientDirections == 2);
    require(audit.unobservedDirections == 2);
    require(audit.details[0].positive.strongProgressSources == 1);
    require(audit.details[1].positive.strongProgressSources == 1);

    // Yaw is a sector filter, not an alignment requirement for progress votes.
    EvidenceTrialSet against {};
    against.trials.push_back(trial("backward", "Progress", 2, false, 0, 8, 0));
    against.trials.push_back(trial("backward_2", "Progress", 2, false, 1, 8, 0));
    require(auditDirectedCrossingConsensus(against, 124, 8, 8, 0, audit));
    require(audit.details[0].negative.status == DirectionAssessmentStatus::ProgressEvidence);
    require(audit.directions.againstHeadingWindows == 4);

    // Tangencies and heading mismatches leave no directed faces.
    EvidenceTrialSet touching {};
    touching.trials.push_back(trial("touch", "Progress", 1, true));
    touching.trials[0].rows[0] = row(0, "Progress", 120, 128, 8, 0);
    require(auditDirectedCrossingConsensus(touching, 124, 8, 8, 0, audit));
    require(audit.details.empty() && audit.unobservedDirections == 0);
    require(auditDirectedCrossingConsensus(sources, 124, 8, 8, 90, audit));
    require(audit.details.empty() && audit.directions.discovered.faces == 0);

    audit.progressEvidenceDirections = 99;
    require(!auditDirectedCrossingConsensus(sources, 1e40, 8, 8, 0, audit));
    require(!auditDirectedCrossingConsensus(sources, 124, 8, 8, std::nan(""), audit));
    require(audit.progressEvidenceDirections == 99);
    return 0;
  }
  catch (const std::exception &error) {
    std::fprintf(stderr, "directed crossing consensus: %s\n", error.what());
    return 1;
  }
}
