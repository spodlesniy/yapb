#pragma once

#include <algorithm>
#include <cstddef>
#include <map>
#include <utility>
#include <vector>

#include <aipb/explorer/engine_bridge_fine_crossing_consensus.h>

namespace aipb::explorer::bridge {

struct FineSectorFace {
  SpatialCell lowerCell {};
  int axis {};
  double position[3] {};
  bool coarseBoundary {};
  CrossingSectorOutcome positive {};
  CrossingSectorOutcome negative {};
};

struct FineSectorAudit {
  SpatialCell queryCoarse {};
  int sources {};
  int faces {};
  int interiorFaces {};
  int coarseFaces {};
  int multipleSectorFaces {};
  int crossSectorConflictDirections {};
  int progressEvidenceDirections {};
  int restrictionEvidenceDirections {};
  int conflictedDirections {};
  int insufficientDirections {};
  int unobservedDirections {};
  std::vector<FineSectorFace> details {};
};

// Evaluate every recorded yaw sector against the same strict physical fine
// face. Source identity and actual travel sign remain independent of yaw.
inline bool auditFineCrossingSectors(
  const EvidenceTrialSet &sources,
  double x,
  double y,
  double z,
  FineSectorAudit &result
) {
  if (sources.trials.empty() || sources.trials.size() > kMaxTrialSources
    || !spatialCoordinate(x) || !spatialCoordinate(y) || !spatialCoordinate(z)) {
    return false;
  }

  FineSectorAudit next {};
  next.queryCoarse = { spatialCell(x), spatialCell(y), spatialCell(z) };
  next.sources = static_cast<int>(sources.trials.size());

  using FaceKey = std::pair<SpatialCell, int>;
  using SourceDirections = std::pair<DirectedCrossingSourceWindows, DirectedCrossingSourceWindows>;
  std::map<FaceKey, FineSectorFace> faces {};
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

    // One source's many records or yaw sectors never become multiple votes.
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
          FineSectorFace &face = iterator->second;

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
      FineSectorFace &face = faces.at(entry.first);
      addDirectedCrossingSource(face.positive.evidence, entry.second.first);
      addDirectedCrossingSource(face.negative.evidence, entry.second.second);
    }
  }

  next.faces = static_cast<int>(faces.size());

  for (auto &entry : faces) {
    FineSectorFace &face = entry.second;
    classifyDirectedCrossingEvidence(face.positive.evidence);
    classifyDirectedCrossingEvidence(face.negative.evidence);

    if (face.coarseBoundary) {
      ++next.coarseFaces;
    }
    else {
      ++next.interiorFaces;
    }

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

  // Bound console output to the nearest faces, as in D238 and D239.
  const auto distanceSquared = [x, y, z](const FineSectorFace &face) {
    const double dx = face.position[0] - x;
    const double dy = face.position[1] - y;
    const double dz = face.position[2] - z;
    return dx * dx + dy * dy + dz * dz;
  };
  std::sort(next.details.begin(), next.details.end(),
    [&distanceSquared](const FineSectorFace &a, const FineSectorFace &b) {
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
