#pragma once

#include <cmath>
#include <cstddef>
#include <map>
#include <set>
#include <utility>

#include <aipb/explorer/engine_bridge_cross_cell_regions.h>

namespace aipb::explorer::bridge {

struct BoundaryFaceEvidence {
  std::set<int> progressSources {};
  std::set<int> restrictionSources {};
};

struct BoundaryCrossingAudit {
  CrossCellRegion span {};
  int coarseBoundaryFaces {};
  int facesWithProgress {};
  int facesWithRestriction {};
  int facesWithAnyCrossing {};
  int facesWithMultipleProgressSources {};
  int unsupportedFaces {};
  int progressCrossingWindows {};
  int restrictionCrossingWindows {};
  int progressCrossingSources {};
  int restrictionCrossingSources {};
};

inline int crossingCoarseCell(int fineCell) {
  return fineCell >= 0 ? fineCell / kRegionCellsPerAxis
    : (fineCell - kRegionCellsPerAxis + 1) / kRegionCellsPerAxis;
}

inline int crossingAxis(const SpatialCell &cell, int axis) {
  return axis == 0 ? cell.x : (axis == 1 ? cell.y : cell.z);
}

// A direct crossing has endpoints strictly on opposite sides of the coarse
// plane and meets the interior of a shared fine-cell face. Mere contact at
// an endpoint, an edge or a corner is not evidence of a crossing.
inline bool crossingAtFace(const SpatialRecord &record, int axis, int planeCoarse,
  SpatialCell &lowerCell) {
  const double plane = static_cast<double>(planeCoarse) * kSpatialCellSize;
  const double start = record.start[axis];
  const double end = record.end[axis];

  if (!((start < plane && end > plane) || (start > plane && end < plane))) {
    return false;
  }

  const double t = (plane - start) / (end - start);
  int coordinates[3] {};

  for (int other = 0; other < 3; ++other) {
    if (other == axis) {
      coordinates[other] = planeCoarse * kRegionCellsPerAxis - 1;
      continue;
    }

    const double crossing = record.start[other]
      + t * (record.end[other] - record.start[other]);

    if (!spatialCoordinate(crossing)) {
      return false;
    }

    const int fine = localizedCell(crossing);
    const double position = crossing - static_cast<double>(fine) * kLocalizedCellSize;

    if (position <= 1e-9 || position >= kLocalizedCellSize - 1e-9) {
      return false;
    }

    coordinates[other] = fine;
  }

  lowerCell = { coordinates[0], coordinates[1], coordinates[2] };
  return true;
}

// An observed segment crossing is stronger than neighboring occupied cells,
// but is still not a GoldSrc hull collision check or a passability guarantee.
inline bool auditCoarseBoundaryCrossings(
  const EvidenceTrialSet &sources, double x, double y, double z, double heading,
  BoundaryCrossingAudit &result
) {
  BoundaryCrossingAudit next {};
  std::set<SpatialCell> members {};

  if (!spanSourceEvidence(sources, x, y, z, heading, next.span, &members)) {
    return false;
  }

  using FaceKey = std::pair<SpatialCell, int>;
  std::map<FaceKey, BoundaryFaceEvidence> faces {};

  for (const SpatialCell &cell : members) {
    for (int axis = 0; axis < 3; ++axis) {
      SpatialCell neighbor = cell;

      if (axis == 0) {
        ++neighbor.x;
      }
      else if (axis == 1) {
        ++neighbor.y;
      }
      else {
        ++neighbor.z;
      }

      if (members.find(neighbor) != members.end()
        && crossingCoarseCell(crossingAxis(cell, axis))
          != crossingCoarseCell(crossingAxis(neighbor, axis))) {
        faces[{ cell, axis }] = {};
      }
    }
  }

  next.coarseBoundaryFaces = static_cast<int>(faces.size());

  if (faces.empty()) {
    result = std::move(next);
    return true;
  }

  std::set<int> progressSources {};
  std::set<int> restrictionSources {};

  for (std::size_t source = 0; source < sources.trials.size(); ++source) {
    SpatialIndex index {};

    if (!buildSpatialIndex(sources.trials[source].rows, index)) {
      return false;
    }

    for (const SpatialRecord &record : index.records) {
      double recordHeading {};

      if (!spatialRecordHeading(record, recordHeading)) {
        return false;
      }

      if (directionBin(recordHeading) != next.span.headingBin) {
        continue;
      }

      std::set<FaceKey> crossedFaces {};

      for (int axis = 0; axis < 3; ++axis) {
        for (int offset = -1; offset <= 1; ++offset) {
          const int coarse = crossingAxis(next.span.seedCoarse, axis) + offset;
          SpatialCell lowerCell {};

          if (crossingAtFace(record, axis, coarse, lowerCell)) {
            const FaceKey key { lowerCell, axis };

            if (faces.find(key) != faces.end()) {
              crossedFaces.insert(key);
            }
          }
        }
      }

      if (crossedFaces.empty()) {
        continue;
      }

      const bool progress = record.classification == MovementClassification::Progress;

      if (record.classification != MovementClassification::Progress
        && record.classification != MovementClassification::BlockedCandidate
        && record.classification != MovementClassification::DeflectedCandidate) {
        return false;
      }

      if (progress) {
        ++next.progressCrossingWindows;
        progressSources.insert(static_cast<int>(source));
      }
      else {
        ++next.restrictionCrossingWindows;
        restrictionSources.insert(static_cast<int>(source));
      }

      for (const FaceKey &key : crossedFaces) {
        BoundaryFaceEvidence &evidence = faces.at(key);

        if (progress) {
          evidence.progressSources.insert(static_cast<int>(source));
        }
        else {
          evidence.restrictionSources.insert(static_cast<int>(source));
        }
      }
    }
  }

  next.progressCrossingSources = static_cast<int>(progressSources.size());
  next.restrictionCrossingSources = static_cast<int>(restrictionSources.size());

  for (const auto &entry : faces) {
    const BoundaryFaceEvidence &evidence = entry.second;

    if (!evidence.progressSources.empty()) {
      ++next.facesWithProgress;
    }

    if (!evidence.restrictionSources.empty()) {
      ++next.facesWithRestriction;
    }

    if (!evidence.progressSources.empty() || !evidence.restrictionSources.empty()) {
      ++next.facesWithAnyCrossing;
    }

    if (evidence.progressSources.size() >= kMinimumCorroboratingSources) {
      ++next.facesWithMultipleProgressSources;
    }
  }

  next.unsupportedFaces = next.coarseBoundaryFaces - next.facesWithAnyCrossing;
  result = std::move(next);
  return true;
}

} // namespace aipb::explorer::bridge
