#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

#include <aipb/explorer/engine_bridge_source_consensus.h>

namespace aipb::explorer::bridge {

inline constexpr int kLocalizedCellSize = 32;

struct LocalizedSourceEvidence {
  SpatialCell fineCell {};
  SourceConsensus coarse {};
  SourceConsensus fine {};
};

inline int localizedCell(double value) {
  return static_cast<int>(std::floor(value / kLocalizedCellSize));
}

// Inclusive boundaries deliberately preserve contact on a subcell face in both
// neighboring queries. This is an observation intersection, not a collision test.
inline bool segmentIntersectsLocalizedCell(const SpatialRecord &record, const SpatialCell &cell) {
  const int coords[3] { cell.x, cell.y, cell.z };
  double enter = 0.0;
  double leave = 1.0;

  for (int axis = 0; axis < 3; ++axis) {
    const double low = static_cast<double>(coords[axis]) * kLocalizedCellSize;
    const double high = low + kLocalizedCellSize;
    const double start = record.start[axis];
    const double change = record.end[axis] - start;

    if (change == 0.0) {
      if (start < low || start > high) {
        return false;
      }
    }
    else {
      const double a = (low - start) / change;
      const double b = (high - start) / change;
      enter = std::max(enter, std::min(a, b));
      leave = std::min(leave, std::max(a, b));

      if (enter > leave) {
        return false;
      }
    }
  }

  return true;
}

// Compare the existing 128-unit source consensus with evidence from segments
// intersecting the requested 32-unit 3D subcell, retaining source boundaries.
inline bool localizeSourceEvidence(
  const EvidenceTrialSet &sources,
  double x,
  double y,
  double z,
  double heading,
  LocalizedSourceEvidence &result
) {
  if (sources.trials.empty() || sources.trials.size() > kMaxTrialSources
    || !spatialCoordinate(x) || !spatialCoordinate(y) || !spatialCoordinate(z)
    || !std::isfinite(heading)) {
    return false;
  }

  LocalizedSourceEvidence next {};
  next.fineCell = { localizedCell(x), localizedCell(y), localizedCell(z) };

  if (!consolidateSourceEvidence(sources, x, y, z, heading, next.coarse)) {
    return false;
  }

  SourceConsensus &fine = next.fine;
  fine.cell = next.coarse.cell;
  fine.headingBin = next.coarse.headingBin;
  fine.sources = next.coarse.sources;
  SpatialPosting lower {};
  lower.cell = next.coarse.cell;
  lower.record = -1;

  for (const EvidenceTrial &trial : sources.trials) {
    SpatialIndex index {};

    if (!buildSpatialIndex(trial.rows, index)) {
      return false;
    }

    int observations = 0;
    int progress = 0;
    int restricted = 0;
    auto it = std::lower_bound(index.postings.begin(), index.postings.end(), lower);

    for (; it != index.postings.end() && !(lower.cell < it->cell); ++it) {
      if (it->record < 0 || static_cast<std::size_t>(it->record) >= index.records.size()) {
        return false;
      }

      const SpatialRecord &record = index.records[static_cast<std::size_t>(it->record)];
      double recordedHeading {};

      if (!spatialRecordHeading(record, recordedHeading)) {
        return false;
      }

      if (directionBin(recordedHeading) != fine.headingBin
        || !segmentIntersectsLocalizedCell(record, next.fineCell)) {
        continue;
      }

      ++observations;

      switch (record.classification) {
      case MovementClassification::Progress:
        ++progress;
        ++fine.progress;
        break;
      case MovementClassification::BlockedCandidate:
        ++restricted;
        ++fine.blockedCandidates;
        break;
      case MovementClassification::DeflectedCandidate:
        ++restricted;
        ++fine.deflectedCandidates;
        break;
      case MovementClassification::InsufficientEvidence:
        return false;
      }
    }

    fine.windows += observations;

    if (observations == 0) {
      continue;
    }

    ++fine.observedSources;

    if (progress > 0) {
      ++fine.progressSources;
    }

    if (restricted > 0) {
      ++fine.restrictionSources;
    }

    if (progress > 0 && restricted > 0) {
      ++fine.mixedSources;
    }
    else if (observations < kMinimumSourceWindows) {
      ++fine.weakSources;
    }
    else if (progress > 0) {
      ++fine.strongProgressSources;
    }
    else {
      ++fine.strongRestrictionSources;
    }
  }

  fine.status = classifySourceConsensus(fine);
  result = next;
  return true;
}

} // namespace aipb::explorer::bridge
