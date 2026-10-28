#pragma once

#include <cstddef>
#include <map>
#include <utility>
#include <vector>

#include <aipb/explorer/engine_bridge_fine_crossings.h>

namespace aipb::explorer::bridge {

struct FineCrossingConsensusFace {
  FineCrossingFace crossing {};
  DirectedCrossingEvidence positive {};
  DirectedCrossingEvidence negative {};
};

struct FineCrossingConsensusAudit {
  FineCrossingDiscovery discovery {};
  int progressEvidenceDirections {};
  int restrictionEvidenceDirections {};
  int conflictedDirections {};
  int insufficientDirections {};
  int unobservedDirections {};
  std::vector<FineCrossingConsensusFace> details {};
};

// Each strict fine-face crossing is assessed separately for positive and
// negative travel. Repeated windows within one trace are correlated evidence.
inline bool auditFineCrossingConsensus(
  const EvidenceTrialSet &sources,
  double x,
  double y,
  double z,
  double heading,
  FineCrossingConsensusAudit &result
) {
  FineCrossingConsensusAudit next {};

  if (!discoverFineCrossings(sources, x, y, z, heading, next.discovery)) {
    return false;
  }

  using FaceKey = std::pair<SpatialCell, int>;
  using SourceDirections = std::pair<DirectedCrossingSourceWindows, DirectedCrossingSourceWindows>;
  std::map<FaceKey, std::size_t> faceIndices {};

  for (const FineCrossingFace &face : next.discovery.details) {
    const FaceKey key { face.lowerCell, face.axis };
    faceIndices.emplace(key, next.details.size());

    FineCrossingConsensusFace detail {};
    detail.crossing = face;
    next.details.push_back(std::move(detail));
  }

  const SpatialCell &coarse = next.discovery.queryCoarse;
  const int minFine[3] {
    (coarse.x - kSpanCoarseRadius) * kRegionCellsPerAxis,
    (coarse.y - kSpanCoarseRadius) * kRegionCellsPerAxis,
    (coarse.z - kSpanCoarseRadius) * kRegionCellsPerAxis
  };

  for (const EvidenceTrial &trial : sources.trials) {
    SpatialIndex index {};

    if (!buildSpatialIndex(trial.rows, index)) {
      return false;
    }

    std::map<FaceKey, SourceDirections> votes {};

    for (const SpatialRecord &record : index.records) {
      double recordedHeading {};

      if (!spatialRecordHeading(record, recordedHeading)) {
        return false;
      }

      if (directionBin(recordedHeading) != next.discovery.headingBin) {
        continue;
      }

      if (record.classification != MovementClassification::Progress
        && record.classification != MovementClassification::BlockedCandidate
        && record.classification != MovementClassification::DeflectedCandidate) {
        return false;
      }

      for (int axis = 0; axis < 3; ++axis) {
        for (int planeFine = minFine[axis] + 1;
          planeFine < minFine[axis] + kSpanCellsPerAxis; ++planeFine) {
          SpatialCell lowerCell {};
          double position[3] {};

          if (!crossingAtFineFace(record, axis, planeFine, lowerCell, position)) {
            continue;
          }

          const FaceKey key { lowerCell, axis };

          if (faceIndices.find(key) == faceIndices.end()) {
            continue;
          }

          SourceDirections &direction = votes[key];
          DirectedCrossingSourceWindows &windows =
            record.end[axis] > record.start[axis] ? direction.first : direction.second;

          if (record.classification == MovementClassification::Progress) {
            ++windows.progress;
          }
          else {
            ++windows.restriction;
          }
        }
      }
    }

    for (const auto &entry : votes) {
      FineCrossingConsensusFace &face = next.details[faceIndices.at(entry.first)];
      addDirectedCrossingSource(face.positive, entry.second.first);
      addDirectedCrossingSource(face.negative, entry.second.second);
    }
  }

  for (FineCrossingConsensusFace &face : next.details) {
    classifyDirectedCrossingEvidence(face.positive);
    classifyDirectedCrossingEvidence(face.negative);

    const DirectionAssessmentStatus statuses[2] {
      face.positive.status, face.negative.status
    };

    for (const DirectionAssessmentStatus status : statuses) {
      switch (status) {
      case DirectionAssessmentStatus::ProgressEvidence:
        ++next.progressEvidenceDirections;
        break;
      case DirectionAssessmentStatus::RestrictionEvidence:
        ++next.restrictionEvidenceDirections;
        break;
      case DirectionAssessmentStatus::Conflicted:
        ++next.conflictedDirections;
        break;
      case DirectionAssessmentStatus::Insufficient:
        ++next.insufficientDirections;
        break;
      case DirectionAssessmentStatus::Unobserved:
        ++next.unobservedDirections;
        break;
      }
    }
  }

  result = std::move(next);
  return true;
}

} // namespace aipb::explorer::bridge
