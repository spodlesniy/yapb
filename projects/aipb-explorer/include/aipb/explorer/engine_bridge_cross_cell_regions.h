#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include <aipb/explorer/engine_bridge_spatial_regions.h>

namespace aipb::explorer::bridge {

inline constexpr int kSpanCoarseRadius = 1;
inline constexpr int kSpanCellsPerAxis = (2 * kSpanCoarseRadius + 1) * kRegionCellsPerAxis;
inline constexpr int kMaxSpanCandidateChecks = kMaxSpatialPostings;

struct SpanCellCounts {
  int progress[kMaxTrialSources] {};
  int blocked[kMaxTrialSources] {};
  int deflected[kMaxTrialSources] {};
};

struct SpanWindow {
  MovementClassification classification { MovementClassification::InsufficientEvidence };
  std::vector<SpatialCell> cells {};
};

struct SpanSource {
  std::vector<SpanWindow> windows {};
};

struct CrossCellRegion {
  SpatialCell seedCell {};
  SpatialCell seedCoarse {};
  SpatialCell minCell {};
  SpatialCell maxCell {};
  int headingBin {};
  RegionEvidenceKind seedKind { RegionEvidenceKind::Unobserved };
  SourceConsensus coarse {};
  SourceConsensus seed {};
  SourceConsensus region {};
  int memberCells {};
  int coarseCells {};
  int boundaryMixed {};
  int boundaryUnobserved {};
  int boundaryOpposite {};
  int boundaryWindowEdge {};
};

inline SourceConsensus spanSourceConsensus(
  const SpanCellCounts &counts, int sources, const SpatialCell &cell, int bin
) {
  SourceConsensus result {};
  result.cell = cell;
  result.headingBin = bin;
  result.sources = sources;

  for (int source = 0; source < sources; ++source) {
    accumulateRegionSource(result, counts.progress[source],
      counts.blocked[source], counts.deflected[source]);
  }

  result.status = classifySourceConsensus(result);
  return result;
}

inline bool spanInWindow(const SpatialCell &cell, const int *lower, const int *upper) {
  return cell.x >= lower[0] && cell.x <= upper[0]
    && cell.y >= lower[1] && cell.y <= upper[1]
    && cell.z >= lower[2] && cell.z <= upper[2];
}

// A bounded 3x3x3 coarse-cell window prevents unintended map-wide grouping.
// Cells join by six shared faces, never by approximate distance or diagonals.
inline bool spanSourceEvidence(
  const EvidenceTrialSet &sources, double x, double y, double z, double heading,
  CrossCellRegion &result, std::set<SpatialCell> *regionMembers = nullptr
) {
  if (sources.trials.empty() || sources.trials.size() > kMaxTrialSources
    || !spatialCoordinate(x) || !spatialCoordinate(y) || !spatialCoordinate(z)
    || !std::isfinite(heading)) {
    return false;
  }

  CrossCellRegion next {};
  next.seedCell = { localizedCell(x), localizedCell(y), localizedCell(z) };
  next.seedCoarse = { spatialCell(x), spatialCell(y), spatialCell(z) };
  next.headingBin = directionBin(heading);

  if (!consolidateSourceEvidence(sources, x, y, z, heading, next.coarse)) {
    return false;
  }

  const int lower[3] {
    (next.seedCoarse.x - kSpanCoarseRadius) * kRegionCellsPerAxis,
    (next.seedCoarse.y - kSpanCoarseRadius) * kRegionCellsPerAxis,
    (next.seedCoarse.z - kSpanCoarseRadius) * kRegionCellsPerAxis
  };
  const int upper[3] {
    lower[0] + kSpanCellsPerAxis - 1,
    lower[1] + kSpanCellsPerAxis - 1,
    lower[2] + kSpanCellsPerAxis - 1
  };

  std::map<SpatialCell, SpanCellCounts> cells {};
  std::vector<SpanSource> records {};
  records.reserve(sources.trials.size());
  int candidateChecks = 0;

  for (std::size_t source = 0; source < sources.trials.size(); ++source) {
    SpatialIndex index {};

    if (!buildSpatialIndex(sources.trials[source].rows, index)) {
      return false;
    }

    SpanSource trace {};

    for (const SpatialRecord &record : index.records) {
      double recordHeading {};

      if (!spatialRecordHeading(record, recordHeading)) {
        return false;
      }

      if (directionBin(recordHeading) != next.headingBin) {
        continue;
      }

      SpanWindow window {};
      window.classification = record.classification;
      int low[3] {};
      int high[3] {};
      bool intersectsWindow = true;

      for (int axis = 0; axis < 3; ++axis) {
        const double minValue = std::min(record.start[axis], record.end[axis]);
        const double maxValue = std::max(record.start[axis], record.end[axis]);
        // Include possible contact on both sides of a shared cell face.
        low[axis] = std::max(lower[axis], localizedCell(minValue) - 1);
        high[axis] = std::min(upper[axis], localizedCell(maxValue) + 1);

        if (low[axis] > high[axis]) {
          intersectsWindow = false;
        }
      }

      if (!intersectsWindow) {
        continue;
      }

      for (int cx = low[0]; cx <= high[0]; ++cx) {
        for (int cy = low[1]; cy <= high[1]; ++cy) {
          for (int cz = low[2]; cz <= high[2]; ++cz) {
            if (++candidateChecks > kMaxSpanCandidateChecks) {
              return false;
            }

            const SpatialCell cell { cx, cy, cz };

            if (!segmentIntersectsLocalizedCell(record, cell)) {
              continue;
            }

            SpanCellCounts &counts = cells[cell];

            switch (record.classification) {
            case MovementClassification::Progress:
              ++counts.progress[source];
              break;
            case MovementClassification::BlockedCandidate:
              ++counts.blocked[source];
              break;
            case MovementClassification::DeflectedCandidate:
              ++counts.deflected[source];
              break;
            case MovementClassification::InsufficientEvidence:
              return false;
            }

            window.cells.push_back(cell);
          }
        }
      }

      if (!window.cells.empty()) {
        trace.windows.push_back(std::move(window));
      }
    }

    records.push_back(std::move(trace));
  }

  const auto seedIt = cells.find(next.seedCell);

  if (seedIt != cells.end()) {
    next.seed = spanSourceConsensus(seedIt->second,
      static_cast<int>(sources.trials.size()), next.seedCoarse, next.headingBin);
  }
  else {
    next.seed.cell = next.seedCoarse;
    next.seed.headingBin = next.headingBin;
    next.seed.sources = static_cast<int>(sources.trials.size());
  }

  next.seedKind = classifyRegionCell(next.seed);
  next.region.cell = next.seedCoarse;
  next.region.headingBin = next.headingBin;
  next.region.sources = static_cast<int>(sources.trials.size());

  if (next.seedKind == RegionEvidenceKind::Unobserved
    || next.seedKind == RegionEvidenceKind::Mixed) {
    if (regionMembers) {
      regionMembers->clear();
    }

    result = std::move(next);
    return true;
  }

  std::set<SpatialCell> members { next.seedCell };
  std::vector<SpatialCell> queue { next.seedCell };
  std::set<SpatialCell> occupiedCoarse { next.seedCoarse };
  next.minCell = next.seedCell;
  next.maxCell = next.seedCell;

  const int delta[6][3] {
    { -1, 0, 0 }, { 1, 0, 0 }, { 0, -1, 0 },
    { 0, 1, 0 }, { 0, 0, -1 }, { 0, 0, 1 }
  };

  for (std::size_t front = 0; front < queue.size(); ++front) {
    const SpatialCell current = queue[front];

    for (const auto &axis : delta) {
      const SpatialCell other {
        current.x + axis[0], current.y + axis[1], current.z + axis[2]
      };

      if (!spanInWindow(other, lower, upper)) {
        ++next.boundaryWindowEdge;
        continue;
      }

      const auto neighbor = cells.find(other);
      const RegionEvidenceKind kind = neighbor == cells.end()
        ? RegionEvidenceKind::Unobserved
        : classifyRegionCell(spanSourceConsensus(neighbor->second,
          static_cast<int>(sources.trials.size()), next.seedCoarse, next.headingBin));

      if (kind == next.seedKind) {
        if (members.insert(other).second) {
          queue.push_back(other);
          next.minCell.x = std::min(next.minCell.x, other.x);
          next.minCell.y = std::min(next.minCell.y, other.y);
          next.minCell.z = std::min(next.minCell.z, other.z);
          next.maxCell.x = std::max(next.maxCell.x, other.x);
          next.maxCell.y = std::max(next.maxCell.y, other.y);
          next.maxCell.z = std::max(next.maxCell.z, other.z);
          occupiedCoarse.insert({ other.x >= 0 ? other.x / kRegionCellsPerAxis
            : (other.x - kRegionCellsPerAxis + 1) / kRegionCellsPerAxis,
            other.y >= 0 ? other.y / kRegionCellsPerAxis
              : (other.y - kRegionCellsPerAxis + 1) / kRegionCellsPerAxis,
            other.z >= 0 ? other.z / kRegionCellsPerAxis
              : (other.z - kRegionCellsPerAxis + 1) / kRegionCellsPerAxis });
        }
      }
      else if (kind == RegionEvidenceKind::Mixed) {
        ++next.boundaryMixed;
      }
      else if (kind == RegionEvidenceKind::RestrictionOnly
        || kind == RegionEvidenceKind::ProgressOnly) {
        ++next.boundaryOpposite;
      }
      else {
        ++next.boundaryUnobserved;
      }
    }
  }

  next.memberCells = static_cast<int>(members.size());
  next.coarseCells = static_cast<int>(occupiedCoarse.size());

  // Count a source record once if it intersects any member fine cell.
  for (const SpanSource &trace : records) {
    int progress = 0;
    int blocked = 0;
    int deflected = 0;

    for (const SpanWindow &window : trace.windows) {
      bool included = false;

      for (const SpatialCell &cell : window.cells) {
        if (members.find(cell) != members.end()) {
          included = true;
          break;
        }
      }

      if (!included) {
        continue;
      }

      switch (window.classification) {
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

  if (regionMembers) {
    *regionMembers = members;
  }

  result = std::move(next);
  return true;
}

} // namespace aipb::explorer::bridge
