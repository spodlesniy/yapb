#pragma once

#include <algorithm>
#include <map>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include <aipb/explorer/engine_bridge_fine_refinement.h>
#include <aipb/explorer/engine_bridge_directed_crossing_consensus.h>

namespace aipb::explorer::bridge {

inline constexpr int kMaxFineTrialGapsPrinted = 16;

struct FineTrialGap {
  FineRefinementGroup group {};
  DirectedCrossingEvidence movement {};
  int stationaryWindows {};
  int stationarySources {};
  int missingStrongSources { -1 };
};

struct FineTrialGapAudit {
  FineRefinementAudit refinement {};
  int groups {};
  int movingGroups {};
  int stationaryOnlyGroups {};
  int insufficientGroups {};
  int conflictedGroups {};
  int progressEvidenceGroups {};
  int restrictionEvidenceGroups {};
  int movingSources {};
  std::vector<FineTrialGap> details {};
};

inline int missingFineTrialSources(const DirectedCrossingEvidence &evidence) {
  if (evidence.status != DirectionAssessmentStatus::Insufficient) {
    return -1;
  }

  if (evidence.progressSources > 0 && evidence.restrictionSources == 0) {
    return std::max(0, kMinimumCorroboratingSources - evidence.strongProgressSources);
  }

  if (evidence.restrictionSources > 0 && evidence.progressSources == 0) {
    return std::max(0, kMinimumCorroboratingSources - evidence.strongRestrictionSources);
  }

  return -1;
}

// A stationary record is a diagnostic control, never an affirmative vote
// that a particular displacement succeeded or that a wall was hit.
inline bool auditFineTrialGaps(
  const EvidenceTrialSet &sources,
  double x,
  double y,
  double z,
  FineTrialGapAudit &result
) {
  FineTrialGapAudit next {};

  if (!auditFineRefinement(sources, x, y, z, next.refinement)) {
    return false;
  }

  using Key = std::tuple<SpatialCell, int, int>;
  using SourceVotes = std::map<int, DirectedCrossingSourceWindows>;
  std::map<Key, SourceVotes> votes {};
  std::map<Key, std::set<int>> stationary {};
  std::set<int> movingSources {};

  for (const FineZeroExample &record : next.refinement.targets.zeros.records) {
    const Key key {
      fineRefinementStartSubcell(record),
      record.headingBin,
      fineRefinementTravelBin(record)
    };

    if (record.start[0] == record.end[0]
      && record.start[1] == record.end[1]
      && record.start[2] == record.end[2]) {
      stationary[key].insert(record.source);
      continue;
    }

    DirectedCrossingSourceWindows &windows = votes[key][record.source];

    if (record.classification == MovementClassification::Progress) {
      ++windows.progress;
    }
    else {
      ++windows.restriction;
    }

    movingSources.insert(record.source);
  }

  next.movingSources = static_cast<int>(movingSources.size());

  for (const FineRefinementGroup &group : next.refinement.details) {
    const Key key { group.startSubcell, group.headingBin, group.travelBin };
    FineTrialGap gap {};
    gap.group = group;

    if (group.travelBin == kFineRefinementStationary) {
      gap.stationaryWindows = group.windows;
      gap.stationarySources = static_cast<int>(stationary[key].size());
      ++next.stationaryOnlyGroups;
    }
    else {
      const auto iterator = votes.find(key);

      if (iterator == votes.end()) {
        return false;
      }

      for (const auto &source : iterator->second) {
        addDirectedCrossingSource(gap.movement, source.second);
      }

      classifyDirectedCrossingEvidence(gap.movement);

      if (gap.movement.progressWindows + gap.movement.restrictionWindows
        != group.windows || gap.movement.observedSources != group.sources) {
        return false;
      }

      gap.missingStrongSources = missingFineTrialSources(gap.movement);
      ++next.movingGroups;

      switch (gap.movement.status) {
      case DirectionAssessmentStatus::Insufficient:
        ++next.insufficientGroups;
        break;
      case DirectionAssessmentStatus::Conflicted:
        ++next.conflictedGroups;
        break;
      case DirectionAssessmentStatus::ProgressEvidence:
        ++next.progressEvidenceGroups;
        break;
      case DirectionAssessmentStatus::RestrictionEvidence:
        ++next.restrictionEvidenceGroups;
        break;
      case DirectionAssessmentStatus::Unobserved:
        return false;
      }
    }

    next.details.push_back(std::move(gap));
  }

  next.groups = static_cast<int>(next.details.size());

  if (next.groups != next.refinement.groups
    || next.movingGroups + next.stationaryOnlyGroups != next.groups) {
    return false;
  }

  // Review conflicting moving outcomes first, then candidate restrictions.
  // Stationary repetitions have no directional movement-quality status.
  std::sort(next.details.begin(), next.details.end(),
    [](const FineTrialGap &a, const FineTrialGap &b) {
      const bool aStationary = a.group.travelBin == kFineRefinementStationary;
      const bool bStationary = b.group.travelBin == kFineRefinementStationary;

      if (aStationary != bStationary) {
        return !aStationary;
      }

      const bool aConflict = a.movement.status == DirectionAssessmentStatus::Conflicted;
      const bool bConflict = b.movement.status == DirectionAssessmentStatus::Conflicted;

      if (aConflict != bConflict) {
        return aConflict;
      }

      if (a.movement.restrictionSources != b.movement.restrictionSources) {
        return a.movement.restrictionSources > b.movement.restrictionSources;
      }

      if (a.movement.observedSources != b.movement.observedSources) {
        return a.movement.observedSources < b.movement.observedSources;
      }

      if (a.group.startSubcell < b.group.startSubcell) {
        return true;
      }

      if (b.group.startSubcell < a.group.startSubcell) {
        return false;
      }

      if (a.group.headingBin != b.group.headingBin) {
        return a.group.headingBin < b.group.headingBin;
      }

      return a.group.travelBin < b.group.travelBin;
    });

  result = std::move(next);
  return true;
}

} // namespace aipb::explorer::bridge
