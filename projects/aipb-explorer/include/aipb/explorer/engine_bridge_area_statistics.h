#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

#include <aipb/explorer/engine_bridge_spatial_index.h>

namespace aipb::explorer::bridge {

inline constexpr char kAreaStatisticsVersion[] = "aipb-explorer-area-statistics-v1";
inline constexpr char kAreaStatisticsColumns[] =
  "cell_x,cell_y,cell_z,observations,progress,blocked_candidates,deflected_candidates\n";

struct AreaEvidence {
  SpatialCell cell {};
  int observations {};
  int progress {};
  int blockedCandidates {};
  int deflectedCandidates {};
};

struct AreaStatistics {
  int sourceRecords {};
  int sourcePostings {};
  std::vector<AreaEvidence> areas {};
};

struct AreaStatisticsReport {
  int cells {};
  int observations {};
  int progress {};
  int blockedCandidates {};
  int deflectedCandidates {};
};

// Each unique (cell, record) posting contributes one piece of evidence in that cell.
inline bool aggregateSpatialEvidence(const SpatialIndex &source, AreaStatistics &result) {
  if (source.records.empty() || source.records.size() > kMaxCollectionRows
    || source.postings.empty() || source.postings.size() > kMaxSpatialPostings) {
    return false;
  }

  AreaStatistics next {};
  next.sourceRecords = static_cast<int>(source.records.size());
  next.sourcePostings = static_cast<int>(source.postings.size());
  SpatialPosting previous {};
  bool hasPrevious = false;

  for (const SpatialPosting &posting : source.postings) {
    if (posting.record < 0 || posting.record >= next.sourceRecords
      || (hasPrevious && !(previous < posting))) {
      return false;
    }

    if (next.areas.empty() || next.areas.back().cell < posting.cell) {
      next.areas.push_back({});
      next.areas.back().cell = posting.cell;
    }

    AreaEvidence &area = next.areas.back();
    ++area.observations;

    switch (source.records[static_cast<std::size_t>(posting.record)].classification) {
    case MovementClassification::Progress:
      ++area.progress;
      break;
    case MovementClassification::BlockedCandidate:
      ++area.blockedCandidates;
      break;
    case MovementClassification::DeflectedCandidate:
      ++area.deflectedCandidates;
      break;
    case MovementClassification::InsufficientEvidence:
      return false;
    }

    previous = posting;
    hasPrevious = true;
  }

  if (next.areas.size() != static_cast<std::size_t>(source.cells)) {
    return false;
  }

  result = std::move(next);
  return true;
}

inline AreaStatisticsReport summarizeAreaStatistics(const AreaStatistics &statistics) {
  AreaStatisticsReport result {};
  result.cells = static_cast<int>(statistics.areas.size());

  for (const AreaEvidence &area : statistics.areas) {
    result.observations += area.observations;
    result.progress += area.progress;
    result.blockedCandidates += area.blockedCandidates;
    result.deflectedCandidates += area.deflectedCandidates;
  }

  return result;
}

// Persist a new immutable, map-scoped snapshot; never modify the spatial index.
inline bool writeAreaStatistics(
  const char *indexPath,
  const char *outputPath,
  const char *map,
  AreaStatisticsReport &report
) {
  if (!indexPath || !*indexPath || !outputPath || !*outputPath
    || !safeName(map, 63) || std::strcmp(indexPath, outputPath) == 0) {
    return false;
  }

  SpatialIndex index {};

  if (!readSpatialIndex(indexPath, map, index)) {
    return false;
  }

  AreaStatistics statistics {};

  if (!aggregateSpatialEvidence(index, statistics)) {
    return false;
  }

  std::FILE *file = openTraceFile(outputPath, "wx");

  if (!file) {
    return false;
  }

  bool ok = std::fprintf(file, "%s,%s,%d,%d,%d,%zu\n",
    kAreaStatisticsVersion, map, kSpatialCellSize, statistics.sourceRecords,
    statistics.sourcePostings, statistics.areas.size()) > 0
    && std::fputs(kAreaStatisticsColumns, file) >= 0;

  for (const AreaEvidence &area : statistics.areas) {
    if (!ok) {
      break;
    }

    ok = std::fprintf(file, "%d,%d,%d,%d,%d,%d,%d\n",
      area.cell.x, area.cell.y, area.cell.z, area.observations, area.progress,
      area.blockedCandidates, area.deflectedCandidates) > 0;
  }

  if (std::fclose(file) != 0) {
    ok = false;
  }

  if (!ok) {
    std::remove(outputPath);
    return false;
  }

  report = summarizeAreaStatistics(statistics);
  return true;
}

// Validate every record and all declared totals before returning the result.
inline bool readAreaStatistics(const char *path, const char *map, AreaStatistics &result) {
  if (!path || !*path || !safeName(map, 63)) {
    return false;
  }

  std::FILE *file = openTraceFile(path, "rt");

  if (!file) {
    return false;
  }

  bool ok = true;
  char line[256] {};
  int cellCount = 0;
  int cellSize = 0;
  AreaStatistics next {};

  if (!std::fgets(line, sizeof(line), file)) {
    ok = false;
  }

  if (ok) {
    char *fields[6] {};
    ok = splitCollectionFields(line, fields, 6)
      && std::strcmp(fields[0], kAreaStatisticsVersion) == 0
      && std::strcmp(fields[1], map) == 0
      && collectionInteger(fields[2], kSpatialCellSize, kSpatialCellSize, cellSize)
      && collectionInteger(fields[3], 1, kMaxCollectionRows, next.sourceRecords)
      && collectionInteger(fields[4], 1, kMaxSpatialPostings, next.sourcePostings)
      && collectionInteger(fields[5], 1, next.sourcePostings, cellCount);
  }

  if (ok && (!std::fgets(line, sizeof(line), file)
    || std::strcmp(line, kAreaStatisticsColumns) != 0)) {
    ok = false;
  }

  int totalPostings = 0;

  for (int i = 0; ok && i < cellCount; ++i) {
    if (!std::fgets(line, sizeof(line), file)) {
      ok = false;
      break;
    }

    char *fields[7] {};
    AreaEvidence area {};

    if (!splitCollectionFields(line, fields, 7)
      || !collectionInteger(fields[0], -7813, 7812, area.cell.x)
      || !collectionInteger(fields[1], -7813, 7812, area.cell.y)
      || !collectionInteger(fields[2], -7813, 7812, area.cell.z)
      || !collectionInteger(fields[3], 1, next.sourceRecords, area.observations)
      || !collectionInteger(fields[4], 0, area.observations, area.progress)
      || !collectionInteger(fields[5], 0, area.observations, area.blockedCandidates)
      || !collectionInteger(fields[6], 0, area.observations, area.deflectedCandidates)
      || area.progress + area.blockedCandidates + area.deflectedCandidates
        != area.observations
      || (!next.areas.empty() && !(next.areas.back().cell < area.cell))) {
      ok = false;
      break;
    }

    totalPostings += area.observations;

    if (totalPostings > next.sourcePostings) {
      ok = false;
      break;
    }

    next.areas.push_back(area);
  }

  if (ok && (totalPostings != next.sourcePostings
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

inline bool queryAreaStatistics(
  const AreaStatistics &statistics,
  double x,
  double y,
  double z,
  AreaEvidence &result
) {
  if (!spatialCoordinate(x) || !spatialCoordinate(y) || !spatialCoordinate(z)) {
    return false;
  }

  const SpatialCell cell { spatialCell(x), spatialCell(y), spatialCell(z) };
  AreaEvidence next {};
  next.cell = cell;
  const auto it = std::lower_bound(statistics.areas.begin(), statistics.areas.end(),
    cell, [](const AreaEvidence &area, const SpatialCell &lookup) {
      return area.cell < lookup;
    });

  if (it != statistics.areas.end() && !(cell < it->cell)) {
    next = *it;
  }

  result = next;
  return true;
}

} // namespace aipb::explorer::bridge
