#pragma once

#include <cstddef>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include <aipb/explorer/engine_bridge_fine_crossing_sequences.h>

namespace aipb::explorer::bridge {

inline constexpr int kMaxFinePairPrinted = 24;

struct FineDirectedFaceKey {
  SpatialCell lowerCell {};
  int axis {};
  bool positive {};
};

inline bool operator<(const FineDirectedFaceKey &a, const FineDirectedFaceKey &b) {
  if (a.lowerCell < b.lowerCell) {
    return true;
  }
  if (b.lowerCell < a.lowerCell) {
    return false;
  }
  if (a.axis != b.axis) {
    return a.axis < b.axis;
  }
  return a.positive < b.positive;
}

struct FineOrderedPairKey {
  FineDirectedFaceKey first {};
  FineDirectedFaceKey second {};
};

inline bool operator<(const FineOrderedPairKey &a, const FineOrderedPairKey &b) {
  if (a.first < b.first) {
    return true;
  }
  if (b.first < a.first) {
    return false;
  }
  return a.second < b.second;
}

inline FineDirectedFaceKey fineDirectedKey(const FineSequenceStep &step) {
  return { step.lowerCell, step.axis, step.positive };
}

struct FinePairEvidence {
  FineOrderedPairKey pair {};
  DirectedCrossingEvidence consensus {};
  int firstSource { -1 };
  int firstStep {};
  int headingBin {};
};

struct FinePairAudit {
  FineSequenceAudit sequences {};
  int distinctPairs {};
  int observedSources {};
  int progressEvidencePairs {};
  int restrictionEvidencePairs {};
  int conflictedPairs {};
  int insufficientPairs {};
  std::vector<FinePairEvidence> details {};
};

// A pair is credited once per source, even when that trace repeats the
// identical ordered pair in many windows. No cross-record joining is allowed.
inline bool auditFineSequencePairs(
  const EvidenceTrialSet &sources,
  double x,
  double y,
  double z,
  double heading,
  FinePairAudit &result
) {
  FinePairAudit next {};
  if (!auditFineCrossingSequences(sources, x, y, z, heading, next.sequences)) {
    return false;
  }

  using SourceVotes = std::map<int, DirectedCrossingSourceWindows>;
  std::map<FineOrderedPairKey, SourceVotes> votes {};
  std::map<FineOrderedPairKey, FinePairEvidence> details {};
  std::set<int> observedSources {};

  for (const FineCrossingSequence &chain : next.sequences.details) {
    for (std::size_t i = 1; i < chain.steps.size(); ++i) {
      const FineOrderedPairKey key {
        fineDirectedKey(chain.steps[i - 1]),
        fineDirectedKey(chain.steps[i])
      };
      FinePairEvidence &detail = details[key];
      detail.pair = key;
      if (detail.firstSource < 0) {
        detail.firstSource = chain.source;
        detail.firstStep = chain.firstStep;
        detail.headingBin = chain.headingBin;
      }

      DirectedCrossingSourceWindows &windows = votes[key][chain.source];
      if (chain.classification == MovementClassification::Progress) {
        ++windows.progress;
      }
      else {
        ++windows.restriction;
      }
      observedSources.insert(chain.source);
    }
  }

  next.distinctPairs = static_cast<int>(details.size());
  next.observedSources = static_cast<int>(observedSources.size());
  for (auto &entry : details) {
    FinePairEvidence &detail = entry.second;
    for (const auto &source : votes.at(entry.first)) {
      addDirectedCrossingSource(detail.consensus, source.second);
    }
    classifyDirectedCrossingEvidence(detail.consensus);
    switch (detail.consensus.status) {
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
