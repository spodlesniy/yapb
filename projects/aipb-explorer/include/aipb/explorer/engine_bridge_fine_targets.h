#pragma once

#include <algorithm>
#include <map>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include <aipb/explorer/engine_bridge_fine_zero_reasons.h>

namespace aipb::explorer::bridge {

inline constexpr int kMaxFineTargetsPrinted = 16;

struct FineTarget {
  SpatialCell cell {};
  int headingBin {};
  int windows {};
  int stationary {};
  int boundaryContact {};
  int sameCell {};
  int outside {};
  int other {};
  int progress {};
  int restriction {};
  int sources {};
  double start[3] {};
  int firstSource {};
  int firstStep {};
  std::set<int> sourceIds {};
};

struct FineTargetAudit {
  FineZeroAudit zeros {};
  int groups {};
  int groupedWindows {};
  int distinctSources {};
  std::vector<FineTarget> targets {};
};

// Candidate observations are actual recorded starting coordinates, not
// commands to move a bot or claims that the position is traversable.
inline bool auditFineTargets(
  const EvidenceTrialSet &sources,
  double x,
  double y,
  double z,
  FineTargetAudit &result
) {
  FineTargetAudit next {};

  if (!auditFineZeroReasons(sources, x, y, z, next.zeros)) {
    return false;
  }

  using Key = std::pair<SpatialCell, int>;
  std::map<Key, FineTarget> grouped {};
  std::set<int> allSources {};

  for (const FineZeroExample &record : next.zeros.records) {
    const SpatialCell cell {
      localizedCell(record.start[0]),
      localizedCell(record.start[1]),
      localizedCell(record.start[2])
    };
    const Key key { cell, record.headingBin };
    auto [it, inserted] = grouped.try_emplace(key);
    FineTarget &target = it->second;

    if (inserted) {
      target.cell = cell;
      target.headingBin = record.headingBin;
      target.firstSource = record.source;
      target.firstStep = record.firstStep;

      for (int axis = 0; axis < 3; ++axis) {
        target.start[axis] = record.start[axis];
      }
    }

    ++target.windows;
    target.sourceIds.insert(record.source);
    allSources.insert(record.source);

    if (record.classification == MovementClassification::Progress) {
      ++target.progress;
    }
    else {
      ++target.restriction;
    }

    switch (record.reason) {
    case FineZeroReason::Stationary:
      ++target.stationary;
      break;
    case FineZeroReason::BoundaryContact:
      ++target.boundaryContact;
      break;
    case FineZeroReason::SameFineCell:
      ++target.sameCell;
      break;
    case FineZeroReason::OutsideWindow:
      ++target.outside;
      break;
    case FineZeroReason::Other:
      ++target.other;
      break;
    }
  }

  for (auto &entry : grouped) {
    FineTarget &target = entry.second;
    target.sources = static_cast<int>(target.sourceIds.size());
    next.groupedWindows += target.windows;
    next.targets.push_back(std::move(target));
  }

  next.groups = static_cast<int>(next.targets.size());
  next.distinctSources = static_cast<int>(allSources.size());

  if (next.groupedWindows != next.zeros.coverage.zeroFaceRecords) {
    return false;
  }

  // Prioritize missing independent observations before repeated windows.
  std::sort(next.targets.begin(), next.targets.end(),
    [](const FineTarget &a, const FineTarget &b) {
      if (a.sources != b.sources) {
        return a.sources < b.sources;
      }

      if (a.restriction != b.restriction) {
        return a.restriction > b.restriction;
      }

      if (a.windows != b.windows) {
        return a.windows > b.windows;
      }

      if (a.cell < b.cell) {
        return true;
      }

      if (b.cell < a.cell) {
        return false;
      }

      return a.headingBin < b.headingBin;
    });

  result = std::move(next);
  return true;
}

} // namespace aipb::explorer::bridge
