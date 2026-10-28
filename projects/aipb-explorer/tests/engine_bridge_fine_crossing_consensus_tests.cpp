#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>

#include <aipb/explorer/engine_bridge_fine_crossing_consensus.h>

using namespace aipb::explorer::bridge;

static void require(bool condition) {
  if (!condition) {
    throw std::runtime_error("fine crossing consensus regression");
  }
}

static std::string row(
  int step,
  const char *kind,
  double start,
  double end,
  double y,
  double heading = 0
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
  double y = 8,
  double heading = 0
) {
  EvidenceTrial result {};
  result.name = name;
  result.botIndex = 0;
  result.sourceSteps = 100;

  for (int i = 0; i < windows; ++i) {
    const double start = (positive ? 8 : 112) + i * 0.1;
    const double end = (positive ? 112 : 8) + i * 0.1;
    result.rows.push_back(row(i * 4, kind, start, end, y, heading));
  }

  return result;
}

int main() {
  try {
    FineCrossingConsensusAudit audit {};
    EvidenceTrialSet sources {};
    sources.trials.push_back(trial("one", "Progress", 8, true));
    require(auditFineCrossingConsensus(sources, 60, 8, 8, 0, audit));
    require(audit.discovery.faces == 3 && audit.discovery.interiorFaces == 3);
    require(audit.insufficientDirections == 3 && audit.unobservedDirections == 3);
    require(audit.progressEvidenceDirections == 0);
    require(audit.details[0].crossing.position[0] == 64);
    require(audit.details[0].positive.progressWindows == 8);
    require(audit.details[0].positive.strongProgressSources == 1);
    require(audit.details[0].positive.status == DirectionAssessmentStatus::Insufficient);
    require(audit.details[0].negative.status == DirectionAssessmentStatus::Unobserved);

    // A second weak source does not confirm a strong one.
    sources.trials.push_back(trial("weak", "Progress", 1, true));
    require(auditFineCrossingConsensus(sources, 60, 8, 8, 0, audit));
    require(audit.details[0].positive.weakSources == 1);
    require(audit.details[0].positive.status == DirectionAssessmentStatus::Insufficient);
    sources.trials.push_back(trial("two", "Progress", 2, true));
    require(auditFineCrossingConsensus(sources, 60, 8, 8, 0, audit));
    require(audit.progressEvidenceDirections == 3 && audit.unobservedDirections == 3);
    require(audit.details[0].positive.strongProgressSources == 2);
    require(audit.details[0].positive.observedSources == 3);

    // Reverse restriction crossings have their own source-quality status.
    sources.trials.push_back(trial("blocked", "BlockedCandidate", 2, false));
    sources.trials.push_back(trial("deflected", "DeflectedCandidate", 2, false));
    require(auditFineCrossingConsensus(sources, 60, 8, 8, 0, audit));
    require(audit.progressEvidenceDirections == 3 && audit.restrictionEvidenceDirections == 3);
    require(audit.details[0].positive.status == DirectionAssessmentStatus::ProgressEvidence);
    require(audit.details[0].negative.status == DirectionAssessmentStatus::RestrictionEvidence);
    require(audit.details[0].negative.strongRestrictionSources == 2);
    require(audit.details[0].negative.restrictionWindows == 4);

    // A weak opposing outcome vetoes the same signed face only.
    sources.trials.push_back(trial("opposed", "BlockedCandidate", 1, true));
    require(auditFineCrossingConsensus(sources, 60, 8, 8, 0, audit));
    require(audit.conflictedDirections == 3 && audit.restrictionEvidenceDirections == 3);
    require(audit.details[0].positive.status == DirectionAssessmentStatus::Conflicted);
    require(audit.details[0].negative.status == DirectionAssessmentStatus::RestrictionEvidence);

    // Opposed outcomes from one source are mixed, not independent votes.
    EvidenceTrialSet mixed {};
    EvidenceTrial mixedTrial = trial("mixed", "Progress", 2, true);
    mixedTrial.rows.push_back(row(8, "DeflectedCandidate", 10, 110, 8));
    mixed.trials.push_back(std::move(mixedTrial));
    require(auditFineCrossingConsensus(mixed, 60, 8, 8, 0, audit));
    require(audit.conflictedDirections == 3 && audit.unobservedDirections == 3);
    require(audit.details[0].positive.mixedSources == 1);
    require(audit.details[0].positive.strongProgressSources == 0);

    // Adjacent faces and opposite travel signs do not pool votes.
    EvidenceTrialSet separated {};
    separated.trials.push_back(trial("left", "Progress", 2, true, 8));
    separated.trials.push_back(trial("right", "Progress", 2, true, 40));
    require(auditFineCrossingConsensus(separated, 60, 8, 8, 0, audit));
    require(audit.discovery.faces == 6);
    require(audit.insufficientDirections == 6 && audit.unobservedDirections == 6);
    require(audit.details[0].positive.strongProgressSources == 1);
    require(audit.details[1].positive.strongProgressSources == 1);

    // 128-unit coarse boundaries remain ordinary 32-unit face keys.
    EvidenceTrialSet coarse {};
    coarse.trials.push_back(trial("coarse", "Progress", 2, true));
    coarse.trials[0].rows[0] = row(0, "Progress", 120, 136, 8);
    coarse.trials[0].rows[1] = row(4, "Progress", 121, 137, 8);
    require(auditFineCrossingConsensus(coarse, 124, 8, 8, 0, audit));
    require(audit.discovery.faces == 1 && audit.discovery.coarseFaces == 1);
    require(audit.details[0].crossing.lowerCell.x == 3);
    require(audit.details[0].positive.status == DirectionAssessmentStatus::Insufficient);

    // Heading filters, endpoint contacts and invalid inputs stay conservative.
    require(auditFineCrossingConsensus(sources, 60, 8, 8, 90, audit));
    require(audit.discovery.faces == 0 && audit.details.empty());
    EvidenceTrialSet touching {};
    touching.trials.push_back(trial("touch", "Progress", 1, true));
    touching.trials[0].rows[0] = row(0, "Progress", 24, 32, 8);
    require(auditFineCrossingConsensus(touching, 16, 8, 8, 0, audit));
    require(audit.details.empty());
    audit.progressEvidenceDirections = 77;
    require(!auditFineCrossingConsensus(sources, 1e40, 8, 8, 0, audit));
    require(!auditFineCrossingConsensus(sources, 60, 8, 8, std::nan(""), audit));
    require(audit.progressEvidenceDirections == 77);
    return 0;
  }
  catch (const std::exception &error) {
    std::fprintf(stderr, "fine crossing consensus: %s\n", error.what());
    return 1;
  }
}
