#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include <aipb/explorer/engine_bridge_crossing_sectors.h>

namespace aipb::explorer::bridge {

inline constexpr int kMaxFineCrossingFaces =
  3 * (kSpanCellsPerAxis - 1) * kSpanCellsPerAxis * kSpanCellsPerAxis;
inline constexpr int kMaxFineCrossingPrintedFaces = 24;

struct FineCrossingFace {
  SpatialCell lowerCell {};
  int axis {};
  double position[3] {};
  bool coarseBoundary {};
  int positiveProgress {};
  int negativeProgress {};
  int positiveBlocked {};
  int negativeBlocked {};
  int positiveDeflected {};
  int negativeDeflected {};
  std::set<int> progressSources {};
  std::set<int> restrictionSources {};
  std::set<int> positiveSources {};
  std::set<int> negativeSources {};
};

struct FineCrossingDiscovery {
  SpatialCell queryCoarse {};
  int headingBin {};
  int sources {};
  int faces {};
  int coarseFaces {};
  int interiorFaces {};
  int progressFaces {};
  int restrictionFaces {};
  int progressWindows {};
  int blockedWindows {};
  int deflectedWindows {};
  int progressSources {};
  int restrictionSources {};
  std::vector<FineCrossingFace> details {};
};

// Both endpoints must lie strictly across the 32-unit plane, and the
// intersection must be inside one face rather than on its edge or corner.
inline bool crossingAtFineFace(
  const SpatialRecord &record,
  int axis,
  int planeFine,
  SpatialCell &lowerCell,
  double *position
) {
  const double plane = static_cast<double>(planeFine) * kLocalizedCellSize;
  const double start = record.start[axis];
  const double end = record.end[axis];

  if (!((start < plane && end > plane) || (start > plane && end < plane))) {
    return false;
  }

  const double t = (plane - start) / (end - start);
  int coordinates[3] {};
  double crossing[3] {};

  for (int other = 0; other < 3; ++other) {
    if (other == axis) {
      coordinates[other] = planeFine - 1;
      crossing[other] = plane;
      continue;
    }

    crossing[other] = record.start[other]
      + t * (record.end[other] - record.start[other]);

    if (!spatialCoordinate(crossing[other])) {
      return false;
    }

    const int fine = localizedCell(crossing[other]);
    const double within = crossing[other]
      - static_cast<double>(fine) * kLocalizedCellSize;

    if (within <= 1e-9 || within >= kLocalizedCellSize - 1e-9) {
      return false;
    }

    coordinates[other] = fine;
  }

  lowerCell = { coordinates[0], coordinates[1], coordinates[2] };

  for (int other = 0; other < 3; ++other) {
    position[other] = crossing[other];
  }

  return true;
}

// D238 discovers observed crossings of all 32-unit faces in the same bounded
// 3x3x3 coarse-cell window as D234. No source count asserts traversability.
inline bool discoverFineCrossings(
  const EvidenceTrialSet &sources,
  double x,
  double y,
  double z,
  double heading,
  FineCrossingDiscovery &result
) {
  if (sources.trials.empty() || sources.trials.size() > kMaxTrialSources
    || !spatialCoordinate(x) || !spatialCoordinate(y) || !spatialCoordinate(z)
    || !std::isfinite(heading)) {
    return false;
  }

  FineCrossingDiscovery next {};
  next.queryCoarse = { spatialCell(x), spatialCell(y), spatialCell(z) };
  next.headingBin = directionBin(heading);
  next.sources = static_cast<int>(sources.trials.size());

  using FaceKey = std::pair<SpatialCell, int>;
  std::map<FaceKey, FineCrossingFace> faces {};
  std::set<int> progressSources {};
  std::set<int> restrictionSources {};
  const int coarse[3] {
    next.queryCoarse.x, next.queryCoarse.y, next.queryCoarse.z
  };
  const int minFine[3] {
    (coarse[0] - kSpanCoarseRadius) * kRegionCellsPerAxis,
    (coarse[1] - kSpanCoarseRadius) * kRegionCellsPerAxis,
    (coarse[2] - kSpanCoarseRadius) * kRegionCellsPerAxis
  };
  const int maxFine[3] {
    minFine[0] + kSpanCellsPerAxis - 1,
    minFine[1] + kSpanCellsPerAxis - 1,
    minFine[2] + kSpanCellsPerAxis - 1
  };

  for (std::size_t source = 0; source < sources.trials.size(); ++source) {
    SpatialIndex index {};

    if (!buildSpatialIndex(sources.trials[source].rows, index)) {
      return false;
    }

    for (const SpatialRecord &record : index.records) {
      double recordedHeading {};

      if (!spatialRecordHeading(record, recordedHeading)) {
        return false;
      }

      if (directionBin(recordedHeading) != next.headingBin) {
        continue;
      }

      if (record.classification != MovementClassification::Progress
        && record.classification != MovementClassification::BlockedCandidate
        && record.classification != MovementClassification::DeflectedCandidate) {
        return false;
      }

      bool anyCrossing = false;

      for (int axis = 0; axis < 3; ++axis) {
        // The 11 interior planes of the 12x12x12 fine-cell window.
        for (int planeFine = minFine[axis] + 1; planeFine <= maxFine[axis]; ++planeFine) {
          SpatialCell lowerCell {};
          double position[3] {};

          if (!crossingAtFineFace(record, axis, planeFine, lowerCell, position)) {
            continue;
          }

          const int fine[3] {
            lowerCell.x, lowerCell.y, lowerCell.z
          };
          bool withinWindow = true;

          for (int other = 0; other < 3; ++other) {
            const int upper = fine[other] + (axis == other ? 1 : 0);

            if (fine[other] < minFine[other] || upper > maxFine[other]) {
              withinWindow = false;
              break;
            }
          }

          if (!withinWindow) {
            continue;
          }

          const FaceKey key { lowerCell, axis };
          auto [iterator, inserted] = faces.try_emplace(key);
          FineCrossingFace &face = iterator->second;

          if (inserted) {
            face.lowerCell = lowerCell;
            face.axis = axis;
            face.coarseBoundary = crossingCoarseCell(planeFine - 1)
              != crossingCoarseCell(planeFine);

            for (int other = 0; other < 3; ++other) {
              face.position[other] = position[other];
            }

            if (faces.size() > kMaxFineCrossingFaces) {
              return false;
            }
          }

          const bool positive = record.end[axis] > record.start[axis];

          if (positive) {
            face.positiveSources.insert(static_cast<int>(source));
          }
          else {
            face.negativeSources.insert(static_cast<int>(source));
          }

          switch (record.classification) {
          case MovementClassification::Progress:
            positive ? ++face.positiveProgress : ++face.negativeProgress;
            face.progressSources.insert(static_cast<int>(source));
            break;
          case MovementClassification::BlockedCandidate:
            positive ? ++face.positiveBlocked : ++face.negativeBlocked;
            face.restrictionSources.insert(static_cast<int>(source));
            break;
          case MovementClassification::DeflectedCandidate:
            positive ? ++face.positiveDeflected : ++face.negativeDeflected;
            face.restrictionSources.insert(static_cast<int>(source));
            break;
          case MovementClassification::InsufficientEvidence:
            return false;
          }

          anyCrossing = true;
        }
      }

      // One record traversing many fine planes contributes one summary window.
      if (anyCrossing) {
        if (record.classification == MovementClassification::Progress) {
          ++next.progressWindows;
          progressSources.insert(static_cast<int>(source));
        }
        else {
          restrictionSources.insert(static_cast<int>(source));

          if (record.classification == MovementClassification::BlockedCandidate) {
            ++next.blockedWindows;
          }
          else {
            ++next.deflectedWindows;
          }
        }
      }
    }
  }

  next.faces = static_cast<int>(faces.size());
  next.progressSources = static_cast<int>(progressSources.size());
  next.restrictionSources = static_cast<int>(restrictionSources.size());

  for (const auto &entry : faces) {
    const FineCrossingFace &face = entry.second;
    face.coarseBoundary ? ++next.coarseFaces : ++next.interiorFaces;

    if (!face.progressSources.empty()) {
      ++next.progressFaces;
    }

    if (!face.restrictionSources.empty()) {
      ++next.restrictionFaces;
    }

    next.details.push_back(face);
  }

  // Place the nearest crossings first to bound console output usefully.
  const auto distanceSquared = [x, y, z](const FineCrossingFace &face) {
    const double dx = face.position[0] - x;
    const double dy = face.position[1] - y;
    const double dz = face.position[2] - z;
    return dx * dx + dy * dy + dz * dz;
  };
  std::sort(next.details.begin(), next.details.end(),
    [&distanceSquared](const FineCrossingFace &a, const FineCrossingFace &b) {
      const double da = distanceSquared(a);
      const double db = distanceSquared(b);

      if (da != db) {
        return da < db;
      }

      if (a.lowerCell < b.lowerCell) {
        return true;
      }

      if (b.lowerCell < a.lowerCell) {
        return false;
      }

      return a.axis < b.axis;
    });

  result = std::move(next);
  return true;
}

} // namespace aipb::explorer::bridge
