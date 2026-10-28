#pragma once

#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <aipb/explorer/engine_bridge_direction_assessment.h>

namespace aipb::explorer::bridge {

inline constexpr char kTrialSetVersion[] = "aipb-explorer-trials-v1";
inline constexpr int kMaxTrialSources = 16;

struct EvidenceTrial {
  std::string name {};
  int botIndex {};
  int sourceSteps {};
  std::vector<std::string> rows {};
};

struct EvidenceTrialSet {
  std::vector<EvidenceTrial> trials {};
  int rows {};
};

struct EvidenceTrialReport {
  int trials {};
  int rows {};
};

struct TrialCorroboration {
  SpatialCell cell {};
  int headingBin {};
  int sources {};
  int observedSources {};
  int progressSources {};
  int restrictionSources {};
  int mixedSources {};
  int observations {};
  int progress {};
  int blockedCandidates {};
  int deflectedCandidates {};
  DirectionAssessmentStatus status { DirectionAssessmentStatus::Unobserved };
};

inline bool appendEvidenceTrials(EvidenceTrialSet &destination, const EvidenceTrialSet &source) {
  std::set<std::string> names {};
  std::set<std::string> records {};

  for (const EvidenceTrial &trial : destination.trials) {
    names.insert(trial.name);

    for (const std::string &row : trial.rows) {
      records.insert(row);
    }
  }

  for (const EvidenceTrial &trial : source.trials) {
    if (!safeName(trial.name.c_str()) || trial.botIndex < -1 || trial.botIndex > 31
      || trial.sourceSteps < 1 || trial.sourceSteps > ProbeBudget::kMaxSafetyFrames
      || trial.rows.empty() || names.count(trial.name) != 0
      || destination.trials.size() >= kMaxTrialSources
      || destination.rows + trial.rows.size() > kMaxCollectionRows) {
      return false;
    }

    for (const std::string &row : trial.rows) {
      // Shared canonical windows cannot provide independent experiment evidence.
      if (!records.insert(row).second) {
        return false;
      }
    }

    names.insert(trial.name);
    destination.rows += static_cast<int>(trial.rows.size());
    destination.trials.push_back(trial);
  }

  return true;
}

inline bool readRawEvidenceTrial(
  const char *path,
  const char *map,
  const char *name,
  EvidenceTrialSet &result
) {
  if (!safeName(name) || !safeName(map, 63)) {
    return false;
  }

  std::FILE *file = openTraceFile(path, "rt");

  if (!file) {
    return false;
  }

  char header[256] {};
  bool ok = std::fgets(header, sizeof(header), file) != nullptr;
  int botIndex = 0;
  int steps = 0;

  if (ok) {
    char *fields[4] {};
    ok = splitCollectionFields(header, fields, 4)
      && std::strcmp(fields[0], kObservationVersion) == 0
      && std::strcmp(fields[1], map) == 0
      && collectionInteger(fields[2], -1, 31, botIndex)
      && collectionInteger(fields[3], 1, ProbeBudget::kMaxSafetyFrames, steps);
  }

  if (std::fclose(file) != 0 || !ok) {
    return false;
  }

  CollectionReadState validated {};

  if (!readObservationCollection(path, map, validated)
    || validated.rows.empty() || validated.report.duplicates != 0) {
    return false;
  }

  EvidenceTrialSet next {};
  EvidenceTrial trial {};
  trial.name = name;
  trial.botIndex = botIndex;
  trial.sourceSteps = steps;
  trial.rows = std::move(validated.rows);
  next.rows = static_cast<int>(trial.rows.size());
  next.trials.push_back(std::move(trial));
  result = std::move(next);
  return true;
}

inline bool readEvidenceTrialSet(
  const char *path,
  const char *map,
  EvidenceTrialSet &result
) {
  if (!path || !*path || !safeName(map, 63)) {
    return false;
  }

  std::FILE *file = openTraceFile(path, "rt");

  if (!file) {
    return false;
  }

  bool ok = true;
  char line[1024] {};
  int trialCount = 0;
  int rowCount = 0;
  EvidenceTrialSet next {};

  if (!std::fgets(line, sizeof(line), file)) {
    ok = false;
  }

  if (ok) {
    char *fields[4] {};
    ok = splitCollectionFields(line, fields, 4)
      && std::strcmp(fields[0], kTrialSetVersion) == 0
      && std::strcmp(fields[1], map) == 0
      && collectionInteger(fields[2], 1, kMaxTrialSources, trialCount)
      && collectionInteger(fields[3], 1, kMaxCollectionRows, rowCount);
  }

  if (ok && (!std::fgets(line, sizeof(line), file)
    || std::strcmp(line, kObservationCsvColumns) != 0)) {
    ok = false;
  }

  std::set<std::string> names {};
  std::set<std::string> uniqueRows {};

  for (int i = 0; ok && i < trialCount; ++i) {
    if (!std::fgets(line, sizeof(line), file)) {
      ok = false;
      break;
    }

    char *fields[5] {};
    EvidenceTrial trial {};
    int rows = 0;

    if (!splitCollectionFields(line, fields, 5)
      || std::strcmp(fields[0], "T") != 0
      || !safeName(fields[1])
      || !collectionInteger(fields[2], -1, 31, trial.botIndex)
      || !collectionInteger(fields[3], 1, ProbeBudget::kMaxSafetyFrames, trial.sourceSteps)
      || !collectionInteger(fields[4], 1, ProbeBudget::kMaxSafetyFrames, rows)
      || next.rows + rows > rowCount
      || !names.insert(fields[1]).second) {
      ok = false;
      break;
    }

    trial.name = fields[1];
    int lastStep = -1;

    for (int j = 0; ok && j < rows; ++j) {
      if (!std::fgets(line, sizeof(line), file) || std::strncmp(line, "R,", 2) != 0) {
        ok = false;
        break;
      }

      char original[1024] {};
      std::memcpy(original, line + 2, std::strlen(line + 2) + 1);
      char normalized[768] {};

      if (!normalizeObservationRow(original, normalized, sizeof(normalized), trial.sourceSteps)
        || std::strcmp(normalized, line + 2) != 0
        || !uniqueRows.insert(normalized).second) {
        ok = false;
        break;
      }

      char copy[768] {};
      std::memcpy(copy, normalized, std::strlen(normalized) + 1);
      char *rowFields[18] {};
      int first = 0;
      int last = 0;

      if (!splitCollectionFields(copy, rowFields, 18)
        || !collectionInteger(rowFields[0], 0, trial.sourceSteps - 1, first)
        || !collectionInteger(rowFields[1], first, trial.sourceSteps - 1, last)
        || first <= lastStep) {
        ok = false;
        break;
      }

      lastStep = last;
      trial.rows.emplace_back(normalized);
    }

    if (ok) {
      next.rows += rows;
      next.trials.push_back(std::move(trial));
    }
  }

  if (ok && (next.rows != rowCount || std::fgets(line, sizeof(line), file)
    || std::ferror(file))) {
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

inline bool readTrialSource(
  const char *path,
  const char *map,
  const char *name,
  EvidenceTrialSet &result
) {
  if (!path || !*path || !safeName(name)) {
    return false;
  }

  std::FILE *file = openTraceFile(path, "rt");

  if (!file) {
    return false;
  }

  char line[256] {};
  bool ok = std::fgets(line, sizeof(line), file) != nullptr;
  bool snapshot = ok && std::strncmp(line, kTrialSetVersion,
    sizeof(kTrialSetVersion) - 1) == 0
    && line[sizeof(kTrialSetVersion) - 1] == ',';

  if (std::fclose(file) != 0 || !ok) {
    return false;
  }

  return snapshot ? readEvidenceTrialSet(path, map, result)
    : readRawEvidenceTrial(path, map, name, result);
}

inline bool writeEvidenceTrialSet(
  const char *firstPath,
  const char *firstName,
  const char *secondPath,
  const char *secondName,
  const char *outputPath,
  const char *map,
  EvidenceTrialReport &report
) {
  if (!firstPath || !secondPath || !outputPath || !*outputPath
    || !safeName(map, 63) || !safeName(firstName) || !safeName(secondName)
    || std::strcmp(firstPath, secondPath) == 0
    || std::strcmp(firstPath, outputPath) == 0
    || std::strcmp(secondPath, outputPath) == 0) {
    return false;
  }

  EvidenceTrialSet a {};
  EvidenceTrialSet b {};
  EvidenceTrialSet combined {};

  if (!readTrialSource(firstPath, map, firstName, a)
    || !readTrialSource(secondPath, map, secondName, b)
    || !appendEvidenceTrials(combined, a)
    || !appendEvidenceTrials(combined, b)) {
    return false;
  }

  std::FILE *file = openTraceFile(outputPath, "wx");

  if (!file) {
    return false;
  }

  bool ok = std::fprintf(file, "%s,%s,%zu,%d\n", kTrialSetVersion,
    map, combined.trials.size(), combined.rows) > 0
    && std::fputs(kObservationCsvColumns, file) >= 0;

  for (const EvidenceTrial &trial : combined.trials) {
    if (!ok) {
      break;
    }

    ok = std::fprintf(file, "T,%s,%d,%d,%zu\n", trial.name.c_str(),
      trial.botIndex, trial.sourceSteps, trial.rows.size()) > 0;

    for (const std::string &row : trial.rows) {
      if (!ok) {
        break;
      }

      ok = std::fprintf(file, "R,%s", row.c_str()) > 0;
    }
  }

  if (std::fclose(file) != 0) {
    ok = false;
  }

  if (!ok) {
    std::remove(outputPath);
    return false;
  }

  report = { static_cast<int>(combined.trials.size()), combined.rows };
  return true;
}

// One source trace contributes at most one vote per outcome, regardless of window count.
inline bool corroborateTrialDirection(
  const EvidenceTrialSet &sources,
  double x,
  double y,
  double z,
  double heading,
  TrialCorroboration &result
) {
  if (sources.trials.empty() || sources.trials.size() > kMaxTrialSources
    || !spatialCoordinate(x) || !spatialCoordinate(y) || !spatialCoordinate(z)
    || !std::isfinite(heading)) {
    return false;
  }

  TrialCorroboration next {};
  next.cell = { spatialCell(x), spatialCell(y), spatialCell(z) };
  next.headingBin = directionBin(heading);
  next.sources = static_cast<int>(sources.trials.size());

  for (const EvidenceTrial &trial : sources.trials) {
    SpatialIndex index {};

    if (!buildSpatialIndex(trial.rows, index)) {
      return false;
    }

    DirectionAssessment assessment {};

    if (!assessDirectionalEvidence(index, x, y, z, heading, assessment)) {
      return false;
    }

    next.observations += assessment.observations;
    next.progress += assessment.progress;
    next.blockedCandidates += assessment.blockedCandidates;
    next.deflectedCandidates += assessment.deflectedCandidates;

    const bool progress = assessment.progress > 0;
    const bool restriction = assessment.blockedCandidates > 0
      || assessment.deflectedCandidates > 0;

    if (progress || restriction) {
      ++next.observedSources;
    }

    if (progress) {
      ++next.progressSources;
    }

    if (restriction) {
      ++next.restrictionSources;
    }

    if (progress && restriction) {
      ++next.mixedSources;
    }
  }

  if (next.observedSources == 0) {
    next.status = DirectionAssessmentStatus::Unobserved;
  }
  else if (next.progressSources > 0 && next.restrictionSources > 0) {
    next.status = DirectionAssessmentStatus::Conflicted;
  }
  else if (next.progressSources >= 2) {
    next.status = DirectionAssessmentStatus::ProgressEvidence;
  }
  else if (next.restrictionSources >= 2) {
    next.status = DirectionAssessmentStatus::RestrictionEvidence;
  }
  else {
    next.status = DirectionAssessmentStatus::Insufficient;
  }

  result = next;
  return true;
}

} // namespace aipb::explorer::bridge
