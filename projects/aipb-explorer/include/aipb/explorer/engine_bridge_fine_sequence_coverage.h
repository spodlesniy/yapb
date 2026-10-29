#pragma once

#include <algorithm>
#include <cstddef>
#include <set>
#include <utility>
#include <vector>

#include <aipb/explorer/engine_bridge_fine_crossing_sequences.h>

namespace aipb::explorer::bridge {

inline constexpr int kMaxFineCoverageCandidates = 8;

struct FineCoverageBin {
  int headingBin {};
  int records {};
  int zeroFaceRecords {};
  int singleFaceRecords {};
  int twoFaceChains {};
  int threePlusChains {};
  int ambiguousRecords {};
  int discontinuousRecords {};
  int pairWindows {};
  int tripletWindows {};
  int chainSources {};
  int tripletSources {};
  int longestFaces {};
};

struct FineCoverageCandidate {
  int source {};
  int firstStep {};
  int headingBin {};
  int faces {};
  MovementClassification classification { MovementClassification::InsufficientEvidence };
};

struct FineCoverageAudit {
  SpatialCell queryCoarse {};
  int sources {};
  int records {};
  int zeroFaceRecords {};
  int singleFaceRecords {};
  int twoFaceChains {};
  int threePlusChains {};
  int ambiguousRecords {};
  int discontinuousRecords {};
  int pairWindows {};
  int tripletWindows {};
  int chainSources {};
  int tripletSources {};
  int longestFaces {};
  FineCoverageBin bins[kDirectionBins] {};
  std::vector<FineCoverageCandidate> longest {};
};

inline bool betterFineCoverageCandidate(
  const FineCoverageCandidate &a,
  const FineCoverageCandidate &b
) {
  if (a.faces != b.faces) {
    return a.faces > b.faces;
  }

  if (a.source != b.source) {
    return a.source < b.source;
  }

  if (a.firstStep != b.firstStep) {
    return a.firstStep < b.firstStep;
  }

  return a.headingBin < b.headingBin;
}

// Summarize D241's strict, record-local crossing extraction for every yaw
// sector. A zero-face record is not evidence of a blocked or invalid route.
inline bool auditFineSequenceCoverage(
  const EvidenceTrialSet &sources,
  double x,
  double y,
  double z,
  FineCoverageAudit &result
) {
  FineCoverageAudit next {};
  std::set<int> chainSources {};
  std::set<int> tripletSources {};

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

    if (sequences.crossingRecords != sequences.singleFaceRecords + sequences.chains) {
      return false;
    }

    const int zeroFaceRecords = sequences.records - sequences.crossingRecords
      - sequences.ambiguousRecords - sequences.discontinuousRecords;

    if (zeroFaceRecords < 0) {
      return false;
    }

    FineCoverageBin &bucket = next.bins[bin];
    bucket.headingBin = bin;
    bucket.records = sequences.records;
    bucket.zeroFaceRecords = zeroFaceRecords;
    bucket.singleFaceRecords = sequences.singleFaceRecords;
    bucket.ambiguousRecords = sequences.ambiguousRecords;
    bucket.discontinuousRecords = sequences.discontinuousRecords;
    bucket.pairWindows = sequences.orderedPairs;
    std::set<int> binChainSources {};
    std::set<int> binTripletSources {};

    for (const FineCrossingSequence &chain : sequences.details) {
      const int faces = static_cast<int>(chain.steps.size());

      if (faces < 2) {
        return false;
      }

      if (faces == 2) {
        ++bucket.twoFaceChains;
      }
      else {
        ++bucket.threePlusChains;
        bucket.tripletWindows += faces - 2;
        binTripletSources.insert(chain.source);
        tripletSources.insert(chain.source);
      }

      bucket.longestFaces = std::max(bucket.longestFaces, faces);
      binChainSources.insert(chain.source);
      chainSources.insert(chain.source);

      next.longest.push_back({
        chain.source,
        chain.firstStep,
        chain.headingBin,
        faces,
        chain.classification
      });
      std::sort(next.longest.begin(), next.longest.end(), betterFineCoverageCandidate);

      if (next.longest.size() > static_cast<std::size_t>(kMaxFineCoverageCandidates)) {
        next.longest.resize(kMaxFineCoverageCandidates);
      }
    }

    if (bucket.twoFaceChains + bucket.threePlusChains != sequences.chains) {
      return false;
    }

    bucket.chainSources = static_cast<int>(binChainSources.size());
    bucket.tripletSources = static_cast<int>(binTripletSources.size());

    next.records += bucket.records;
    next.zeroFaceRecords += bucket.zeroFaceRecords;
    next.singleFaceRecords += bucket.singleFaceRecords;
    next.twoFaceChains += bucket.twoFaceChains;
    next.threePlusChains += bucket.threePlusChains;
    next.ambiguousRecords += bucket.ambiguousRecords;
    next.discontinuousRecords += bucket.discontinuousRecords;
    next.pairWindows += bucket.pairWindows;
    next.tripletWindows += bucket.tripletWindows;
    next.longestFaces = std::max(next.longestFaces, bucket.longestFaces);
  }

  next.chainSources = static_cast<int>(chainSources.size());
  next.tripletSources = static_cast<int>(tripletSources.size());
  result = std::move(next);
  return true;
}

} // namespace aipb::explorer::bridge
