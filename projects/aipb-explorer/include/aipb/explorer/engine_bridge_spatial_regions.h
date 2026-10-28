#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include <aipb/explorer/engine_bridge_spatial_localization.h>

namespace aipb::explorer::bridge {

inline constexpr int kRegionCellsPerAxis = kSpatialCellSize / kLocalizedCellSize;
inline constexpr int kRegionCellsPerCoarse = kRegionCellsPerAxis
  * kRegionCellsPerAxis * kRegionCellsPerAxis;
static_assert(kRegionCellsPerAxis == 4, "D231 requires four subcells per coarse axis");

enum class RegionEvidenceKind {
  Unobserved,
  ProgressOnly,
  RestrictionOnly,
  Mixed
};

inline const char *regionEvidenceName(RegionEvidenceKind kind) {
  switch (kind) {
  case RegionEvidenceKind::Unobserved:
    return "Unobserved";
  case RegionEvidenceKind::ProgressOnly:
    return "ProgressOnly";
  case RegionEvidenceKind::RestrictionOnly:
    return "RestrictionOnly";
  case RegionEvidenceKind::Mixed:
    return "Mixed";
  }

  return "Invalid";
}

inline RegionEvidenceKind classifyRegionCell(const SourceConsensus &evidence) {
  const bool progress = evidence.progress > 0;
  const bool restricted = evidence.blockedCandidates > 0 || evidence.deflectedCandidates > 0;

  if (progress && restricted) {
    return RegionEvidenceKind::Mixed;
  }

  if (progress) {
    return RegionEvidenceKind::ProgressOnly;
  }

  return restricted ? RegionEvidenceKind::RestrictionOnly : RegionEvidenceKind::Unobserved;
}

struct RegionEvidence {
  SpatialCell coarseCell {};
  SpatialCell seedCell {};
  SpatialCell minCell {};
  SpatialCell maxCell {};
  int headingBin {};
  RegionEvidenceKind seedKind { RegionEvidenceKind::Unobserved };
  SourceConsensus seed {};
  SourceConsensus coarse {};
  SourceConsensus region {};
  int memberCells {};
  int boundaryMixed {};
  int boundaryUnobserved {};
  int boundaryOpposite {};
  int boundaryCoarseEdge {};
};

inline int regionCellOffset(int x, int y, int z) {
  return (x * kRegionCellsPerAxis + y) * kRegionCellsPerAxis + z;
}

inline void accumulateRegionSource(SourceConsensus &out,
  int progress, int blocked, int deflected) {
  const int count = progress + blocked + deflected;
  out.windows += count;
  out.progress += progress;
  out.blockedCandidates += blocked;
  out.deflectedCandidates += deflected;

  if (count == 0) {
    return;
  }

  ++out.observedSources;
  const bool hasProgress = progress > 0;
  const bool hasRestriction = blocked > 0 || deflected > 0;

  if (hasProgress) {
    ++out.progressSources;
  }

  if (hasRestriction) {
    ++out.restrictionSources;
  }

  if (hasProgress && hasRestriction) {
    ++out.mixedSources;
  }
  else if (count < kMinimumSourceWindows) {
    ++out.weakSources;
  }
  else if (hasProgress) {
    ++out.strongProgressSources;
  }
  else {
    ++out.strongRestrictionSources;
  }
}

inline bool groupNeighboringEvidence(
  const EvidenceTrialSet &sources,
  double x,
  double y,
  double z,
  double heading,
  RegionEvidence &result
) {
  if (sources.trials.empty() || sources.trials.size() > kMaxTrialSources
    || !spatialCoordinate(x) || !spatialCoordinate(y) || !spatialCoordinate(z)
    || !std::isfinite(heading)) {
    return false;
  }

  RegionEvidence next {};
  next.coarseCell = { spatialCell(x), spatialCell(y), spatialCell(z) };
  next.seedCell = { localizedCell(x), localizedCell(y), localizedCell(z) };
  next.headingBin = directionBin(heading);

  if (!consolidateSourceEvidence(sources, x, y, z, heading, next.coarse)) {
    return false;
  }

  SourceConsensus cells[kRegionCellsPerCoarse] {};
  SpatialCell fineCells[kRegionCellsPerCoarse] {};
  RegionEvidenceKind kinds[kRegionCellsPerCoarse] {};
  std::vector<SpatialIndex> indices {};
  indices.reserve(sources.trials.size());

  for (int ix = 0; ix < kRegionCellsPerAxis; ++ix) {
    for (int iy = 0; iy < kRegionCellsPerAxis; ++iy) {
      for (int iz = 0; iz < kRegionCellsPerAxis; ++iz) {
        const int i = regionCellOffset(ix, iy, iz);
        fineCells[i] = { next.coarseCell.x * kRegionCellsPerAxis + ix,
          next.coarseCell.y * kRegionCellsPerAxis + iy,
          next.coarseCell.z * kRegionCellsPerAxis + iz };
        cells[i].cell = next.coarseCell;
        cells[i].headingBin = next.headingBin;
        cells[i].sources = static_cast<int>(sources.trials.size());
      }
    }
  }

  SpatialPosting lower {};
  lower.cell = next.coarseCell;
  lower.record = -1;

  for (const EvidenceTrial &trial : sources.trials) {
    SpatialIndex index {};

    if (!buildSpatialIndex(trial.rows, index)) {
      return false;
    }

    int progress[kRegionCellsPerCoarse] {};
    int blocked[kRegionCellsPerCoarse] {};
    int deflected[kRegionCellsPerCoarse] {};
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

      if (directionBin(recordedHeading) != next.headingBin) {
        continue;
      }

      for (int i = 0; i < kRegionCellsPerCoarse; ++i) {
        if (!segmentIntersectsLocalizedCell(record, fineCells[i])) {
          continue;
        }

        switch (record.classification) {
        case MovementClassification::Progress:
          ++progress[i];
          break;
        case MovementClassification::BlockedCandidate:
          ++blocked[i];
          break;
        case MovementClassification::DeflectedCandidate:
          ++deflected[i];
          break;
        case MovementClassification::InsufficientEvidence:
          return false;
        }
      }
    }

    for (int i = 0; i < kRegionCellsPerCoarse; ++i) {
      accumulateRegionSource(cells[i], progress[i], blocked[i], deflected[i]);
    }

    indices.push_back(std::move(index));
  }

