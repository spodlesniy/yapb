#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include <aipb/explorer/engine_bridge_observation_collection.h>

namespace aipb::explorer::bridge {

inline constexpr char kSpatialIndexVersion[] = "aipb-explorer-spatial-index-v1";
inline constexpr int kSpatialCellSize = 128;
inline constexpr int kMaxSpatialPostings = 131072;

struct SpatialCell {
  int x {};
  int y {};
  int z {};
};

inline bool operator<(const SpatialCell &a, const SpatialCell &b) {
  if (a.x != b.x) {
    return a.x < b.x;
  }

  if (a.y != b.y) {
    return a.y < b.y;
  }

  return a.z < b.z;
}

struct SpatialPosting {
  SpatialCell cell {};
  int record {};
};

inline bool operator<(const SpatialPosting &a, const SpatialPosting &b) {
  if (a.cell < b.cell) {
    return true;
  }

  if (b.cell < a.cell) {
    return false;
  }

  return a.record < b.record;
}

struct SpatialRecord {
  std::string canonical {};
  double start[3] {};
  double end[3] {};
  int firstStep {};
  MovementClassification classification { MovementClassification::InsufficientEvidence };
};

struct SpatialIndex {
  std::vector<SpatialRecord> records {};
  std::vector<SpatialPosting> postings {};
  int cells {};
};

struct SpatialIndexReport {
  int records {};
  int cells {};
  int postings {};
};

struct SpatialQueryResult {
  int matches {};
  int nearestRecord { -1 };
  double nearestDistance {};
};

inline bool spatialCoordinate(double coordinate) {
  return std::isfinite(coordinate) && std::abs(coordinate) <= 1000000.0;
}

inline int spatialCell(double coordinate) {
  return static_cast<int>(std::floor(coordinate / kSpatialCellSize));
}

inline bool parseSpatialRecord(const char *line, SpatialRecord &record) {
  if (!line || std::strlen(line) >= 1024) {
    return false;
  }

  char input[1024] {};
  char normalized[768] {};
  std::memcpy(input, line, std::strlen(line) + 1);

  if (!normalizeObservationRow(input, normalized, sizeof(normalized))) {
    return false;
  }

  char fieldsBuffer[768] {};
  std::memcpy(fieldsBuffer, normalized, std::strlen(normalized) + 1);
  char *fields[18] {};

  if (!splitCollectionFields(fieldsBuffer, fields, 18)
    || !collectionInteger(fields[0], 0, ProbeBudget::kMaxSafetyFrames - 1,
      record.firstStep)) {
    return false;
  }

  for (int i = 0; i < 3; ++i) {
    if (!collectionDouble(fields[7 + i], record.start[i])
      || !collectionDouble(fields[10 + i], record.end[i])
      || !spatialCoordinate(record.start[i])
      || !spatialCoordinate(record.end[i])) {
      return false;
    }
  }

  if (std::strcmp(fields[3], "Progress") == 0) {
    record.classification = MovementClassification::Progress;
  }
  else if (std::strcmp(fields[3], "BlockedCandidate") == 0) {
    record.classification = MovementClassification::BlockedCandidate;
  }
  else if (std::strcmp(fields[3], "DeflectedCandidate") == 0) {
    record.classification = MovementClassification::DeflectedCandidate;
  }
  else {
    return false;
  }

  record.canonical = normalized;
  return true;
}

inline bool buildSpatialIndex(const std::vector<std::string> &rows, SpatialIndex &result) {
  if (rows.empty() || rows.size() > kMaxCollectionRows) {
    return false;
  }

  SpatialIndex next {};

  for (const std::string &row : rows) {
    SpatialRecord record {};

    if (!parseSpatialRecord(row.c_str(), record)) {
      return false;
    }

    const int id = static_cast<int>(next.records.size());
    int lower[3] {};
    int upper[3] {};
    int count = 1;

    for (int axis = 0; axis < 3; ++axis) {
      lower[axis] = spatialCell(std::min(record.start[axis], record.end[axis]));
      upper[axis] = spatialCell(std::max(record.start[axis], record.end[axis]));
      const int span = upper[axis] - lower[axis] + 1;

      if (span < 1 || span > 64 / count) {
        return false;
      }

      count *= span;
    }

    if (next.postings.size() + static_cast<std::size_t>(count) > kMaxSpatialPostings) {
      return false;
    }

    next.records.push_back(record);

    for (int x = lower[0]; x <= upper[0]; ++x) {
      for (int y = lower[1]; y <= upper[1]; ++y) {
        for (int z = lower[2]; z <= upper[2]; ++z) {
          next.postings.push_back({ { x, y, z }, id });
        }
      }
    }
  }

  std::sort(next.postings.begin(), next.postings.end());

  for (std::size_t i = 0; i < next.postings.size(); ++i) {
    if (i == 0 || next.postings[i - 1].cell < next.postings[i].cell) {
      ++next.cells;
    }
  }

  result = std::move(next);
  return true;
}

inline bool writeSpatialIndex(
  const char *sourcePath,
  const char *destinationPath,
  const char *map,
  SpatialIndexReport &report
) {
  if (!sourcePath || !destinationPath || !*sourcePath || !*destinationPath
    || std::strcmp(sourcePath, destinationPath) == 0 || !safeName(map, 63)) {
    return false;
  }

  CollectionReadState source {};

  if (!readObservationCollection(sourcePath, map, source)) {
    return false;
  }

  SpatialIndex index {};

  if (!buildSpatialIndex(source.rows, index)) {
    return false;
  }

  std::FILE *file = openTraceFile(destinationPath, "wx");

  if (!file) {
    return false;
  }

  bool ok = std::fprintf(file, "%s,%s,%d,%zu,%zu\n",
    kSpatialIndexVersion, map, kSpatialCellSize,
    index.records.size(), index.postings.size()) > 0;

  for (std::size_t i = 0; ok && i < index.records.size(); ++i) {
    ok = std::fprintf(file, "R,%zu,%s", i, index.records[i].canonical.c_str()) > 0;
  }

  for (const SpatialPosting &posting : index.postings) {
    if (!ok) {
      break;
    }

    ok = std::fprintf(file, "C,%d,%d,%d,%d\n",
      posting.cell.x, posting.cell.y, posting.cell.z, posting.record) > 0;
  }

  if (std::fclose(file) != 0) {
    ok = false;
  }

  if (!ok) {
    std::remove(destinationPath);
    return false;
  }

  report = { static_cast<int>(index.records.size()), index.cells,
    static_cast<int>(index.postings.size()) };
  return true;
}

inline bool readSpatialIndex(
  const char *path,
  const char *map,
  SpatialIndex &result
) {
  if (!path || !*path || !safeName(map, 63)) {
    return false;
  }

  std::FILE *file = openTraceFile(path, "rt");

  if (!file) {
    return false;
  }

  bool ok = true;
  int recordCount = 0;
  int postingCount = 0;
  char line[1024] {};
  SpatialIndex index {};

  if (!std::fgets(line, sizeof(line), file)) {
    ok = false;
  }

  if (ok) {
    char *fields[5] {};
    int cellSize = 0;

    ok = splitCollectionFields(line, fields, 5)
      && std::strcmp(fields[0], kSpatialIndexVersion) == 0
      && std::strcmp(fields[1], map) == 0
      && collectionInteger(fields[2], kSpatialCellSize, kSpatialCellSize, cellSize)
      && collectionInteger(fields[3], 1, kMaxCollectionRows, recordCount)
      && collectionInteger(fields[4], 1, kMaxSpatialPostings, postingCount);
  }

  for (int id = 0; ok && id < recordCount; ++id) {
    if (!std::fgets(line, sizeof(line), file)
      || line[0] != 'R' || line[1] != ',') {
      ok = false;
      break;
    }

    char *comma = std::strchr(line + 2, ',');
    int parsedId = -1;

    if (!comma) {
      ok = false;
      break;
    }

    *comma = '\0';

    if (!collectionInteger(line + 2, id, id, parsedId)) {
      ok = false;
      break;
    }

    SpatialRecord record {};

    if (!parseSpatialRecord(comma + 1, record)
      || record.canonical != comma + 1) {
      ok = false;
      break;
    }

    index.records.push_back(std::move(record));
  }

  if (ok) {
    std::vector<std::string> rows {};
    rows.reserve(index.records.size());

    for (const SpatialRecord &record : index.records) {
      rows.push_back(record.canonical);
    }

    ok = buildSpatialIndex(rows, index)
      && index.postings.size() == static_cast<std::size_t>(postingCount);
  }

  for (int i = 0; ok && i < postingCount; ++i) {
    if (!std::fgets(line, sizeof(line), file)) {
      ok = false;
      break;
    }

    char *fields[5] {};
    int marker = -1;
    int x = 0;
    int y = 0;
    int z = 0;
    int record = -1;

    if (!splitCollectionFields(line, fields, 5)
      || std::strcmp(fields[0], "C") != 0
      || !collectionInteger(fields[1], -7813, 7812, x)
      || !collectionInteger(fields[2], -7813, 7812, y)
      || !collectionInteger(fields[3], -7813, 7812, z)
      || !collectionInteger(fields[4], 0, recordCount - 1, record)) {
      ok = false;
      break;
    }

    const SpatialPosting &expected = index.postings[static_cast<std::size_t>(i)];

    if (x != expected.cell.x || y != expected.cell.y
      || z != expected.cell.z || record != expected.record) {
      ok = false;
    }

    (void)marker;
  }

  if (ok && (std::fgets(line, sizeof(line), file) || std::ferror(file))) {
    ok = false;
  }

  if (std::fclose(file) != 0) {
    ok = false;
  }

  if (!ok) {
    return false;
  }

  result = std::move(index);
  return true;
}

inline bool searchSpatialIndex(
  const SpatialIndex &index,
  double x,
  double y,
  double z,
  double radius,
  SpatialQueryResult &result
) {
  if (index.records.empty() || !spatialCoordinate(x) || !spatialCoordinate(y)
    || !spatialCoordinate(z) || !std::isfinite(radius)
    || radius < 1.0 || radius > 512.0) {
    return false;
  }

  SpatialQueryResult next {};
  double nearestSquared = radius * radius;
  std::set<int> checked {};
  const int lower[3] { spatialCell(x - radius), spatialCell(y - radius),
    spatialCell(z - radius) };
  const int upper[3] { spatialCell(x + radius), spatialCell(y + radius),
    spatialCell(z + radius) };
  const double origin[3] { x, y, z };

  for (int cx = lower[0]; cx <= upper[0]; ++cx) {
    for (int cy = lower[1]; cy <= upper[1]; ++cy) {
      for (int cz = lower[2]; cz <= upper[2]; ++cz) {
        const SpatialPosting key { { cx, cy, cz }, -1 };
        auto it = std::lower_bound(index.postings.begin(), index.postings.end(), key);

        for (; it != index.postings.end()
          && it->cell.x == cx && it->cell.y == cy && it->cell.z == cz; ++it) {
          if (!checked.insert(it->record).second) {
            continue;
          }

          const SpatialRecord &record = index.records[static_cast<std::size_t>(it->record)];
          double lengthSquared = 0.0;
          double dot = 0.0;

          for (int axis = 0; axis < 3; ++axis) {
            const double delta = record.end[axis] - record.start[axis];
            lengthSquared += delta * delta;
            dot += (origin[axis] - record.start[axis]) * delta;
          }

          const double t = lengthSquared > 0.0
            ? std::max(0.0, std::min(1.0, dot / lengthSquared)) : 0.0;
          double distanceSquared = 0.0;

          for (int axis = 0; axis < 3; ++axis) {
            const double nearest = record.start[axis]
              + t * (record.end[axis] - record.start[axis]);
            const double delta = origin[axis] - nearest;
            distanceSquared += delta * delta;
          }

          if (distanceSquared <= radius * radius) {
            ++next.matches;

            if (next.nearestRecord < 0 || distanceSquared < nearestSquared
              || (distanceSquared == nearestSquared && it->record < next.nearestRecord)) {
              next.nearestRecord = it->record;
              nearestSquared = distanceSquared;
            }
          }
        }
      }
    }
  }

  if (next.nearestRecord >= 0) {
    next.nearestDistance = std::sqrt(nearestSquared);
  }

  result = next;
  return true;
}

} // namespace aipb::explorer::bridge
