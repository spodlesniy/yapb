#pragma once

#include <cmath>
#include <cstddef>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include <aipb/explorer/engine_bridge_crossing_discovery.h>

namespace aipb::explorer::bridge {

// A yaw-only heading cannot predict vertical movement. Near-tangential yaw
// components are reported separately rather than forcing an aligned/opposed
// label from a weak normal projection.
inline constexpr double kCrossingNormalProjectionThreshold = 0.25;

enum class CrossingHeadingRelation {
  WithHeading,
  AgainstHeading,
  TransverseHeading,
  VerticalAxis
};

inline CrossingHeadingRelation crossingHeadingRelation(
  int axis, double headingDegrees, double signedAxisDisplacement
) {
  if (axis == 2) {
    return CrossingHeadingRelation::VerticalAxis;
  }

  const double radians = std::fmod(headingDegrees, 360.0)
    * (3.14159265358979323846 / 180.0);
  const double projection = axis == 0 ? std::cos(radians) : std::sin(radians);

  if (std::abs(projection) < kCrossingNormalProjectionThreshold) {
    return CrossingHeadingRelation::TransverseHeading;
  }

  return signedAxisDisplacement * projection > 0
    ? CrossingHeadingRelation::WithHeading
    : CrossingHeadingRelation::AgainstHeading;
}

struct DirectedCrossingFace {
  DiscoveredCrossingFace discovered {};
  int positiveProgress {};
  int negativeProgress {};
  int positiveRestriction {};
  int negativeRestriction {};
  int withHeading {};
  int againstHeading {};
  int transverseHeading {};
  int verticalAxis {};
  std::set<int> positiveSources {};
  std::set<int> negativeSources {};
  std::set<int> againstHeadingSources {};
};

struct CrossingDirectionAudit {
  CrossingDiscovery discovered {};
  int positiveFaces {};
  int negativeFaces {};
  int bidirectionalFaces {};
  int againstHeadingFaces {};
  int againstHeadingWindows {};
  int transverseHeadingWindows {};
  int verticalAxisWindows {};
  std::vector<DirectedCrossingFace> details {};
};

// D234 discovers only strict interior-face crossings. D235 classifies each
// discovered face's observed travel sign against each source record's yaw.
// The query heading selects a 45-degree bin; it is not used as that record's yaw.
inline bool auditCrossingDirections(
  const EvidenceTrialSet &sources, double x, double y, double z, double heading,
  CrossingDirectionAudit &result
) {
  CrossingDirectionAudit next {};

  if (!discoverSourceCrossings(sources, x, y, z, heading, next.discovered)) {
    return false;
  }

  using FaceKey = std::pair<SpatialCell, int>;
  std::map<FaceKey, DirectedCrossingFace> faces {};

  for (const DiscoveredCrossingFace &face : next.discovered.details) {
    DirectedCrossingFace directed {};
    directed.discovered = face;
    faces.emplace(FaceKey { face.lowerCell, face.axis }, std::move(directed));
  }

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

      if (directionBin(recordedHeading) != next.discovered.headingBin) {
        continue;
      }

      if (record.classification != MovementClassification::Progress
        && record.classification != MovementClassification::BlockedCandidate
        && record.classification != MovementClassification::DeflectedCandidate) {
        return false;
      }

      bool opposedWindow = false;
      bool transverseWindow = false;
      bool verticalWindow = false;

      for (int axis = 0; axis < 3; ++axis) {
        for (int offset = 0; offset <= 1; ++offset) {
          const int planeCoarse = crossingAxis(next.discovered.queryCoarse, axis) + offset;
          SpatialCell lowerCell {};

          if (!crossingAtFace(record, axis, planeCoarse, lowerCell)) {
            continue;
          }

          const auto found = faces.find(FaceKey { lowerCell, axis });

          if (found == faces.end()) {
            continue;
          }

          DirectedCrossingFace &face = found->second;
          const double displacement = record.end[axis] - record.start[axis];
          const bool positive = displacement > 0;
          const bool progress = record.classification == MovementClassification::Progress;

          if (positive) {
            face.positiveSources.insert(static_cast<int>(source));

            if (progress) {
              ++face.positiveProgress;
            }
            else {
              ++face.positiveRestriction;
            }
          }
          else {
            face.negativeSources.insert(static_cast<int>(source));

            if (progress) {
              ++face.negativeProgress;
            }
            else {
              ++face.negativeRestriction;
            }
          }

          switch (crossingHeadingRelation(axis, recordedHeading, displacement)) {
          case CrossingHeadingRelation::WithHeading:
            ++face.withHeading;
            break;
          case CrossingHeadingRelation::AgainstHeading:
            ++face.againstHeading;
            face.againstHeadingSources.insert(static_cast<int>(source));
            opposedWindow = true;
            break;
          case CrossingHeadingRelation::TransverseHeading:
            ++face.transverseHeading;
            transverseWindow = true;
            break;
          case CrossingHeadingRelation::VerticalAxis:
            ++face.verticalAxis;
            verticalWindow = true;
            break;
          }
        }
      }

      // A record may cross multiple coarse planes. Summary window counts
      // include each source record only once per relationship category.
      if (opposedWindow) {
        ++next.againstHeadingWindows;
      }

      if (transverseWindow) {
        ++next.transverseHeadingWindows;
      }

      if (verticalWindow) {
        ++next.verticalAxisWindows;
      }
    }
  }

  for (const auto &entry : faces) {
    const DirectedCrossingFace &face = entry.second;

    if (!face.positiveSources.empty()) {
      ++next.positiveFaces;
    }

    if (!face.negativeSources.empty()) {
      ++next.negativeFaces;
    }

    if (!face.positiveSources.empty() && !face.negativeSources.empty()) {
      ++next.bidirectionalFaces;
    }

    if (face.againstHeading > 0) {
      ++next.againstHeadingFaces;
    }

    next.details.push_back(face);
  }

  result = std::move(next);
  return true;
}

} // namespace aipb::explorer::bridge