  for (int i = 0; i < kRegionCellsPerCoarse; ++i) {
    cells[i].status = classifySourceConsensus(cells[i]);
    kinds[i] = classifyRegionCell(cells[i]);
  }

  const int sx = next.seedCell.x - next.coarseCell.x * kRegionCellsPerAxis;
  const int sy = next.seedCell.y - next.coarseCell.y * kRegionCellsPerAxis;
  const int sz = next.seedCell.z - next.coarseCell.z * kRegionCellsPerAxis;

  if (sx < 0 || sx >= kRegionCellsPerAxis || sy < 0 || sy >= kRegionCellsPerAxis
    || sz < 0 || sz >= kRegionCellsPerAxis) {
    return false;
  }

  const int seed = regionCellOffset(sx, sy, sz);
  next.seed = cells[seed];
  next.seedKind = kinds[seed];
  next.region.cell = next.coarseCell;
  next.region.headingBin = next.headingBin;
  next.region.sources = static_cast<int>(sources.trials.size());

  if (next.seedKind == RegionEvidenceKind::Unobserved
    || next.seedKind == RegionEvidenceKind::Mixed) {
    result = next;
    return true;
  }

  bool member[kRegionCellsPerCoarse] {};
  int queue[kRegionCellsPerCoarse] {};
  int count = 1;
  member[seed] = true;
  queue[0] = seed;
  next.minCell = next.seedCell;
  next.maxCell = next.seedCell;

  // Only six shared faces connect cells. Diagonal contacts never join regions.
  for (int front = 0; front < count; ++front) {
    const int i = queue[front];
    const int cx = i / (kRegionCellsPerAxis * kRegionCellsPerAxis);
    const int cy = i / kRegionCellsPerAxis % kRegionCellsPerAxis;
    const int cz = i % kRegionCellsPerAxis;
    const int delta[6][3] {
      { -1, 0, 0 }, { 1, 0, 0 }, { 0, -1, 0 },
      { 0, 1, 0 }, { 0, 0, -1 }, { 0, 0, 1 }
    };

    for (const auto &axis : delta) {
      const int nx = cx + axis[0];
      const int ny = cy + axis[1];
      const int nz = cz + axis[2];

      if (nx < 0 || nx >= kRegionCellsPerAxis || ny < 0 || ny >= kRegionCellsPerAxis
        || nz < 0 || nz >= kRegionCellsPerAxis) {
        ++next.boundaryCoarseEdge;
        continue;
      }

      const int other = regionCellOffset(nx, ny, nz);

      if (kinds[other] == next.seedKind) {
        if (!member[other]) {
          member[other] = true;
          queue[count++] = other;
          const SpatialCell &cell = fineCells[other];
          next.minCell.x = std::min(next.minCell.x, cell.x);
          next.minCell.y = std::min(next.minCell.y, cell.y);
          next.minCell.z = std::min(next.minCell.z, cell.z);
          next.maxCell.x = std::max(next.maxCell.x, cell.x);
          next.maxCell.y = std::max(next.maxCell.y, cell.y);
          next.maxCell.z = std::max(next.maxCell.z, cell.z);
        }
      }
      else if (kinds[other] == RegionEvidenceKind::Mixed) {
        ++next.boundaryMixed;
      }
      else if (kinds[other] == RegionEvidenceKind::Unobserved) {
        ++next.boundaryUnobserved;
      }
      else {
        ++next.boundaryOpposite;
      }
    }
  }

  next.memberCells = count;

  // A source window may intersect several member cells. Count it only once
  // when consolidating the region, preserving source-quality thresholds.
  for (const SpatialIndex &index : indices) {
    int progress = 0;
    int blocked = 0;
    int deflected = 0;
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

      if (directionBin(recordedHeading) != next.headingBin) {
        continue;
      }

      bool touchesRegion = false;

      for (int i = 0; i < kRegionCellsPerCoarse; ++i) {
        if (member[i] && segmentIntersectsLocalizedCell(record, fineCells[i])) {
          touchesRegion = true;
          break;
        }
      }

      if (!touchesRegion) {
        continue;
      }

      switch (record.classification) {
      case MovementClassification::Progress:
        ++progress;
        break;
      case MovementClassification::BlockedCandidate:
        ++blocked;
        break;
      case MovementClassification::DeflectedCandidate:
        ++deflected;
        break;
      case MovementClassification::InsufficientEvidence:
        return false;
      }
    }

    accumulateRegionSource(next.region, progress, blocked, deflected);
  }

  next.region.status = classifySourceConsensus(next.region);
  result = next;
  return true;
}

} // namespace aipb::explorer::bridge
