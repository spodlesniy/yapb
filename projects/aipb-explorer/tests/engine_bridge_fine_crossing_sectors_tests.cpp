#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

#include <aipb/explorer/engine_bridge_fine_crossing_sectors.h>

using namespace aipb::explorer::bridge;

static void require(bool ok) {
  if (!ok) {
    throw std::runtime_error("fine crossing sectors regression");
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
  const int written = std::snprintf(output, sizeof(output),
    "%d,%d,%d,%s,%s,unknown,100,%.17g,%.17g,8,%.17g,%.17g,8,%.17g,25,5,5,0\n",
    step, step + 3, step, kind,
    std::string(kind) == "Progress" ? "direct" : "confirming",
    start, y, end, y, heading);
  require(written > 0 && static_cast<std::size_t>(written) < sizeof(output));

  return output;
}

static EvidenceTrial trial(const char *name) {
  EvidenceTrial source {};
  source.name = name;
  source.botIndex = 0;
  source.sourceSteps = 100;
  return source;
}

int main() {
  try {
    EvidenceTrialSet sources {};
    EvidenceTrial first = trial("first");
    first.rows.push_back(row(0, "Progress", 8, 112, 8, 0));
    first.rows.push_back(row(4, "Progress", 9, 111, 8, 90));
    first.rows.push_back(row(8, "Progress", 10, 110, 8, 180));
    sources.trials.push_back(first);
    FineSectorAudit audit {};
    require(auditFineCrossingSectors(sources, 60, 8, 8, audit));
    require(audit.faces == 3 && audit.interiorFaces == 3 && audit.coarseFaces == 0);
    require(audit.multipleSectorFaces == 3 && audit.insufficientDirections == 3);
    require(audit.unobservedDirections == 3 && audit.progressEvidenceDirections == 0);
    require(audit.details[0].position[0] == 64);
    require(audit.details[0].positive.evidence.progressWindows == 3);
    require(audit.details[0].positive.evidence.strongProgressSources == 1);
    require(audit.details[0].positive.progressBins == 0x15u);

    EvidenceTrial second = trial("second");
    second.rows.push_back(row(0, "Progress", 8, 112, 8, 270));
    second.rows.push_back(row(4, "Progress", 9, 111, 8, 315));
    sources.trials.push_back(second);
    require(auditFineCrossingSectors(sources, 60, 8, 8, audit));
    require(audit.progressEvidenceDirections == 3 && audit.unobservedDirections == 3);
    require(audit.details[0].positive.evidence.strongProgressSources == 2);

    EvidenceTrial reverse = trial("reverse");
    reverse.rows.push_back(row(0, "BlockedCandidate", 112, 8, 8, 0));
    reverse.rows.push_back(row(4, "DeflectedCandidate", 111, 9, 8, 90));
    sources.trials.push_back(reverse);
    require(auditFineCrossingSectors(sources, 60, 8, 8, audit));
    require(audit.details[0].positive.evidence.status == DirectionAssessmentStatus::ProgressEvidence);
    require(audit.details[0].negative.evidence.status == DirectionAssessmentStatus::Insufficient);
    require(audit.crossSectorConflictDirections == 0);

    EvidenceTrial opposing = trial("opposing");
    opposing.rows.push_back(row(0, "DeflectedCandidate", 8, 112, 8, 45));
    sources.trials.push_back(opposing);
    require(auditFineCrossingSectors(sources, 60, 8, 8, audit));
    require(audit.conflictedDirections == 3 && audit.crossSectorConflictDirections == 3);
    require(audit.details[0].positive.evidence.status == DirectionAssessmentStatus::Conflicted);
    require(audit.details[0].negative.evidence.status == DirectionAssessmentStatus::Insufficient);

    // Mixed observations from one trace remain one conflicted source.
    EvidenceTrialSet mixed {};
    EvidenceTrial one = trial("mixed");
    one.rows.push_back(row(0, "Progress", 8, 112, 8, 0));
    one.rows.push_back(row(4, "BlockedCandidate", 9, 111, 8, 90));
    mixed.trials.push_back(one);
    require(auditFineCrossingSectors(mixed, 60, 8, 8, audit));
    require(audit.details[0].positive.evidence.mixedSources == 1);
    require(audit.details[0].positive.evidence.observedSources == 1);
    require(audit.details[0].positive.evidence.status == DirectionAssessmentStatus::Conflicted);

    // Separate physical faces never corroborate one another.
    EvidenceTrialSet separate {};
    EvidenceTrial near = trial("near");
    near.rows.push_back(row(0, "Progress", 8, 112, 8, 0));
    near.rows.push_back(row(4, "Progress", 9, 111, 8, 90));
    separate.trials.push_back(near);
    EvidenceTrial far = trial("far");
    far.rows.push_back(row(0, "Progress", 8, 112, 40, 180));
    far.rows.push_back(row(4, "Progress", 9, 111, 40, 270));
    separate.trials.push_back(far);
    require(auditFineCrossingSectors(separate, 60, 8, 8, audit));
    require(audit.faces == 6 && audit.insufficientDirections == 6);
    require(audit.unobservedDirections == 6);

    // The 128-unit plane remains a single fine face.
    EvidenceTrialSet coarse {};
    EvidenceTrial crossing = trial("coarse");
    crossing.rows.push_back(row(0, "Progress", 120, 136, 8, 0));
    coarse.trials.push_back(crossing);
    require(auditFineCrossingSectors(coarse, 124, 8, 8, audit));
    require(audit.faces == 1 && audit.coarseFaces == 1);
    require(audit.details[0].lowerCell.x == 3);

    // Endpoint contact is not a crossing, and invalid input is transactional.
    EvidenceTrialSet touching {};
    EvidenceTrial tangent = trial("tangent");
    tangent.rows.push_back(row(0, "Progress", 24, 32, 8, 0));
    touching.trials.push_back(tangent);
    require(auditFineCrossingSectors(touching, 16, 8, 8, audit));
    require(audit.faces == 0 && audit.details.empty());
    audit.faces = 77;
    require(!auditFineCrossingSectors(sources, 1e40, 8, 8, audit));
    require(audit.faces == 77);
    return 0;
  }
  catch (const std::exception &error) {
    std::fprintf(stderr, "fine crossing sectors: %s\n", error.what());
    return 1;
  }
}
