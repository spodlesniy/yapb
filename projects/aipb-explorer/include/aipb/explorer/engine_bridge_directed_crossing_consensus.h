#pragma once

#include <cstddef>
#include <map>
#include <utility>
#include <vector>

#include <aipb/explorer/engine_bridge_crossing_directions.h>

namespace aipb::explorer::bridge {

struct DirectedCrossingEvidence {
  int progressWindows {};
  int restrictionWindows {};
  int observedSources {};
  int weakSources {};
  int strongProgressSources {};
  int strongRestrictionSources {};
  int mixedSources {};
  int progressSources {};
  int restrictionSources {};
  DirectionAssessmentStatus status { DirectionAssessmentStatus::Unobserved };
};

struct DirectedCrossingConsensusFace {
  DirectedCrossingFace crossing {};
  DirectedCrossingEvidence positive {};
  DirectedCrossingEvidence negative {};
};

struct DirectedCrossingConsensusAudit {
  CrossingDirectionAudit directions {};
  int progressEvidenceDirections {};
  int restrictionEvidenceDirections {};
  int conflictedDirections {};
  int insufficientDirections {};
  int unobservedDirections {};
  std::vector<DirectedCrossingConsensusFace> details {};
};

struct DirectedCrossingSourceWindows {
  int progress {};
  int restriction {};
};

// Source quality is evaluated per physical face and actual travel sign.
// Multiple windows in one trace never become independent corroborations.
inline void addDirectedCrossingSource(
  DirectedCrossingEvidence &evidence,
  const DirectedCrossingSourceWindows &windows
) {
  evidence.progressWindows += windows.progress;
  evidence.restrictionWindows += windows.restriction;

  if (windows.progress == 0 && windows.restriction == 0) {
    return;
  }

  ++evidence.observedSources;
  const bool progress = windows.progress > 0;
  const bool restriction = windows.restriction > 0;

  if (progress) {
    ++evidence.progressSources;
  }

  if (restriction) {
    ++evidence.restrictionSources;
  }

  if (progress && restriction) {
    ++evidence.mixedSources;
  }
  else if (windows.progress + windows.restriction < kMinimumSourceWindows) {
    ++evidence.weakSources;
  }
  else if (progress) {
    ++evidence.strongProgressSources;
  }
  else {
    ++evidence.strongRestrictionSources;
  }
}

inline void classifyDirectedCrossingEvidence(DirectedCrossingEvidence &evidence) {
  SourceConsensus quality {};
  quality.observedSources = evidence.observedSources;
  quality.strongProgressSources = evidence.strongProgressSources;
  quality.strongRestrictionSources = evidence.strongRestrictionSources;
  quality.mixedSources = evidence.mixedSources;
  quality.progressSources = evidence.progressSources;
  quality.restrictionSources = evidence.restrictionSources;
  evidence.status = classifySourceConsensus(quality);
}

// A crossing is observed trajectory geometry, not a GoldSrc hull clearance
// test. Opposite signs and requested yaw never contribute votes to one another.
inline bool auditDirectedCrossingConsensus(
  const EvidenceTrialSet &sources,
  double x,
  double y,
  double z,
  double heading,
  DirectedCrossingConsensusAudit &result
) {
  DirectedCrossingConsensusAudit next {};

  if (!auditCrossingDirections(sources, x, y, z, heading, next.directions)) {
    return false;
  }

  using FaceKey = std::pair<SpatialCell, int>;
  using SourceDirections = std::pair<DirectedCrossingSourceWindows, DirectedCrossingSourceWindows>;
  std::map<FaceKey, std::size_t> faceIndices {};

  for (const DirectedCrossingFace &face : next.directions.details) {
    const FaceKey key { face.discovered.lowerCell, face.discovered.axis };
    faceIndices.emplace(key, next.details.size());
    DirectedCrossingConsensusFace detail {};
    detail.crossing = face;
    next.details.push_back(std::move(detail));
  }

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

      if (directionBin(recordedHeading) != next.directions.discovered.headingBin) {
        continue;
      }

      if (record.classification != MovementClassification::Progress
        && record.classification != MovementClassification::BlockedCandidate
        && record.classification != MovementClassification::DeflectedCandidate) {
        return false;
      }

      for (int axis = 0; axis < 3; ++axis) {
        for (int offset = 0; offset <= 1; ++offset) {
          const int plane = crossingAxis(next.directions.discovered.queryCoarse, axis) + offset;
          SpatialCell lowerCell {};

          if (!crossingAtFace(record, axis, plane, lowerCell)) {
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
      DirectedCrossingConsensusFace &face = next.details[faceIndices.at(entry.first)];
      addDirectedCrossingSource(face.positive, entry.second.first);
      addDirectedCrossingSource(face.negative, entry.second.second);
    }
  }

  for (DirectedCrossingConsensusFace &face : next.details) {
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
