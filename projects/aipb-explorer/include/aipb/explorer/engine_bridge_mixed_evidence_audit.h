#pragma once

#include <algorithm>
#include <cmath>
#include <utility>

#include <aipb/explorer/engine_bridge_direction_evidence.h>

namespace aipb::explorer::bridge {

struct MixedEvidenceAudit {
  SpatialCell cell {};
  DirectionEvidence headings[kDirectionBins] {};
  int occupiedHeadings {};
  int observations {};
  int progress {};
  int blockedCandidates {};
  int deflectedCandidates {};
  int sameHeadingConflicts {};
  bool otherHeadingOpposition {};
  bool allClassesPresent {};
};

// Recompute from the validated index, never infer collision or passability.
inline bool auditMixedEvidence(
  const SpatialIndex &index,
  double x,
  double y,
  double z,
  MixedEvidenceAudit &result
) {
  if (!spatialCoordinate(x) || !spatialCoordinate(y) || !spatialCoordinate(z)) {
    return false;
  }

  DirectionStatistics statistics {};

  if (!aggregateDirectionalEvidence(index, statistics)) {
    return false;
  }

  MixedEvidenceAudit next {};
  next.cell = { spatialCell(x), spatialCell(y), spatialCell(z) };
  DirectionEvidence key {};
  key.cell = next.cell;

  for (int bin = 0; bin < kDirectionBins; ++bin) {
    next.headings[bin].cell = next.cell;
    next.headings[bin].bin = bin;
  }

  const auto it = std::lower_bound(statistics.entries.begin(), statistics.entries.end(),
    key, [](const DirectionEvidence &entry, const DirectionEvidence &other) {
      return entry.cell < other.cell;
    });

  unsigned progressMask = 0;
  unsigned restrictionMask = 0;

  for (auto cursor = it; cursor != statistics.entries.end()
    && !(next.cell < cursor->cell); ++cursor) {
    if (cursor->bin < 0 || cursor->bin >= kDirectionBins
      || cursor->observations != cursor->progress
        + cursor->blockedCandidates + cursor->deflectedCandidates) {
      return false;
    }

    next.headings[cursor->bin] = *cursor;
    ++next.occupiedHeadings;
    next.observations += cursor->observations;
    next.progress += cursor->progress;
    next.blockedCandidates += cursor->blockedCandidates;
    next.deflectedCandidates += cursor->deflectedCandidates;

    if (directionConflict(*cursor)) {
      ++next.sameHeadingConflicts;
    }

    if (cursor->progress > 0) {
      progressMask |= (1u << cursor->bin);
    }

    if (cursor->blockedCandidates > 0 || cursor->deflectedCandidates > 0) {
      restrictionMask |= (1u << cursor->bin);
    }
  }

  for (int bin = 0; bin < kDirectionBins; ++bin) {
    if ((progressMask & (1u << bin)) != 0
      && (restrictionMask & ~(1u << bin)) != 0) {
      next.otherHeadingOpposition = true;
    }
  }

  next.allClassesPresent = next.progress > 0 && next.blockedCandidates > 0
    && next.deflectedCandidates > 0;
  result = next;
  return true;
}

} // namespace aipb::explorer::bridge
