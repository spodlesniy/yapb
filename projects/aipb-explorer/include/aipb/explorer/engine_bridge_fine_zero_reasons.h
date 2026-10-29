#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include <aipb/explorer/engine_bridge_fine_sequence_coverage.h>

namespace aipb::explorer::bridge {

inline constexpr int kMaxFineZeroExamples = 12;

enum class FineZeroReason {
  OutsideWindow,
  Stationary,
  BoundaryContact,
  SameFineCell,
  Other
};

inline const char *fineZeroReasonName(FineZeroReason reason) {
  switch (reason) {
  case FineZeroReason::OutsideWindow:
    return "OutsideWindow";
  case FineZeroReason::Stationary:
    return "Stationary";
  case FineZeroReason::BoundaryContact:
    return "BoundaryContact";
  case FineZeroReason::SameFineCell:
    return "SameFineCell";
  case FineZeroReason::Other:
    return "Other";
  }

  return "Other";
}

struct FineZeroCounts {
  int outside {};
  int stationary {};
  int boundaryContact {};
  int sameFineCell {};
  int other {};

  int total() const {
    return outside + stationary + boundaryContact + sameFineCell + other;
  }
};

struct FineZeroExample {
  int source {};
  int firstStep {};
  int headingBin {};
  MovementClassification classification { MovementClassification::InsufficientEvidence };
  FineZeroReason reason { FineZeroReason::Other };
  double start[3] {};
  double end[3] {};
};

struct FineZeroAudit {
  FineCoverageAudit coverage {};
  FineZeroCounts total {};
  FineZeroCounts bins[kDirectionBins] {};
  std::vector<FineZeroExample> examples {};
};

// Inclusive segment-box clipping distinguishes out-of-window records from
// valid short observations. It does not test collision or traversability.
inline bool fineZeroIntersectsWindow(
  const SpatialRecord &record,
  const int *minFine,
  const int *maxFine
) {
  double enter = 0.0;
  double leave = 1.0;

  for (int axis = 0; axis < 3; ++axis) {
    const double low = static_cast<double>(minFine[axis]) * kLocalizedCellSize;
    const double high = static_cast<double>(maxFine[axis] + 1) * kLocalizedCellSize;
    const double change = record.end[axis] - record.start[axis];

    if (change == 0.0) {
      if (record.start[axis] < low || record.start[axis] > high) {
        return false;
      }
    }
    else {
      const double a = (low - record.start[axis]) / change;
      const double b = (high - record.start[axis]) / change;
      enter = std::max(enter, std::min(a, b));
      leave = std::min(leave, std::max(a, b));

      if (enter > leave) {
        return false;
      }
    }
  }

  return true;
}

inline bool fineZeroBoundaryContact(const SpatialRecord &record) {
  for (int axis = 0; axis < 3; ++axis) {
    for (int endpoint = 0; endpoint < 2; ++endpoint) {
      const double value = endpoint == 0 ? record.start[axis] : record.end[axis];
      const double nearest = std::round(value / kLocalizedCellSize) * kLocalizedCellSize;

      if (std::abs(value - nearest) <= 1e-9) {
        return true;
      }
    }
  }

  return false;
}

inline FineZeroReason classifyFineZeroRecord(
  const SpatialRecord &record,
  const int *minFine,
  const int *maxFine
) {
  if (!fineZeroIntersectsWindow(record, minFine, maxFine)) {
    return FineZeroReason::OutsideWindow;
  }

  bool stationary = true;
  bool sameCell = true;

  for (int axis = 0; axis < 3; ++axis) {
    stationary = stationary && record.start[axis] == record.end[axis];
    sameCell = sameCell
      && localizedCell(record.start[axis]) == localizedCell(record.end[axis]);
  }

  if (stationary) {
    return FineZeroReason::Stationary;
  }

  if (fineZeroBoundaryContact(record)) {
    return FineZeroReason::BoundaryContact;
  }

  if (sameCell) {
    return FineZeroReason::SameFineCell;
  }

  return FineZeroReason::Other;
}

inline void addFineZeroReason(FineZeroCounts &counts, FineZeroReason reason) {
  switch (reason) {
  case FineZeroReason::OutsideWindow:
    ++counts.outside;
    break;
  case FineZeroReason::Stationary:
    ++counts.stationary;
    break;
  case FineZeroReason::BoundaryContact:
    ++counts.boundaryContact;
    break;
  case FineZeroReason::SameFineCell:
    ++counts.sameFineCell;
    break;
  case FineZeroReason::Other:
    ++counts.other;
    break;
  }
}

// Reproduce D241's strict crossing and ambiguity tests, then independently
// reconcile the zero-face counts against the D245 coverage audit.
inline bool auditFineZeroReasons(
  const EvidenceTrialSet &sources,
  double x,
  double y,
  double z,
  FineZeroAudit &result
) {
  FineZeroAudit next {};

  if (!auditFineSequenceCoverage(sources, x, y, z, next.coverage)) {
    return false;
  }

  const SpatialCell &coarse = next.coverage.queryCoarse;
  const int coords[3] { coarse.x, coarse.y, coarse.z };
  int minFine[3] {};
  int maxFine[3] {};

  for (int axis = 0; axis < 3; ++axis) {
    minFine[axis] = (coords[axis] - kSpanCoarseRadius) * kRegionCellsPerAxis;
    maxFine[axis] = minFine[axis] + kSpanCellsPerAxis - 1;
  }

  for (std::size_t source = 0; source < sources.trials.size(); ++source) {
    SpatialIndex index {};

    if (!buildSpatialIndex(sources.trials[source].rows, index)) {
      return false;
    }

    for (const SpatialRecord &record : index.records) {
      double yaw {};

      if (!spatialRecordHeading(record, yaw)) {
        return false;
      }

      const int bin = directionBin(yaw);
      bool strict = false;
      bool ambiguous = false;

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

            const double crossing = record.start[other]
              + fraction * (record.end[other] - record.start[other]);
            const double low = static_cast<double>(minFine[other]) * kLocalizedCellSize;
            const double high = static_cast<double>(maxFine[other] + 1) * kLocalizedCellSize;

            if (crossing < low || crossing > high) {
              insideWindow = false;
              break;
            }
          }

          if (!insideWindow) {
            continue;
          }

          SpatialCell lower {};
          double position[3] {};

          if (!crossingAtFineFace(record, axis, planeFine, lower, position)) {
            ambiguous = true;
            continue;
          }

          const int fine[3] { lower.x, lower.y, lower.z };

          for (int other = 0; other < 3; ++other) {
            const int upper = fine[other] + (other == axis ? 1 : 0);

            if (fine[other] < minFine[other] || upper > maxFine[other]) {
              insideWindow = false;
              break;
            }
          }

          if (insideWindow) {
            strict = true;
          }
        }
      }

      if (strict || ambiguous) {
        continue;
      }

      const FineZeroReason reason = classifyFineZeroRecord(record, minFine, maxFine);
      addFineZeroReason(next.total, reason);
      addFineZeroReason(next.bins[bin], reason);

      if (next.examples.size() < static_cast<std::size_t>(kMaxFineZeroExamples)) {
        FineZeroExample example {};
        example.source = static_cast<int>(source);
        example.firstStep = record.firstStep;
        example.headingBin = bin;
        example.classification = record.classification;
        example.reason = reason;

        for (int axis = 0; axis < 3; ++axis) {
          example.start[axis] = record.start[axis];
          example.end[axis] = record.end[axis];
        }

        next.examples.push_back(example);
      }
    }
  }

  if (next.total.total() != next.coverage.zeroFaceRecords) {
    return false;
  }

  for (int bin = 0; bin < kDirectionBins; ++bin) {
    if (next.bins[bin].total() != next.coverage.bins[bin].zeroFaceRecords) {
      return false;
    }
  }

  result = std::move(next);
  return true;
}

} // namespace aipb::explorer::bridge
