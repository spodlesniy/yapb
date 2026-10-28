#pragma once

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include <aipb/explorer/engine_bridge_trace_observations.h>

namespace aipb::explorer::bridge {

inline constexpr int kMaxCollectionRows = 16384;
inline constexpr const char kCollectionVersion[] = "aipb-explorer-collection-v1";
inline constexpr const char kObservationVersion[] = "aipb-explorer-observations-v2";

struct ObservationCollectionReport {
  int inputRows {};
  int added {};
  int duplicates {};
  int total {};
};

inline bool splitCollectionFields(char *line, char **fields, int expected) {
  if (!line || expected <= 0) {
    return false;
  }

  const std::size_t length = std::strlen(line);

  if (length == 0 || line[length - 1] != '\n') {
    return false;
  }

  line[length - 1] = '\0';

  if (length > 1 && line[length - 2] == '\r') {
    line[length - 2] = '\0';
  }

  char *next = line;

  for (int i = 0; i < expected; ++i) {
    fields[i] = next;
    char *separator = std::strchr(next, ',');

    if (i < expected - 1) {
      if (!separator || separator == next) {
        return false;
      }

      *separator = '\0';
      next = separator + 1;
    }
    else if (separator || !*next) {
      return false;
    }
  }

  return true;
}

inline bool collectionInteger(const char *text, int minimum, int maximum, int &value) {
  if (!text || !*text) {
    return false;
  }

  char *end {};
  const long parsed = std::strtol(text, &end, 10);

  if (*end || parsed < minimum || parsed > maximum) {
    return false;
  }

  value = static_cast<int>(parsed);
  return true;
}

inline bool collectionDouble(const char *text, double &value) {
  if (!text || !*text) {
    return false;
  }

  char *end {};
  const double parsed = std::strtod(text, &end);

  if (*end || !std::isfinite(parsed)) {
    return false;
  }

  value = parsed;
  return true;
}

// Parse and normalize only the exact verified observation-v2 schema.
inline bool normalizeObservationRow(char *line, char *output, std::size_t outputSize,
  int sourceSteps = 0) {
  char *fields[18] {};

  if (!splitCollectionFields(line, fields, 18)
    || std::strcmp(fields[5], "unknown") != 0) {
    return false;
  }

  TraceWindowEvidence record {};

  if (!collectionInteger(fields[0], 0, ProbeBudget::kMaxSafetyFrames - 1, record.firstStep)
    || !collectionInteger(fields[1], record.firstStep, ProbeBudget::kMaxSafetyFrames - 1, record.lastStep)
    || !collectionInteger(fields[2], 0, record.firstStep, record.stateStartStep)
    || !collectionInteger(fields[6], 100, 255 * ProbeBudget::kMaxSafetyFrames, record.durationMs)
    || (sourceSteps > 0 && record.lastStep >= sourceSteps)) {
    return false;
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

  if (std::strcmp(fields[4], "direct") == 0) {
    record.confirmation = WindowConfirmation::Direct;
  }
  else if (std::strcmp(fields[4], "retrospective") == 0) {
    record.confirmation = WindowConfirmation::Retrospective;
  }
  else if (std::strcmp(fields[4], "confirming") == 0) {
    record.confirmation = WindowConfirmation::Confirming;
  }
  else if (std::strcmp(fields[4], "continuation") == 0) {
    record.confirmation = WindowConfirmation::Continuation;
  }
  else {
    return false;
  }

  double *coordinates[6] { &record.start.x, &record.start.y, &record.start.z,
    &record.end.x, &record.end.y, &record.end.z };

  for (int i = 0; i < 6; ++i) {
    if (!collectionDouble(fields[7 + i], *coordinates[i])) {
      return false;
    }
  }

  if (!collectionDouble(fields[13], record.headingDegrees)
    || !collectionDouble(fields[14], record.requestedUnits)
    || !collectionDouble(fields[15], record.observedUnits)
    || !collectionDouble(fields[16], record.projectedUnits)
    || !collectionDouble(fields[17], record.lateralUnits)) {
    return false;
  }

  return formatObservation(output, outputSize, record);
}

struct CollectionReadState {
  std::vector<std::string> rows {};
  std::set<std::string> unique {};
  ObservationCollectionReport report {};
};

// Both inputs must be complete and valid before an output file is created.
inline bool readObservationCollection(
  const char *path,
  const char *expectedMap,
  CollectionReadState &state
) {
  std::FILE *file = openTraceFile(path, "rt");

  if (!file) {
    return false;
  }

  bool valid = true;
  bool aggregate = false;
  int sourceSteps = 0;
  int declaredRows = 0;
  int rowsRead = 0;
  char line[1024] {};

  if (!std::fgets(line, sizeof(line), file)) {
    valid = false;
  }

  if (valid) {
    char *fields[4] {};
    int fieldCount = 0;

    if (std::strncmp(line, kCollectionVersion, sizeof(kCollectionVersion) - 1) == 0
      && line[sizeof(kCollectionVersion) - 1] == ',') {
      fieldCount = 3;
      aggregate = true;
    }
    else if (std::strncmp(line, kObservationVersion, sizeof(kObservationVersion) - 1) == 0
      && line[sizeof(kObservationVersion) - 1] == ',') {
      fieldCount = 4;
    }
    else {
      valid = false;
    }

    if (valid && (!splitCollectionFields(line, fields, fieldCount)
      || std::strcmp(fields[0], aggregate ? kCollectionVersion : kObservationVersion) != 0
      || std::strcmp(fields[1], expectedMap) != 0)) {
      valid = false;
    }

    if (valid && aggregate) {
      valid = collectionInteger(fields[2], 1, kMaxCollectionRows, declaredRows);
    }
    else if (valid) {
      int botIndex {};
      valid = collectionInteger(fields[2], -1, 31, botIndex)
        && collectionInteger(fields[3], 1, ProbeBudget::kMaxSafetyFrames, sourceSteps);
    }
  }

  if (valid && (!std::fgets(line, sizeof(line), file)
    || std::strcmp(line, kObservationCsvColumns) != 0)) {
    valid = false;
  }

  int previousLastStep = -1;

  while (valid && std::fgets(line, sizeof(line), file)) {
    char canonical[768] {};
    int stepStart = -1;
    int stepEnd = -1;

    // Read the numeric step range before normalization mutates its buffer.
    char copy[1024] {};
    std::memcpy(copy, line, std::strlen(line) + 1);
    char *fields[18] {};

    if (!splitCollectionFields(copy, fields, 18)
      || !collectionInteger(fields[0], 0, ProbeBudget::kMaxSafetyFrames - 1, stepStart)
      || !collectionInteger(fields[1], stepStart, ProbeBudget::kMaxSafetyFrames - 1, stepEnd)
      || (!aggregate && stepStart <= previousLastStep)
      || !normalizeObservationRow(line, canonical, sizeof(canonical), sourceSteps)) {
      valid = false;
      break;
    }

    previousLastStep = stepEnd;
    ++rowsRead;

    if (rowsRead > (aggregate ? declaredRows : ProbeBudget::kMaxSafetyFrames)
      || state.report.inputRows >= 2 * kMaxCollectionRows) {
      valid = false;
      break;
    }

    ++state.report.inputRows;
    const std::string row { canonical };

    if (state.unique.find(row) != state.unique.end()) {
      ++state.report.duplicates;
    }
    else {
      if (state.rows.size() >= static_cast<std::size_t>(kMaxCollectionRows)) {
        valid = false;
        break;
      }

      state.unique.insert(row);
      state.rows.push_back(row);
      ++state.report.added;
    }
  }

  if (std::ferror(file) || rowsRead == 0
    || (aggregate && rowsRead != declaredRows)) {
    valid = false;
  }

  if (std::fclose(file) != 0) {
    valid = false;
  }

  return valid;
}

// Produces a new immutable snapshot; no in-place modification or overwrite.
inline bool mergeObservationCollections(
  const char *firstPath,
  const char *secondPath,
  const char *outputPath,
  const char *map,
  ObservationCollectionReport &report
) {
  if (!firstPath || !*firstPath || !secondPath || !*secondPath
    || !outputPath || !*outputPath || !safeName(map, 63)
    || std::strcmp(firstPath, outputPath) == 0
    || std::strcmp(secondPath, outputPath) == 0) {
    return false;
  }

  CollectionReadState state {};

  if (!readObservationCollection(firstPath, map, state)
    || !readObservationCollection(secondPath, map, state)
    || state.rows.empty()) {
    return false;
  }

  std::FILE *output = openTraceFile(outputPath, "wx");

  if (!output) {
    return false;
  }

  bool valid = std::fprintf(output, "%s,%s,%zu\n",
    kCollectionVersion, map, state.rows.size()) > 0
    && std::fputs(kObservationCsvColumns, output) >= 0;

  for (const auto &row : state.rows) {
    if (!valid || std::fputs(row.c_str(), output) < 0) {
      valid = false;
      break;
    }
  }

  if (std::fclose(output) != 0) {
    valid = false;
  }

  if (!valid) {
    std::remove(outputPath);
    return false;
  }

  state.report.total = static_cast<int>(state.rows.size());
  report = state.report;
  return true;
}

} // namespace aipb::explorer::bridge
