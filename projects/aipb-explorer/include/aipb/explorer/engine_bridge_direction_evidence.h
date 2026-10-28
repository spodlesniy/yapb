#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

#include <aipb/explorer/engine_bridge_spatial_index.h>

namespace aipb::explorer::bridge {

inline constexpr char kDirectionEvidenceVersion[] = "aipb-explorer-direction-evidence-v1";
inline constexpr char kDirectionEvidenceColumns[] =
  "cell_x,cell_y,cell_z,heading_bin,observations,progress,blocked_candidates,deflected_candidates\n";
inline constexpr int kDirectionBins = 8;
inline constexpr double kDirectionBinDegrees = 360.0 / kDirectionBins;

inline int directionBin(double headingDegrees) {
  const double normalized = std::fmod(headingDegrees, 360.0);
  const int bin = static_cast<int>(std::floor(
    (normalized + kDirectionBinDegrees * 0.5) / kDirectionBinDegrees));
  return (bin % kDirectionBins + kDirectionBins) % kDirectionBins;
}

struct DirectionEvidence {
  SpatialCell cell {};
  int bin {};
  int observations {};
  int progress {};
  int blockedCandidates {};
  int deflectedCandidates {};
};

inline bool directionEvidenceLess(const DirectionEvidence &a, const DirectionEvidence &b) {
  if (a.cell < b.cell) {
    return true;
  }

  if (b.cell < a.cell) {
    return false;
  }

  return a.bin < b.bin;
}

inline bool directionConflict(const DirectionEvidence &evidence) {
  return evidence.progress > 0
    && (evidence.blockedCandidates > 0 || evidence.deflectedCandidates > 0);
}

struct DirectionStatistics {
  int sourceRecords {};
  int sourcePostings {};
  std::vector<DirectionEvidence> entries {};
};

struct DirectionStatisticsReport {
  int cells {};
  int bins {};
  int observations {};
  int progress {};
  int blockedCandidates {};
  int deflectedCandidates {};
  int conflictedBins {};
  int crossHeadingMixedCells {};
};

struct DirectionQuery {
  DirectionEvidence evidence {};
  int headingsInCell {};
  bool sameHeadingConflict {};
  bool otherHeadingOpposition {};
};

// The heading is the requested command heading, not the observed displacement.
inline bool spatialRecordHeading(const SpatialRecord &record, double &heading) {
  if (record.canonical.size() >= 768) {
    return false;
  }

  char row[768] {};
  std::memcpy(row, record.canonical.c_str(), record.canonical.size() + 1);
  char *fields[18] {};

  return splitCollectionFields(row, fields, 18)
    && collectionDouble(fields[13], heading);
}

inline bool aggregateDirectionalEvidence(
  const SpatialIndex &source,
  DirectionStatistics &result
) {
  if (source.records.empty() || source.records.size() > kMaxCollectionRows
    || source.postings.empty() || source.postings.size() > kMaxSpatialPostings) {
    return false;
  }

  std::vector<int> bins {};
  bins.reserve(source.records.size());

  for (const SpatialRecord &record : source.records) {
    double heading {};

    if (!spatialRecordHeading(record, heading)) {
      return false;
    }

    bins.push_back(directionBin(heading));
  }

  DirectionStatistics next {};
  next.sourceRecords = static_cast<int>(source.records.size());
  next.sourcePostings = static_cast<int>(source.postings.size());

  std::size_t offset = 0;
  SpatialPosting previous {};
  bool hasPrevious = false;

  while (offset < source.postings.size()) {
    const SpatialCell cell = source.postings[offset].cell;
    DirectionEvidence counts[kDirectionBins] {};

    for (int bin = 0; bin < kDirectionBins; ++bin) {
      counts[bin].cell = cell;
      counts[bin].bin = bin;
    }

    while (offset < source.postings.size()
      && !(cell < source.postings[offset].cell)
      && !(source.postings[offset].cell < cell)) {
      const SpatialPosting &posting = source.postings[offset];

      if (posting.record < 0 || posting.record >= next.sourceRecords
        || (hasPrevious && !(previous < posting))) {
        return false;
      }

      DirectionEvidence &entry = counts[bins[static_cast<std::size_t>(posting.record)]];
      ++entry.observations;

      switch (source.records[static_cast<std::size_t>(posting.record)].classification) {
      case MovementClassification::Progress:
        ++entry.progress;
        break;
      case MovementClassification::BlockedCandidate:
        ++entry.blockedCandidates;
        break;
      case MovementClassification::DeflectedCandidate:
        ++entry.deflectedCandidates;
        break;
      case MovementClassification::InsufficientEvidence:
        return false;
      }

      previous = posting;
      hasPrevious = true;
      ++offset;
    }

    for (const DirectionEvidence &entry : counts) {
      if (entry.observations > 0) {
        next.entries.push_back(entry);
      }
    }
  }

  result = std::move(next);
  return true;
}

inline DirectionStatisticsReport summarizeDirectionalEvidence(
  const DirectionStatistics &statistics
) {
  DirectionStatisticsReport report {};
  report.bins = static_cast<int>(statistics.entries.size());
  SpatialCell prior {};
  bool hasPrior = false;
  unsigned progressMask = 0;
  unsigned restrictionMask = 0;

  const auto finishCell = [&]() {
    for (int progressBin = 0; progressBin < kDirectionBins; ++progressBin) {
      if ((progressMask & (1u << progressBin)) == 0) {
        continue;
      }

      const unsigned otherBins = restrictionMask & ~(1u << progressBin);

      if (otherBins != 0) {
        ++report.crossHeadingMixedCells;
        break;
      }
    }
  };

  for (const DirectionEvidence &entry : statistics.entries) {
    if (!hasPrior || prior < entry.cell) {
      if (hasPrior) {
        finishCell();
      }

      prior = entry.cell;
      hasPrior = true;
      progressMask = 0;
      restrictionMask = 0;
      ++report.cells;
    }

    report.observations += entry.observations;
    report.progress += entry.progress;
    report.blockedCandidates += entry.blockedCandidates;
    report.deflectedCandidates += entry.deflectedCandidates;

    if (directionConflict(entry)) {
      ++report.conflictedBins;
    }

    if (entry.progress > 0) {
      progressMask |= (1u << entry.bin);
    }

    if (entry.blockedCandidates > 0 || entry.deflectedCandidates > 0) {
      restrictionMask |= (1u << entry.bin);
    }
  }

  if (hasPrior) {
    finishCell();
  }

  return report;
}

inline bool writeDirectionalEvidence(
  const char *indexPath,
  const char *outputPath,
  const char *map,
  DirectionStatisticsReport &report
) {
  if (!indexPath || !*indexPath || !outputPath || !*outputPath
    || !safeName(map, 63) || std::strcmp(indexPath, outputPath) == 0) {
    return false;
  }

  SpatialIndex index {};

  if (!readSpatialIndex(indexPath, map, index)) {
    return false;
  }

  DirectionStatistics statistics {};

  if (!aggregateDirectionalEvidence(index, statistics)) {
    return false;
  }

  std::FILE *file = openTraceFile(outputPath, "wx");

  if (!file) {
    return false;
  }

  bool ok = std::fprintf(file, "%s,%s,%d,%d,%d,%d,%zu\n",
    kDirectionEvidenceVersion, map, kSpatialCellSize, kDirectionBins,
    statistics.sourceRecords, statistics.sourcePostings, statistics.entries.size()) > 0
    && std::fputs(kDirectionEvidenceColumns, file) >= 0;

  for (const DirectionEvidence &entry : statistics.entries) {
    if (!ok) {
      break;
    }

    ok = std::fprintf(file, "%d,%d,%d,%d,%d,%d,%d,%d\n",
      entry.cell.x, entry.cell.y, entry.cell.z, entry.bin, entry.observations,
      entry.progress, entry.blockedCandidates, entry.deflectedCandidates) > 0;
  }

  if (std::fclose(file) != 0) {
    ok = false;
  }

  if (!ok) {
    std::remove(outputPath);
    return false;
  }

  report = summarizeDirectionalEvidence(statistics);
  return true;
}

// Reject truncated, reordered or internally inconsistent snapshot files.
inline bool readDirectionalEvidence(
  const char *path,
  const char *map,
  DirectionStatistics &result
) {
  if (!path || !*path || !safeName(map, 63)) {
    return false;
  }

  std::FILE *file = openTraceFile(path, "rt");

  if (!file) {
    return false;
  }

  DirectionStatistics next {};
  bool ok = true;
  char line[256] {};
  int cellSize = 0;
  int binCount = 0;
  int rowCount = 0;

  if (!std::fgets(line, sizeof(line), file)) {
    ok = false;
  }

  if (ok) {
    char *fields[7] {};
    ok = splitCollectionFields(line, fields, 7)
      && std::strcmp(fields[0], kDirectionEvidenceVersion) == 0
      && std::strcmp(fields[1], map) == 0
      && collectionInteger(fields[2], kSpatialCellSize, kSpatialCellSize, cellSize)
      && collectionInteger(fields[3], kDirectionBins, kDirectionBins, binCount)
      && collectionInteger(fields[4], 1, kMaxCollectionRows, next.sourceRecords)
      && collectionInteger(fields[5], 1, kMaxSpatialPostings, next.sourcePostings)
      && collectionInteger(fields[6], 1, next.sourcePostings, rowCount);
  }

  if (ok && (!std::fgets(line, sizeof(line), file)
    || std::strcmp(line, kDirectionEvidenceColumns) != 0)) {
    ok = false;
  }

  int sum = 0;
  int perCell = 0;

  for (int i = 0; ok && i < rowCount; ++i) {
    if (!std::fgets(line, sizeof(line), file)) {
      ok = false;
      break;
    }

    char *fields[8] {};
    DirectionEvidence entry {};

    if (!splitCollectionFields(line, fields, 8)
      || !collectionInteger(fields[0], -7813, 7812, entry.cell.x)
      || !collectionInteger(fields[1], -7813, 7812, entry.cell.y)
      || !collectionInteger(fields[2], -7813, 7812, entry.cell.z)
      || !collectionInteger(fields[3], 0, kDirectionBins - 1, entry.bin)
      || !collectionInteger(fields[4], 1, next.sourceRecords, entry.observations)
      || !collectionInteger(fields[5], 0, entry.observations, entry.progress)
      || !collectionInteger(fields[6], 0, entry.observations, entry.blockedCandidates)
      || !collectionInteger(fields[7], 0, entry.observations, entry.deflectedCandidates)
      || entry.progress + entry.blockedCandidates + entry.deflectedCandidates
        != entry.observations
      || (!next.entries.empty() && !directionEvidenceLess(next.entries.back(), entry))) {
      ok = false;
      break;
    }

    if (next.entries.empty() || next.entries.back().cell < entry.cell) {
      perCell = 0;
    }

    perCell += entry.observations;
    sum += entry.observations;

    if (perCell > next.sourceRecords || sum > next.sourcePostings) {
      ok = false;
      break;
    }

    next.entries.push_back(entry);
  }

  if (ok && (sum != next.sourcePostings
    || std::fgets(line, sizeof(line), file) || std::ferror(file))) {
    ok = false;
  }

  if (std::fclose(file) != 0) {
    ok = false;
  }

  if (!ok) {
    return false;
  }

  result = std::move(next);
  return true;
}

inline bool queryDirectionalEvidence(
  const DirectionStatistics &statistics,
  double x,
  double y,
  double z,
  double heading,
  DirectionQuery &result
) {
  if (!spatialCoordinate(x) || !spatialCoordinate(y) || !spatialCoordinate(z)
    || !std::isfinite(heading)) {
    return false;
  }

  DirectionQuery next {};
  next.evidence.cell = { spatialCell(x), spatialCell(y), spatialCell(z) };
  next.evidence.bin = directionBin(heading);
  const DirectionEvidence key = next.evidence;
  const auto first = std::lower_bound(statistics.entries.begin(), statistics.entries.end(),
    key, [](const DirectionEvidence &entry, const DirectionEvidence &query) {
      return entry.cell < query.cell;
    });

  bool otherProgress = false;
  bool otherRestriction = false;

  for (auto it = first; it != statistics.entries.end()
    && !(next.evidence.cell < it->cell); ++it) {
    ++next.headingsInCell;

    if (it->bin == next.evidence.bin) {
      next.evidence = *it;
    }
    else {
      otherProgress = otherProgress || it->progress > 0;
      otherRestriction = otherRestriction
        || it->blockedCandidates > 0 || it->deflectedCandidates > 0;
    }
  }

  next.sameHeadingConflict = directionConflict(next.evidence);
  next.otherHeadingOpposition =
    (next.evidence.progress > 0 && otherRestriction)
    || ((next.evidence.blockedCandidates > 0
      || next.evidence.deflectedCandidates > 0) && otherProgress);
  result = next;
  return true;
}

} // namespace aipb::explorer::bridge
