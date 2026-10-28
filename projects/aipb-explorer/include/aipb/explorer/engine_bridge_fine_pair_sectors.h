#pragma once

#include <cstddef>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include <aipb/explorer/engine_bridge_fine_sequence_pairs.h>

namespace aipb::explorer::bridge {

struct FinePairSectorEvidence {
  FineOrderedPairKey pair {};
  CrossingSectorOutcome outcome {};
  int firstSource { -1 };
  int firstStep {};
  int firstHeadingBin {};
};

struct FinePairSectorAudit {
  SpatialCell queryCoarse {};
  int sources {};
  int chains {};
  int orderedPairWindows {};
  int distinctPairs {};
  int observedSources {};
  int multipleSectorPairs {};
  int crossSectorConflictPairs {};
  int progressEvidencePairs {};
  int restrictionEvidencePairs {};
  int conflictedPairs {};
  int insufficientPairs {};
  std::vector<FinePairSectorEvidence> details {};
};

// Inspect all eight requested-yaw sectors without joining different records.
// A source keeps one identity for a given ordered, signed pair across bins.
inline bool auditFinePairSectors(
  const EvidenceTrialSet &sources,
  double x,
  double y,
  double z,
  FinePairSectorAudit &result
) {
  FinePairSectorAudit next {};
  using SourceVotes = std::map<int, DirectedCrossingSourceWindows>;
  std::map<FineOrderedPairKey, SourceVotes> votes {};
  std::map<FineOrderedPairKey, FinePairSectorEvidence> details {};
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
    next.orderedPairWindows += sequences.orderedPairs;
    const unsigned int binBit = 1u << bin;

    for (const FineCrossingSequence &chain : sequences.details) {
      for (std::size_t i = 1; i < chain.steps.size(); ++i) {
        const FineOrderedPairKey key {
          fineDirectedKey(chain.steps[i - 1]),
          fineDirectedKey(chain.steps[i])
        };
        FinePairSectorEvidence &detail = details[key];
        detail.pair = key;

        if (detail.firstSource < 0) {
          detail.firstSource = chain.source;
          detail.firstStep = chain.firstStep;
          detail.firstHeadingBin = chain.headingBin;
        }

        DirectedCrossingSourceWindows &windows = votes[key][chain.source];

        if (chain.classification == MovementClassification::Progress) {
          ++windows.progress;
          detail.outcome.progressBins |= binBit;
        }
        else {
          ++windows.restriction;
          detail.outcome.restrictionBins |= binBit;
        }

        observedSources.insert(chain.source);
      }
    }
  }

  next.distinctPairs = static_cast<int>(details.size());
  next.observedSources = static_cast<int>(observedSources.size());

  for (auto &entry : details) {
    FinePairSectorEvidence &detail = entry.second;

    for (const auto &source : votes.at(entry.first)) {
      addDirectedCrossingSource(detail.outcome.evidence, source.second);
    }

    classifyDirectedCrossingEvidence(detail.outcome.evidence);

    if (crossingMultipleSectors(
      detail.outcome.progressBins | detail.outcome.restrictionBins)) {
      ++next.multipleSectorPairs;
    }

    if (crossingSectorOpposition(
      detail.outcome.progressBins, detail.outcome.restrictionBins)) {
      ++next.crossSectorConflictPairs;
    }

    switch (detail.outcome.evidence.status) {
    case DirectionAssessmentStatus::ProgressEvidence:
      ++next.progressEvidencePairs;
      break;
    case DirectionAssessmentStatus::RestrictionEvidence:
      ++next.restrictionEvidencePairs;
      break;
    case DirectionAssessmentStatus::Conflicted:
      ++next.conflictedPairs;
      break;
    case DirectionAssessmentStatus::Insufficient:
      ++next.insufficientPairs;
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
