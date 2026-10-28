#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include <aipb/explorer/engine_bridge_boundary_crossings.h>

namespace aipb::explorer::bridge {

inline constexpr int kMaxDiscoverableCrossingFaces =
  3 * (2 * kSpanCoarseRadius) * kSpanCellsPerAxis * kSpanCellsPerAxis;

struct DiscoveredCrossingFace {
  SpatialCell lowerCell {};
  int axis {};
  double position[3] {};
  int progressWindows {};
  int restrictionWindows {};
  std::set<int> progressSources {};
  std::set<int> restrictionSources {};
};

struct CrossingDiscovery {
  SpatialCell queryCoarse {};
  int headingBin {};
  int sources {};
  int faces {};
  int progressFaces {};
  int restrictionFaces {};
  int multipleProgressSourceFaces {};
  int progressWindows {};
  int restrictionWindows {};
  int progressSources {};
  int restrictionSources {};
  std::vector<DiscoveredCrossingFace> details {};
};

// Search recorded segments, not just a D232 component. Two observed adjacent
// cells do not imply that any single recorded segment traversed their boundary.
inline bool discoverSourceCrossings(
  const EvidenceTrialSet &sources, double x, double y, double z, double heading,
  CrossingDiscovery &result
) {
  if (sources.trials.empty() || sources.trials.size() > kMaxTrialSources
    || !spatialCoordinate(x) || !spatialCoordinate(y) || !spatialCoordinate(z)
    || !std::isfinite(heading)) {
    return false;
  }

  CrossingDiscovery next {};
  next.queryCoarse = { spatialCell(x), spatialCell(y), spatialCell(z) };
  next.headingBin = directionBin(heading);
  next.sources = static_cast<int>(sources.trials.size());

  using FaceKey = std::pair<SpatialCell, int>;
  std::map<FaceKey, DiscoveredCrossingFace> faceDetails {};
  std::set<int> progressSources {};
  std::set<int> restrictionSources {};
  const int coarse[3] { next.queryCoarse.x, next.queryCoarse.y, next.queryCoarse.z };
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

      const bool progress = record.classification == MovementClassification::Progress;
      bool anyCrossing = false;

      for (int axis = 0; axis < 3; ++axis) {
        // Only the two interior coarse planes of the bounded 3x3x3 window.
        for (int offset = 0; offset <= 1; ++offset) {
          const int planeCoarse = coarse[axis] + offset;
          SpatialCell lowerCell {};

          if (!crossingAtFace(record, axis, planeCoarse, lowerCell)) {
            continue;
          }

          const int fine[3] { lowerCell.x, lowerCell.y, lowerCell.z };
          bool withinWindow = true;

          for (int other = 0; other < 3; ++other) {
            const int upperPart = fine[other] + (axis == other ? 1 : 0);

            if (fine[other] < minFine[other] || upperPart > maxFine[other]) {
              withinWindow = false;
              break;
            }
          }

          if (!withinWindow) {
            continue;
          }

          const FaceKey key { lowerCell, axis };
          auto inserted = faceDetails.try_emplace(key);
          DiscoveredCrossingFace &face = inserted.first->second;

          if (inserted.second) {
            face.lowerCell = lowerCell;
            face.axis = axis;
            const double plane = static_cast<double>(planeCoarse) * kSpatialCellSize;
            const double t = (plane - record.start[axis])
              / (record.end[axis] - record.start[axis]);

            for (int other = 0; other < 3; ++other) {
              face.position[other] = other == axis ? plane
                : record.start[other] + t * (record.end[other] - record.start[other]);
            }
          }

          if (progress) {
            ++face.progressWindows;
            face.progressSources.insert(static_cast<int>(source));
          }
          else {
            ++face.restrictionWindows;
            face.restrictionSources.insert(static_cast<int>(source));
          }

          anyCrossing = true;

          if (faceDetails.size() > kMaxDiscoverableCrossingFaces) {
            return false;
          }
        }
      }

      if (anyCrossing) {
        if (progress) {
          ++next.progressWindows;
          progressSources.insert(static_cast<int>(source));
        }
        else {
          ++next.restrictionWindows;
          restrictionSources.insert(static_cast<int>(source));
        }
      }
    }
  }

  next.progressSources = static_cast<int>(progressSources.size());
  next.restrictionSources = static_cast<int>(restrictionSources.size());
  next.faces = static_cast<int>(faceDetails.size());

  for (const auto &entry : faceDetails) {
    const DiscoveredCrossingFace &face = entry.second;

    if (!face.progressSources.empty()) {
      ++next.progressFaces;
    }

    if (!face.restrictionSources.empty()) {
      ++next.restrictionFaces;
    }

    if (face.progressSources.size() >= kMinimumCorroboratingSources) {
      ++next.multipleProgressSourceFaces;
    }

    next.details.push_back(face);
  }

  result = std::move(next);
  return true;
}

} // namespace aipb::explorer::bridge
