#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

#include <aipb/explorer/engine_bridge_fine_trial_plans.h>

using namespace aipb::explorer::bridge;

static void require(bool condition) {
  if (!condition) {
    throw std::runtime_error("fine trial plans regression");
  }
}

static std::string row(
  int step,
  const char *kind,
  double sx,
  double ex
) {
  char buffer[512] {};
  const int written = std::snprintf(buffer, sizeof(buffer),
    "%d,%d,%d,%s,%s,unknown,100,%.17g,9,9,%.17g,9,9,0,25,5,5,0\n",
    step, step + 3, step, kind,
    std::string(kind) == "Progress" ? "direct" : "confirming",
    sx, ex);
  require(written > 0 && static_cast<std::size_t>(written) < sizeof(buffer));

  return buffer;
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
    first.rows.push_back(row(0, "Progress", 9, 13));
    first.rows.push_back(row(4, "Progress", 25, 29));
    first.rows.push_back(row(8, "Progress", 26, 30));
    first.rows.push_back(row(12, "BlockedCandidate", 41, 45));
    first.rows.push_back(row(16, "Progress", 57, 61));
    first.rows.push_back(row(20, "DeflectedCandidate", 58, 62));
    first.rows.push_back(row(24, "BlockedCandidate", 72, 72));
    first.rows.push_back(row(28, "BlockedCandidate", 72, 72));
    sources.trials.push_back(first);

    EvidenceTrial second = trial("second");
    second.rows.push_back(row(0, "Progress", 25, 29));
    second.rows.push_back(row(4, "Progress", 26, 30));
    second.rows.push_back(row(8, "BlockedCandidate", 57, 61));
    sources.trials.push_back(second);

    FineTrialPlanAudit audit {};
    require(auditFineTrialPlans(sources, 9, 9, 9, audit));
    require(audit.gaps.groups == 5 && audit.gaps.movingGroups == 4);
    require(audit.candidates == 3 && audit.completedGroups == 1);
    require(audit.stationaryControls == 1 && audit.weakCompletionWindows == 2);
    require(audit.progressPlans == 1 && audit.restrictionPlans == 1);
    require(audit.conflictReviews == 1 && audit.details.size() == 3);

    const FineTrialPlan &conflict = audit.details[0];
    require(conflict.priority == 0 && conflict.kind == FineTrialPlanKind::ReviewConflict);
    require(conflict.gap.group.startSubcell.x == 7);
    require(conflict.gap.movement.status == DirectionAssessmentStatus::Conflicted);
    require(conflict.missingStrongNow == -1 && conflict.newSourcesAfterWeakCompletion == -1);

    const FineTrialPlan &restriction = audit.details[1];
    require(restriction.priority == 1);
    require(restriction.kind == FineTrialPlanKind::InvestigateRestriction);
    require(restriction.gap.group.startSubcell.x == 5);
    require(restriction.missingStrongNow == 2 && restriction.weakSourcesEligible == 1);
    require(restriction.weakCompletionWindows == 1);
    require(restriction.newSourcesAfterWeakCompletion == 1);
    require(restriction.windowsPerNewSource == kMinimumSourceWindows);

    const FineTrialPlan &progress = audit.details[2];
    require(progress.priority == 2);
    require(progress.kind == FineTrialPlanKind::CorroborateProgress);
    require(progress.gap.group.startSubcell.x == 1);
    require(progress.missingStrongNow == 2 && progress.weakSourcesEligible == 1);
    require(progress.weakCompletionWindows == 1);
    require(progress.newSourcesAfterWeakCompletion == 1);

    // A different trial with two windows increases independent strong sources.
    EvidenceTrial third = trial("third");
    third.rows.push_back(row(0, "Progress", 9, 13));
    third.rows.push_back(row(4, "Progress", 10, 14));
    sources.trials.push_back(third);
    require(auditFineTrialPlans(sources, 9, 9, 9, audit));
    require(audit.candidates == 3 && audit.weakCompletionWindows == 2);
    require(audit.details[2].missingStrongNow == 1);
    require(audit.details[2].weakSourcesEligible == 1);
    require(audit.details[2].newSourcesAfterWeakCompletion == 0);

    // Strengthening the original source is not creating a fourth source.
    sources.trials[0].rows.push_back(row(32, "Progress", 10, 14));
    require(auditFineTrialPlans(sources, 9, 9, 9, audit));
    require(audit.candidates == 2 && audit.completedGroups == 2);
    require(audit.progressPlans == 0 && audit.weakCompletionWindows == 1);
    require(audit.conflictReviews == 1 && audit.restrictionPlans == 1);

    // Only stationary controls never produce repeat-trial instructions.
    EvidenceTrialSet stationary {};
    EvidenceTrial idle = trial("idle");
    idle.rows.push_back(row(0, "BlockedCandidate", 72, 72));
    idle.rows.push_back(row(4, "BlockedCandidate", 72, 72));
    stationary.trials.push_back(idle);
    require(auditFineTrialPlans(stationary, 9, 9, 9, audit));
    require(audit.candidates == 0 && audit.stationaryControls == 1);
    require(audit.weakCompletionWindows == 0 && audit.details.empty());

    // A failed query must not replace a previously published audit value.
    audit.candidates = 77;
    require(!auditFineTrialPlans(sources, 1e40, 9, 9, audit));
    require(!auditFineTrialPlans(sources, 9, 9, std::nan(""), audit));
    EvidenceTrialSet empty {};
    require(!auditFineTrialPlans(empty, 9, 9, 9, audit));
    require(audit.candidates == 77);
    return 0;
  }
  catch (const std::exception &error) {
    std::fprintf(stderr, "fine trial plans: %s\n", error.what());
    return 1;
  }
}
