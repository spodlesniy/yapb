#pragma once

#include <algorithm>
#include <map>
#include <tuple>
#include <utility>
#include <vector>

#include <aipb/explorer/engine_bridge_fine_trial_gaps.h>

namespace aipb::explorer::bridge {

inline constexpr int kMaxFineTrialPlansPrinted = 16;

enum class FineTrialPlanKind {
  CorroborateProgress,
  InvestigateRestriction,
  ReviewConflict
};

inline const char *fineTrialPlanKindName(FineTrialPlanKind kind) {
  switch (kind) {
  case FineTrialPlanKind::CorroborateProgress:
    return "CorroborateProgress";
  case FineTrialPlanKind::InvestigateRestriction:
    return "InvestigateRestriction";
  case FineTrialPlanKind::ReviewConflict:
    return "ReviewConflict";
  }

  return "Invalid";
}

struct FineTrialPlan {
  FineTrialGap gap {};
  FineTrialPlanKind kind { FineTrialPlanKind::ReviewConflict };
  int priority {};
  int missingStrongNow { -1 };
  int weakSourcesEligible {};
  int weakCompletionWindows {};
  int newSourcesAfterWeakCompletion { -1 };
  int windowsPerNewSource { kMinimumSourceWindows };
};

struct FineTrialPlanAudit {
  FineTrialGapAudit gaps {};
  int candidates {};
  int progressPlans {};
  int restrictionPlans {};
  int conflictReviews {};
  int completedGroups {};
  int stationaryControls {};
  int weakCompletionWindows {};
  std::vector<FineTrialPlan> details {};
};

// A new independent source means a separate trial provenance, not another
// window or yaw sector from the same source. No command executes a trial.
inline bool auditFineTrialPlans(
  const EvidenceTrialSet &sources,
  double x,
  double y,
  double z,
  FineTrialPlanAudit &result
) {
  FineTrialPlanAudit next {};

  if (!auditFineTrialGaps(sources, x, y, z, next.gaps)) {
    return false;
  }

  using Key = std::tuple<SpatialCell, int, int>;
  using SourceWindows = std::map<int, DirectedCrossingSourceWindows>;
  std::map<Key, SourceWindows> observations {};

  for (const FineZeroExample &record : next.gaps.refinement.targets.zeros.records) {
    const int travel = fineRefinementTravelBin(record);

    if (travel == kFineRefinementStationary) {
      continue;
    }

    const Key key {
      fineRefinementStartSubcell(record),
      record.headingBin,
      travel
    };
    DirectedCrossingSourceWindows &windows = observations[key][record.source];

    if (record.classification == MovementClassification::Progress) {
      ++windows.progress;
    }
    else {
      ++windows.restriction;
    }
  }

  for (const FineTrialGap &gap : next.gaps.details) {
    if (gap.group.travelBin == kFineRefinementStationary) {
      ++next.stationaryControls;
      continue;
    }

    if (gap.movement.status == DirectionAssessmentStatus::ProgressEvidence
      || gap.movement.status == DirectionAssessmentStatus::RestrictionEvidence) {
      ++next.completedGroups;
      continue;
    }

    if (gap.movement.status == DirectionAssessmentStatus::Unobserved) {
      return false;
    }

    FineTrialPlan plan {};
    plan.gap = gap;
    plan.missingStrongNow = gap.missingStrongSources;

    if (gap.movement.status == DirectionAssessmentStatus::Conflicted) {
      plan.kind = FineTrialPlanKind::ReviewConflict;
      plan.priority = 0;
      ++next.conflictReviews;
    }
    else {
      const bool progress = gap.movement.progressSources > 0
        && gap.movement.restrictionSources == 0;
      const bool restriction = gap.movement.restrictionSources > 0
        && gap.movement.progressSources == 0;

      if (!progress && !restriction) {
        return false;
      }

      plan.kind = progress
        ? FineTrialPlanKind::CorroborateProgress
        : FineTrialPlanKind::InvestigateRestriction;
      plan.priority = restriction ? 1 : 2;

      const Key key {
        gap.group.startSubcell,
        gap.group.headingBin,
        gap.group.travelBin
      };
      const auto found = observations.find(key);

      if (found == observations.end()) {
        return false;
      }

      int strongSources {};
      int observedSources {};

      for (const auto &entry : found->second) {
        const DirectedCrossingSourceWindows &windows = entry.second;
        const int total = windows.progress + windows.restriction;

        if (total <= 0 || (progress && windows.restriction > 0)
          || (restriction && windows.progress > 0)) {
          return false;
        }

        ++observedSources;

        if (total >= kMinimumSourceWindows) {
          ++strongSources;
        }
        else {
          ++plan.weakSourcesEligible;
          plan.weakCompletionWindows += kMinimumSourceWindows - total;
        }
      }

      const int recordedStrong = progress
        ? gap.movement.strongProgressSources
        : gap.movement.strongRestrictionSources;

      if (observedSources != gap.movement.observedSources
        || strongSources != recordedStrong
        || plan.weakSourcesEligible != gap.movement.weakSources
        || plan.missingStrongNow != kMinimumCorroboratingSources - strongSources) {
        return false;
      }

      plan.newSourcesAfterWeakCompletion = std::max(0,
        kMinimumCorroboratingSources - strongSources - plan.weakSourcesEligible);
      next.weakCompletionWindows += plan.weakCompletionWindows;

      if (progress) {
        ++next.progressPlans;
      }
      else {
        ++next.restrictionPlans;
      }
    }

    next.details.push_back(std::move(plan));
  }

  next.candidates = static_cast<int>(next.details.size());

  if (next.candidates != next.gaps.insufficientGroups + next.gaps.conflictedGroups
    || next.completedGroups != next.gaps.progressEvidenceGroups
      + next.gaps.restrictionEvidenceGroups
    || next.stationaryControls != next.gaps.stationaryOnlyGroups) {
    return false;
  }

  // Conflicts are reviewed first; potential restrictions are reviewed before
  // ordinary progress corroboration. Source quality breaks equivalent ties.
  std::sort(next.details.begin(), next.details.end(),
    [](const FineTrialPlan &a, const FineTrialPlan &b) {
      if (a.priority != b.priority) {
        return a.priority < b.priority;
      }

      const int aStrong = a.gap.movement.strongProgressSources
        + a.gap.movement.strongRestrictionSources;
      const int bStrong = b.gap.movement.strongProgressSources
        + b.gap.movement.strongRestrictionSources;

      if (aStrong != bStrong) {
        return aStrong > bStrong;
      }

      if (a.gap.group.startSubcell < b.gap.group.startSubcell) {
        return true;
      }

      if (b.gap.group.startSubcell < a.gap.group.startSubcell) {
        return false;
      }

      if (a.gap.group.headingBin != b.gap.group.headingBin) {
        return a.gap.group.headingBin < b.gap.group.headingBin;
      }

      return a.gap.group.travelBin < b.gap.group.travelBin;
    });

  result = std::move(next);
  return true;
}

} // namespace aipb::explorer::bridge
