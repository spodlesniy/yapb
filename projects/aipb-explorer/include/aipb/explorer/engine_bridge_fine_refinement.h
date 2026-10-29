#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include <aipb/explorer/engine_bridge_fine_targets.h>

namespace aipb::explorer::bridge {

inline constexpr double kFineRefinementSize = 8.0;
inline constexpr int kFineRefinementStationary = -1;
inline constexpr int kFineRefinementUp = 8;
inline constexpr int kFineRefinementDown = 9;
inline constexpr int kMaxFineRefinementPrinted = 16;

struct FineRefinementGroup {
  SpatialCell startSubcell {};
  int headingBin {};
  int travelBin {};
  int windows {};
  int progressWindows {};
  int restrictionWindows {};
  int sources {};
  FineZeroCounts reasons {};
  int firstSource {};
  int firstStep {};
  double firstStart[3] {};
  double firstDelta[3] {};
  std::set<int> sourceIds {};

  bool mixedOutcomes() const {
    return progressWindows > 0 && restrictionWindows > 0;
  }
};

struct FineRefinementAudit {
  FineTargetAudit targets {};
  int groups {};
  int groupedWindows {};
  int distinctSources {};
  int mixedGroups {};
  int stationaryGroups {};
  int multipleSourceGroups {};
  std::vector<FineRefinementGroup> details {};
};

// Requested yaw and observed endpoint displacement are distinct dimensions.
// A vertical dominant delta is reported separately from horizontal sectors.
inline int fineRefinementTravelBin(const FineZeroExample &record) {
  const double dx = record.end[0] - record.start[0];
  const double dy = record.end[1] - record.start[1];
  const double dz = record.end[2] - record.start[2];

  if (dx == 0.0 && dy == 0.0 && dz == 0.0) {
    return kFineRefinementStationary;
  }

  if (std::abs(dz) > std::hypot(dx, dy)) {
    return dz > 0.0 ? kFineRefinementUp : kFineRefinementDown;
  }

  const double heading = std::atan2(dy, dx) * 57.295779513082320876;
  return directionBin(heading);
}

inline SpatialCell fineRefinementStartSubcell(const FineZeroExample &record) {
  return {
    static_cast<int>(std::floor(record.start[0] / kFineRefinementSize)),
    static_cast<int>(std::floor(record.start[1] / kFineRefinementSize)),
    static_cast<int>(std::floor(record.start[2] / kFineRefinementSize))
  };
}

// Regroup every D247 zero-face record by observed 8-unit start subcell,
// requested-yaw bin and actual travel bin, without making any route claim.
inline bool auditFineRefinement(
  const EvidenceTrialSet &sources,
  double x,
  double y,
  double z,
  FineRefinementAudit &result
) {
  FineRefinementAudit next {};

  if (!auditFineTargets(sources, x, y, z, next.targets)) {
    return false;
  }

  using Key = std::tuple<SpatialCell, int, int>;
  std::map<Key, FineRefinementGroup> groups {};
  std::set<int> allSources {};
  int perHeading[kDirectionBins] {};

  for (const FineZeroExample &record : next.targets.zeros.records) {
    const SpatialCell start = fineRefinementStartSubcell(record);
    const int travel = fineRefinementTravelBin(record);
    const Key key { start, record.headingBin, travel };
    auto [iterator, inserted] = groups.try_emplace(key);
    FineRefinementGroup &group = iterator->second;

    if (inserted) {
      group.startSubcell = start;
      group.headingBin = record.headingBin;
      group.travelBin = travel;
      group.firstSource = record.source;
      group.firstStep = record.firstStep;

      for (int axis = 0; axis < 3; ++axis) {
        group.firstStart[axis] = record.start[axis];
        group.firstDelta[axis] = record.end[axis] - record.start[axis];
      }
    }

    ++group.windows;
    ++perHeading[record.headingBin];
    allSources.insert(record.source);
    group.sourceIds.insert(record.source);

    if (record.classification == MovementClassification::Progress) {
      ++group.progressWindows;
    }
    else {
      ++group.restrictionWindows;
    }

    addFineZeroReason(group.reasons, record.reason);
  }

  for (auto &entry : groups) {
    FineRefinementGroup &group = entry.second;
    group.sources = static_cast<int>(group.sourceIds.size());

    if (group.reasons.total() != group.windows
      || group.progressWindows + group.restrictionWindows != group.windows) {
      return false;
    }

    next.groupedWindows += group.windows;

    if (group.mixedOutcomes()) {
      ++next.mixedGroups;
    }

    if (group.travelBin == kFineRefinementStationary) {
      ++next.stationaryGroups;
    }

    if (group.sources > 1) {
      ++next.multipleSourceGroups;
    }

    next.details.push_back(std::move(group));
  }

  next.groups = static_cast<int>(next.details.size());
  next.distinctSources = static_cast<int>(allSources.size());

  if (next.groupedWindows != next.targets.groupedWindows) {
    return false;
  }

  for (int bin = 0; bin < kDirectionBins; ++bin) {
    if (perHeading[bin] != next.targets.zeros.coverage.bins[bin].zeroFaceRecords) {
      return false;
    }
  }

  // Mixed, directional evidence is useful for supervised review; repeated
  // stationary windows alone do not outrank other observed motion.
  std::sort(next.details.begin(), next.details.end(),
    [](const FineRefinementGroup &a, const FineRefinementGroup &b) {
      if (a.mixedOutcomes() != b.mixedOutcomes()) {
        return a.mixedOutcomes();
      }

      if ((a.travelBin == kFineRefinementStationary)
        != (b.travelBin == kFineRefinementStationary)) {
        return a.travelBin != kFineRefinementStationary;
      }

      if (a.sources != b.sources) {
        return a.sources < b.sources;
      }

      if (a.restrictionWindows != b.restrictionWindows) {
        return a.restrictionWindows > b.restrictionWindows;
      }

      if (a.startSubcell < b.startSubcell) {
        return true;
      }

      if (b.startSubcell < a.startSubcell) {
        return false;
      }

      if (a.headingBin != b.headingBin) {
        return a.headingBin < b.headingBin;
      }

      return a.travelBin < b.travelBin;
    });

  result = std::move(next);
  return true;
}

} // namespace aipb::explorer::bridge
