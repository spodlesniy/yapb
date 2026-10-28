#pragma once

#include <cmath>
#include <cstddef>
#include <cstdio>

#include <aipb/explorer/engine_bridge_trace_classification.h>
#include <aipb/explorer/engine_bridge_trace_io.h>

namespace aipb::explorer::bridge {

inline constexpr const char kObservationCsvColumns[] =
  "first_step,last_step,state_start_step,classification,contact,duration_ms,"
  "start_x,start_y,start_z,end_x,end_y,end_z,heading_degrees,"
  "requested_units,observed_xy_units,projected_units,lateral_units\n";

struct ObservationWriteResult {
  int records {};
  int progress {};
  int blockedCandidates {};
  int deflectedCandidates {};
};

inline bool formatObservationHeader(
  char *buffer,
  std::size_t capacity,
  const char *map,
  const TraceBuffer &trace
) {
  if (!buffer || !safeName(map, 63) || trace.count <= 0
    || trace.count > ProbeBudget::kMaxSafetyFrames || trace.botIndex < -1) {
    return false;
  }

  const int written = std::snprintf(buffer, capacity,
    "aipb-explorer-observations-v1,%s,%d,%d\n",
    map, trace.botIndex, trace.count);

  return written > 0 && static_cast<std::size_t>(written) < capacity;
}

inline bool formatObservation(
  char *buffer,
  std::size_t capacity,
  const TraceWindowEvidence &window
) {
  if (!buffer || window.firstStep < 0 || window.lastStep < window.firstStep
    || window.lastStep >= ProbeBudget::kMaxSafetyFrames
    || window.stateStartStep < 0 || window.stateStartStep > window.firstStep
    || window.durationMs < 100
    || window.classification == MovementClassification::InsufficientEvidence
    || !validState(window.start) || !validState(window.end)
    || !std::isfinite(window.headingDegrees) || !std::isfinite(window.requestedUnits)
    || !std::isfinite(window.observedUnits) || !std::isfinite(window.projectedUnits)
    || !std::isfinite(window.lateralUnits)
    || window.requestedUnits < 12.0 || window.observedUnits < 0.0) {
    return false;
  }

  const int written = std::snprintf(buffer, capacity,
    "%d,%d,%d,%s,unknown,%d,"
    "%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.9g,"
    "%.17g,%.17g,%.17g,%.17g\n",
    window.firstStep, window.lastStep, window.stateStartStep,
    movementClassificationName(window.classification), window.durationMs,
    window.start.x, window.start.y, window.start.z,
    window.end.x, window.end.y, window.end.z, window.headingDegrees,
    window.requestedUnits, window.observedUnits,
    window.projectedUnits, window.lateralUnits);

  return written > 0 && static_cast<std::size_t>(written) < capacity;
}

struct ObservationWriteContext {
  std::FILE *file {};
  bool ok { true };
  ObservationWriteResult result {};
};

inline void emitObservation(void *context, const TraceWindowEvidence &window) {
  auto &writer = *static_cast<ObservationWriteContext *>(context);

  if (!writer.ok) {
    return;
  }

  char line[768] {};
  writer.ok = formatObservation(line, sizeof(line), window)
    && std::fputs(line, writer.file) >= 0;

  if (writer.ok) {
    ++writer.result.records;

    switch (window.classification) {
    case MovementClassification::Progress:
      ++writer.result.progress;
      break;
    case MovementClassification::BlockedCandidate:
      ++writer.result.blockedCandidates;
      break;
    case MovementClassification::DeflectedCandidate:
      ++writer.result.deflectedCandidates;
      break;
    case MovementClassification::InsufficientEvidence:
      break;
    }
  }
}

// Export confirmed local windows to a new file, never overwrite a prior observation.
inline bool writeTraceObservations(
  const TraceBuffer &trace,
  const char *map,
  const char *path,
  ObservationWriteResult &result
) {
  TraceMovementClassification validated {};

  if (!path || !*path || !safeName(map, 63)
    || trace.count <= 0 || trace.count > ProbeBudget::kMaxSafetyFrames
    || trace.botIndex < -1 || !classifyTraceMovement(trace, validated)) {
    return false;
  }

  std::FILE *file = openTraceFile(path, "wx");

  if (!file) {
    return false;
  }

  char header[128] {};
  ObservationWriteContext writer {};
  writer.file = file;
  writer.ok = formatObservationHeader(header, sizeof(header), map, trace)
    && std::fputs(header, file) >= 0
    && std::fputs(kObservationCsvColumns, file) >= 0;

  if (writer.ok) {
    TraceMovementClassification summary {};

    if (!classifyTraceMovement(trace, summary, &emitObservation, &writer)) {
      writer.ok = false;
    }
  }

  if (std::fclose(file) != 0) {
    writer.ok = false;
  }

  if (!writer.ok || writer.result.records == 0) {
    std::remove(path);
    return false;
  }

  result = writer.result;
  return true;
}

} // namespace aipb::explorer::bridge
