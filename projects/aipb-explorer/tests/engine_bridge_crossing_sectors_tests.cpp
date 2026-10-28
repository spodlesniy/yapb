#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

#include <aipb/explorer/engine_bridge_crossing_sectors.h>

using namespace aipb::explorer::bridge;

static void require(bool ok) {
  if (!ok) {
    throw std::runtime_error("crossing sectors regression");
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
  EvidenceTrial result {};
  result.name = name;
  result.botIndex = 0;
  result.sourceSteps = 100;
  return result;
}

int main() {
  try {
    EvidenceTrialSet sources {};
    EvidenceTrial one = trial("one");
    one.rows.push_back(row(0, "Progress", 120, 136, 8, 0));
    one.rows.push_back(row(4, "Progress", 121, 137, 8, 90));
    one.rows.push_back(row(8, "Progress", 122, 138, 8, 180));
    sources.trials.push_back(one);
    CrossingSectorAudit audit {};
    require(auditCrossingSectors(sources, 124, 8, 8, audit));
    require(audit.faces == 1 && audit.multipleSectorFaces == 1);
    require(audit.details[0].positive.evidence.progressWindows == 3);
    require(audit.details[0].positive.evidence.strongProgressSources == 1);
    require(audit.details[0].positive.evidence.status == DirectionAssessmentStatus::Insufficient);
    require(audit.details[0].positive.progressBins == 0x15u);
    require(audit.unobservedDirections == 1);

    EvidenceTrial two = trial("two");
    two.rows.push_back(row(0, "Progress", 120, 136, 8, 270));
    two.rows.push_back(row(4, "Progress", 121, 137, 8, 315));
    sources.trials.push_back(two);
    require(auditCrossingSectors(sources, 124, 8, 8, audit));
    require(audit.details[0].positive.evidence.status == DirectionAssessmentStatus::ProgressEvidence);
    require(audit.details[0].positive.evidence.strongProgressSources == 2);
    require(audit.progressEvidenceDirections == 1);

    EvidenceTrial reverse = trial("reverse");
    reverse.rows.push_back(row(0, "BlockedCandidate", 140, 120, 8, 0));
    reverse.rows.push_back(row(4, "DeflectedCandidate", 139, 121, 8, 90));
    sources.trials.push_back(reverse);
    require(auditCrossingSectors(sources, 124, 8, 8, audit));
    require(audit.details[0].positive.evidence.status == DirectionAssessmentStatus::ProgressEvidence);
    require(audit.details[0].negative.evidence.status == DirectionAssessmentStatus::Insufficient);
    require(audit.crossSectorConflictDirections == 0);

    EvidenceTrial opposed = trial("opposed");
    opposed.rows.push_back(row(0, "DeflectedCandidate", 123, 139, 8, 45));
    sources.trials.push_back(opposed);
    require(auditCrossingSectors(sources, 124, 8, 8, audit));
    require(audit.details[0].positive.evidence.status == DirectionAssessmentStatus::Conflicted);
    require(audit.crossSectorConflictDirections == 1);
    require(audit.conflictedDirections == 1);
    require(audit.details[0].negative.evidence.status == DirectionAssessmentStatus::Insufficient);

    // One mixed trace remains one source even when the outcomes use different yaw bins.
    EvidenceTrialSet mixed {};
    EvidenceTrial mixedTrial = trial("mixed");
    mixedTrial.rows.push_back(row(0, "Progress", 120, 136, 8, 0));
    mixedTrial.rows.push_back(row(4, "BlockedCandidate", 120, 136, 8, 90));
    mixed.trials.push_back(mixedTrial);
    require(auditCrossingSectors(mixed, 124, 8, 8, audit));
    require(audit.details[0].positive.evidence.mixedSources == 1);
    require(audit.details[0].positive.evidence.observedSources == 1);
    require(audit.details[0].positive.evidence.status == DirectionAssessmentStatus::Conflicted);

    // Evidence for separate 32-unit faces cannot be pooled.
    EvidenceTrialSet spatial {};
    EvidenceTrial a = trial("a");
    a.rows.push_back(row(0, "Progress", 120, 136, 8, 0));
    a.rows.push_back(row(4, "Progress", 121, 137, 8, 90));
    spatial.trials.push_back(a);
    EvidenceTrial b = trial("b");
    b.rows.push_back(row(0, "Progress", 120, 136, 40, 180));
    b.rows.push_back(row(4, "Progress", 121, 137, 40, 270));
    spatial.trials.push_back(b);
    require(auditCrossingSectors(spatial, 124, 8, 8, audit));
    require(audit.faces == 2 && audit.insufficientDirections == 2);
    require(audit.unobservedDirections == 2);

    // Endpoint-only contact and invalid queries retain the conservative contract.
    EvidenceTrialSet touching {};
    EvidenceTrial tangent = trial("tangent");
    tangent.rows.push_back(row(0, "Progress", 120, 128, 8, 0));
    touching.trials.push_back(tangent);
    require(auditCrossingSectors(touching, 124, 8, 8, audit));
    require(audit.faces == 0 && audit.details.empty());
    audit.faces = 77;
    require(!auditCrossingSectors(sources, 1e40, 8, 8, audit));
    require(audit.faces == 77);
    return 0;
  }
  catch (const std::exception &error) {
    std::fprintf(stderr, "crossing sectors: %s\n", error.what());
    return 1;
  }
}
