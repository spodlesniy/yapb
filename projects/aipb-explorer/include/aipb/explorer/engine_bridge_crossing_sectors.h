#pragma once

#include <cmath>
#include <cstddef>
#include <map>
#include <utility>
#include <vector>

#include <aipb/explorer/engine_bridge_directed_crossing_consensus.h>

namespace aipb::explorer::bridge {

struct CrossingSectorOutcome {
  DirectedCrossingEvidence evidence {};
  unsigned int progressBins {};
  unsigned int restrictionBins {};
};

struct CrossingSectorFace {
  SpatialCell lowerCell {};
  int axis {};
  double position[3] {};
  CrossingSectorOutcome positive {};
  CrossingSectorOutcome negative {};
};

struct CrossingSectorAudit {
  SpatialCell queryCoarse {};
  int sources {};
  int faces {};
  int multipleSectorFaces {};
  int crossSectorConflictDirections {};
  int progressEvidenceDirections {};
  int restrictionEvidenceDirections {};
  int conflictedDirections {};
  int insufficientDirections {};
  int unobservedDirections {};
  std::vector<CrossingSectorFace> details {};
};

inline bool crossingMultipleSectors(unsigned int bins) {
  return (bins & (bins - 1u)) != 0u;
}

// At least one Progress observation and one candidate restriction have
// different recorded yaw bins, regardless of whether a common bin also exists.
inline bool crossingSectorOpposition(
  unsigned int progressBins,
  unsigned int restrictionBins
) {
  for (int bin = 0; bin < kDirectionBins; ++bin) {
    const unsigned int bit = 1u << bin;

    if ((progressBins & bit) != 0u
      && (restrictionBins & ~bit) != 0u) {
      return true;
    }
  }

  return false;
}

// Aggregate all yaw sectors without merging physical crossing signs or
// inflating source independence when a single trial changes heading.
inline bool auditCrossingSectors(
  const EvidenceTrialSet &sources,
  double x,
  double y,
  double z,
  CrossingSectorAudit &result
) {
  if (sources.trials.empty() || sources.trials.size() > kMaxTrialSources
    || !spatialCoordinate(x) || !spatialCoordinate(y) || !spatialCoordinate(z)) {
    return false;
  }

  CrossingSectorAudit next {};
  next.queryCoarse = { spatialCell(x), spatialCell(y), spatialCell(z) };
  next.sources = static_cast<int>(sources.trials.size());

  using FaceKey = std::pair<SpatialCell, int>;
  using SourceDirections = std::pair<DirectedCrossingSourceWindows, DirectedCrossingSourceWindows>;
  std::map<FaceKey, CrossingSectorFace> faces {};
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

      if (record.classification != MovementClassification::Progress
        && record.classification != MovementClassification::BlockedCandidate
        && record.classification != MovementClassification::DeflectedCandidate) {
        return false;
      }

      const unsigned int bin = 1u << directionBin(recordedHeading);
      const bool progress = record.classification == MovementClassification::Progress;

      for (int axis = 0; axis < 3; ++axis) {
        for (int offset = 0; offset <= 1; ++offset) {
          const int planeCoarse = coarse[axis] + offset;
          SpatialCell lowerCell {};

          if (!crossingAtFace(record, axis, planeCoarse, lowerCell)) {
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
          CrossingSectorFace &face = iterator->second;

          if (inserted) {
            face.lowerCell = lowerCell;
            face.axis = axis;
            const double plane = static_cast<double>(planeCoarse) * kSpatialCellSize;
            const double t = (plane - record.start[axis])
              / (record.end[axis] - record.start[axis]);

            for (int other = 0; other < 3; ++other) {
              face.position[other] = other == axis ? plane
                : record.start[other] + t * (record.end[other] - record.start[other]);
            }

            if (faces.size() > kMaxDiscoverableCrossingFaces) {
              return false;
            }
          }

          const bool positive = record.end[axis] > record.start[axis];
          CrossingSectorOutcome &outcome = positive ? face.positive : face.negative;
          DirectedCrossingSourceWindows &windows = positive
            ? votes[key].first : votes[key].second;

          if (progress) {
            ++windows.progress;
            outcome.progressBins |= bin;
          }
          else {
            ++windows.restriction;
            outcome.restrictionBins |= bin;
          }
        }
      }
    }

    for (const auto &entry : votes) {
      CrossingSectorFace &face = faces.at(entry.first);
      addDirectedCrossingSource(face.positive.evidence, entry.second.first);
      addDirectedCrossingSource(face.negative.evidence, entry.second.second);
    }
  }

  next.faces = static_cast<int>(faces.size());

  for (auto &entry : faces) {
    CrossingSectorFace &face = entry.second;
    classifyDirectedCrossingEvidence(face.positive.evidence);
    classifyDirectedCrossingEvidence(face.negative.evidence);

    const unsigned int allBins = face.positive.progressBins | face.positive.restrictionBins
      | face.negative.progressBins | face.negative.restrictionBins;

    if (crossingMultipleSectors(allBins)) {
      ++next.multipleSectorFaces;
    }

    const CrossingSectorOutcome outcomes[2] {
      face.positive, face.negative
    };

    for (const CrossingSectorOutcome &outcome : outcomes) {
      if (crossingSectorOpposition(outcome.progressBins, outcome.restrictionBins)) {
        ++next.crossSectorConflictDirections;
      }

      switch (outcome.evidence.status) {
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

    next.details.push_back(face);
  }

  result = std::move(next);
  return true;
}

} // namespace aipb::explorer::bridge
