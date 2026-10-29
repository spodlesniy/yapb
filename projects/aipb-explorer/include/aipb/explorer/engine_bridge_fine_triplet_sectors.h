#pragma once

#include <cstddef>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include <aipb/explorer/engine_bridge_fine_pair_sectors.h>

namespace aipb::explorer::bridge {

inline constexpr int kMaxFineTripletPrinted = 24;

struct FineOrderedTripletKey {
  FineDirectedFaceKey first {};
  FineDirectedFaceKey second {};
  FineDirectedFaceKey third {};
};

inline bool operator<(
  const FineOrderedTripletKey &a,
  const FineOrderedTripletKey &b
) {
  if (a.first < b.first) {
    return true;
  }

  if (b.first < a.first) {
    return false;
  }

  if (a.second < b.second) {
    return true;
  }

  if (b.second < a.second) {
    return false;
  }

  return a.third < b.third;
}

struct FineTripletEvidence {
  FineOrderedTripletKey triplet {};
  CrossingSectorOutcome outcome {};
  int firstSource { -1 };
  int firstStep {};
  int firstHeadingBin {};
};

struct FineTripletAudit {
  SpatialCell queryCoarse {};
  int sources {};
  int chains {};
  int tripletWindows {};
  int distinctTriplets {};
  int observedSources {};
  int multipleSectorTriplets {};
  int crossSectorConflictTriplets {};
  int progressEvidenceTriplets {};
  int restrictionEvidenceTriplets {};
  int conflictedTriplets {};
  int insufficientTriplets {};
  std::vector<FineTripletEvidence> details {};
};

// Only three consecutive strict crossings from the same recorded segment
// form a triplet. Changing yaw or repeating windows does not add sources.
inline bool auditFineTripletSectors(
  const EvidenceTrialSet &sources,
  double x,
  double y,
  double z,
  FineTripletAudit &result
) {
  FineTripletAudit next {};
  using SourceVotes = std::map<int, DirectedCrossingSourceWindows>;
  std::map<FineOrderedTripletKey, SourceVotes> votes {};
  std::map<FineOrderedTripletKey, FineTripletEvidence> evidence {};
  std::set<int> observedSources {};

  for (int bin = 0; bin < kDirectionBins; ++bin) {
    const double heading = static_cast<double>(bin) * 360.0
      / static_cast<double>(kDirectionBins);
    FineSequenceAudit sequences {};

    if (!auditFineCrossingSequences(sources, x, y, z, heading, sequences)) {
      return false;
    }

    if (bin == 0) {
      next.queryCoarse = sequences.queryCoarse;
      next.sources = sequences.sources;
    }

    next.chains += sequences.chains;
    const unsigned int bit = 1u << bin;

    for (const FineCrossingSequence &chain : sequences.details) {
      for (std::size_t i = 2; i < chain.steps.size(); ++i) {
        const FineOrderedTripletKey key {
          fineDirectedKey(chain.steps[i - 2]),
          fineDirectedKey(chain.steps[i - 1]),
          fineDirectedKey(chain.steps[i])
        };

        FineTripletEvidence &detail = evidence[key];
        detail.triplet = key;

        if (detail.firstSource < 0) {
          detail.firstSource = chain.source;
          detail.firstStep = chain.firstStep;
          detail.firstHeadingBin = chain.headingBin;
        }

        DirectedCrossingSourceWindows &windows = votes[key][chain.source];

        if (chain.classification == MovementClassification::Progress) {
          ++windows.progress;
          detail.outcome.progressBins |= bit;
        }
        else {
          ++windows.restriction;
          detail.outcome.restrictionBins |= bit;
        }

        ++next.tripletWindows;
        observedSources.insert(chain.source);
      }
    }
  }

  next.distinctTriplets = static_cast<int>(evidence.size());
  next.observedSources = static_cast<int>(observedSources.size());

  for (auto &entry : evidence) {
    FineTripletEvidence &detail = entry.second;

    for (const auto &source : votes.at(entry.first)) {
      addDirectedCrossingSource(detail.outcome.evidence, source.second);
    }

    classifyDirectedCrossingEvidence(detail.outcome.evidence);

    if (crossingMultipleSectors(
      detail.outcome.progressBins | detail.outcome.restrictionBins)) {
      ++next.multipleSectorTriplets;
    }

    if (crossingSectorOpposition(
      detail.outcome.progressBins, detail.outcome.restrictionBins)) {
      ++next.crossSectorConflictTriplets;
    }

    switch (detail.outcome.evidence.status) {
    case DirectionAssessmentStatus::ProgressEvidence:
      ++next.progressEvidenceTriplets;
      break;
    case DirectionAssessmentStatus::RestrictionEvidence:
      ++next.restrictionEvidenceTriplets;
      break;
    case DirectionAssessmentStatus::Conflicted:
      ++next.conflictedTriplets;
      break;
    case DirectionAssessmentStatus::Insufficient:
      ++next.insufficientTriplets;
      break;
    case DirectionAssessmentStatus::Unobserved:
      return false;
    }

    next.details.push_back(detail);
  }

  result = std::move(next);
  return true;
}

} // namespace aipb::explorer::bridge
