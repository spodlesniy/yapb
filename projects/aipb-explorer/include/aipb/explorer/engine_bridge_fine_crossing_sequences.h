#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <set>
#include <utility>
#include <vector>

#include <aipb/explorer/engine_bridge_fine_crossing_sectors.h>

namespace aipb::explorer::bridge {

inline constexpr int kMaxFineSequencePrintedChains = 8;
inline constexpr int kMaxFineSequencePrintedSteps = 12;

struct FineSequenceStep {
  SpatialCell lowerCell {};
  int axis {};
  bool positive {};
  double fraction {};
  double position[3] {};
};

struct FineCrossingSequence {
  int source {};
  int firstStep {};
  int headingBin {};
  MovementClassification classification { MovementClassification::InsufficientEvidence };
  std::vector<FineSequenceStep> steps {};
};

struct FineSequenceAudit {
  SpatialCell queryCoarse {};
  int headingBin {};
  int sources {};
  int records {};
  int crossingRecords {};
  int singleFaceRecords {};
  int ambiguousRecords {};
  int discontinuousRecords {};
  int chains {};
  int orderedPairs {};
  int progressChains {};
  int restrictionChains {};
  int chainSources {};
  std::vector<FineCrossingSequence> details {};
};

inline SpatialCell fineSequenceCell(
  const FineSequenceStep &step,
  bool destination
) {
  SpatialCell cell = step.lowerCell;
  const bool upper = destination ? step.positive : !step.positive;

  if (upper) {
    if (step.axis == 0) {
      ++cell.x;
    }
    else if (step.axis == 1) {
      ++cell.y;
    }
    else {
      ++cell.z;
    }
  }

  return cell;
}

inline bool sameFineSequenceCell(
  const SpatialCell &a,
  const SpatialCell &b
) {
  return a.x == b.x && a.y == b.y && a.z == b.z;
}

// D241 orders strict grid-face intersections within one recorded straight
// segment. Any edge or corner ambiguity inside the query window vetoes the
// entire record; no links may be inferred between different records.
inline bool auditFineCrossingSequences(
  const EvidenceTrialSet &sources,
  double x,
  double y,
  double z,
  double heading,
  FineSequenceAudit &result
) {
  if (sources.trials.empty() || sources.trials.size() > kMaxTrialSources
    || !spatialCoordinate(x) || !spatialCoordinate(y) || !spatialCoordinate(z)
    || !std::isfinite(heading)) {
    return false;
  }

  FineSequenceAudit next {};
  next.queryCoarse = { spatialCell(x), spatialCell(y), spatialCell(z) };
  next.headingBin = directionBin(heading);
  next.sources = static_cast<int>(sources.trials.size());

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
  std::set<int> chainSources {};

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

      ++next.records;
      bool ambiguous = false;
      std::vector<FineSequenceStep> steps {};

      for (int axis = 0; axis < 3; ++axis) {
        for (int planeFine = minFine[axis] + 1; planeFine <= maxFine[axis]; ++planeFine) {
          const double plane = static_cast<double>(planeFine) * kLocalizedCellSize;
          const double start = record.start[axis];
          const double end = record.end[axis];

          if (!((start < plane && end > plane) || (start > plane && end < plane))) {
            continue;
          }

          const double fraction = (plane - start) / (end - start);
          bool insideWindow = true;

          for (int other = 0; other < 3; ++other) {
            if (other == axis) {
              continue;
            }

            const double coordinate = record.start[other]
              + fraction * (record.end[other] - record.start[other]);
            const double low = static_cast<double>(minFine[other]) * kLocalizedCellSize;
            const double high = static_cast<double>(maxFine[other] + 1) * kLocalizedCellSize;

            if (coordinate < low || coordinate > high) {
              insideWindow = false;
              break;
            }
          }

          if (!insideWindow) {
            continue;
          }

          FineSequenceStep step {};
          step.axis = axis;
          step.positive = end > start;
          step.fraction = fraction;

          if (!crossingAtFineFace(record, axis, planeFine, step.lowerCell, step.position)) {
            ambiguous = true;
            continue;
          }

          const int fine[3] {
            step.lowerCell.x, step.lowerCell.y, step.lowerCell.z
          };

          for (int other = 0; other < 3; ++other) {
            const int upper = fine[other] + (other == axis ? 1 : 0);

            if (fine[other] < minFine[other] || upper > maxFine[other]) {
              insideWindow = false;
              break;
            }
          }

          if (insideWindow) {
            steps.push_back(step);
          }
        }
      }

      if (ambiguous) {
        ++next.ambiguousRecords;
        continue;
      }

      if (steps.empty()) {
        continue;
      }

      std::sort(steps.begin(), steps.end(),
        [](const FineSequenceStep &a, const FineSequenceStep &b) {
          if (a.fraction != b.fraction) {
            return a.fraction < b.fraction;
          }

          return a.axis < b.axis;
        });

      bool contiguous = true;

      for (std::size_t i = 1; i < steps.size(); ++i) {
        if (!(steps[i - 1].fraction < steps[i].fraction)
          || !sameFineSequenceCell(fineSequenceCell(steps[i - 1], true),
            fineSequenceCell(steps[i], false))) {
          contiguous = false;
          break;
        }
      }

      if (!contiguous) {
        ++next.discontinuousRecords;
        continue;
      }

      ++next.crossingRecords;

      if (steps.size() == 1) {
        ++next.singleFaceRecords;
        continue;
      }

      FineCrossingSequence chain {};
      chain.source = static_cast<int>(source);
      chain.firstStep = record.firstStep;
      chain.headingBin = next.headingBin;
      chain.classification = record.classification;
      chain.steps = std::move(steps);

      ++next.chains;
      next.orderedPairs += static_cast<int>(chain.steps.size()) - 1;

      if (chain.classification == MovementClassification::Progress) {
        ++next.progressChains;
      }
      else {
        ++next.restrictionChains;
      }

      chainSources.insert(static_cast<int>(source));
      next.details.push_back(std::move(chain));
    }
  }

  next.chainSources = static_cast<int>(chainSources.size());
  result = std::move(next);
  return true;
}

} // namespace aipb::explorer::bridge
