#pragma once

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <aipb/explorer/engine_bridge_queue.h>
#include <aipb/explorer/engine_bridge_trace_io.h>

namespace aipb::explorer::bridge {

inline constexpr char kExperimentDescriptorVersion[] = "aipb-experiment-v1";
inline constexpr char kExperimentResultVersion[] = "aipb-experiment-result-v1";

// Stage A begins with a bounded, straight grounded program. No engine-side
// teleport, reset or replay is authorized by the descriptor.
struct ExperimentCommand {
  AipbExplorerBridgeCommand input {};
  int frames {};
};

struct GroundExperiment {
  std::string id {};
  std::string sourceId {};
  std::string map {};
  std::string weapon {};
  int botIndex { -1 };
  bool verifiedStart {};
  AipbExplorerBridgeState start {};
  float weaponSpeedCap {};
  int maxFrames {};
  int maxDurationMs {};
  double targetHeading {};
  double minimumProjectedTravel {};
  std::vector<ExperimentCommand> commands {};
};

inline bool validGroundExperiment(const GroundExperiment &experiment) {
  if (!safeName(experiment.id.c_str()) || !safeName(experiment.sourceId.c_str())
    || !safeName(experiment.map.c_str(), 63) || !safeName(experiment.weapon.c_str())
    || experiment.botIndex < 0 || experiment.botIndex > 31
    || !experiment.verifiedStart || !validState(experiment.start)
    || !experiment.start.on_ground || experiment.start.crouching
    || experiment.start.tick != 0
    || !std::isfinite(experiment.weaponSpeedCap) || experiment.weaponSpeedCap <= 0.0f
    || experiment.weaponSpeedCap > 400.0f
    || experiment.maxFrames < 1 || experiment.maxFrames > ProbeBudget::kMaxFrames
    || experiment.maxDurationMs < 1 || experiment.maxDurationMs > ProbeBudget::kMaxDurationMs
    || !std::isfinite(experiment.targetHeading) || experiment.targetHeading < -180.0
    || experiment.targetHeading >= 180.0
    || !std::isfinite(experiment.minimumProjectedTravel)
    || experiment.minimumProjectedTravel <= 0.0
    || experiment.commands.empty()
    || experiment.commands.size() > ProbeQueue::kMaxCommands) {
    return false;
  }

  int frames = 0;

  for (const ExperimentCommand &step : experiment.commands) {
    if (!valid(step.input) || step.input.jump || step.input.duck
      || step.input.yaw_delta != 0.0f || step.frames < 1
      || step.frames > ProbeBudget::kMaxFrames
      || (step.input.forward == 0.0f && step.input.strafe == 0.0f)) {
      return false;
    }

    frames += step.frames;

    if (frames > experiment.maxFrames) {
      return false;
    }
  }

  return frames == experiment.maxFrames;
}

inline std::vector<std::string> splitExperimentFields(const std::string &line) {
  std::vector<std::string> fields {};
  std::size_t start = 0;

  while (start <= line.size()) {
    const std::size_t separator = line.find(',', start);
    const std::string field = line.substr(start, separator == std::string::npos
      ? std::string::npos : separator - start);

    if (field.empty()) {
      return {};
    }

    fields.push_back(field);

    if (separator == std::string::npos) {
      break;
    }

    start = separator + 1;
  }

  return fields;
}

inline bool experimentNumber(const std::string &text, double &value) {
  char *end {};
  value = std::strtod(text.c_str(), &end);
  return end != text.c_str() && *end == '\0' && std::isfinite(value);
}

inline bool experimentInteger(
  const std::string &text,
  int minimum,
  int maximum,
  int &value
) {
  char *end {};
  const long parsed = std::strtol(text.c_str(), &end, 10);

  if (end == text.c_str() || *end != '\0' || parsed < minimum || parsed > maximum) {
    return false;
  }

  value = static_cast<int>(parsed);
  return true;
}

inline bool formatGroundExperiment(
  const GroundExperiment &experiment,
  std::string &output
) {
  if (!validGroundExperiment(experiment)) {
    return false;
  }

  char line[1024] {};
  const auto &s = experiment.start;
  const int size = std::snprintf(line, sizeof(line),
    "%s,%s,%s,%s,%s,%d,1,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.9g,%.9g,1,0,%.9g,%d,%d,%.17g,%.17g,%zu\n",
    kExperimentDescriptorVersion, experiment.id.c_str(), experiment.sourceId.c_str(),
    experiment.map.c_str(), experiment.weapon.c_str(), experiment.botIndex,
    s.x, s.y, s.z, s.vx, s.vy, s.vz, s.yaw, s.pitch,
    experiment.weaponSpeedCap, experiment.maxFrames, experiment.maxDurationMs,
    experiment.targetHeading, experiment.minimumProjectedTravel,
    experiment.commands.size());

  if (size <= 0 || static_cast<std::size_t>(size) >= sizeof(line)) {
    return false;
  }

  std::string next { line };

  for (const ExperimentCommand &command : experiment.commands) {
    const auto &c = command.input;
    const int written = std::snprintf(line, sizeof(line),
      "C,%.9g,%.9g,%.9g,%u,%u,%d\n",
      c.forward, c.strafe, c.yaw_delta,
      static_cast<unsigned>(c.jump), static_cast<unsigned>(c.duck), command.frames);

    if (written <= 0 || static_cast<std::size_t>(written) >= sizeof(line)) {
      return false;
    }

    next += line;
  }

  output = std::move(next);
  return true;
}

inline bool parseGroundExperiment(
  const std::string &data,
  GroundExperiment &result
) {
  const std::size_t headerEnd = data.find('\n');

  if (headerEnd == std::string::npos) {
    return false;
  }

  const auto fields = splitExperimentFields(data.substr(0, headerEnd));

  if (fields.size() != 23 || fields[0] != kExperimentDescriptorVersion) {
    return false;
  }

  GroundExperiment next {};
  next.id = fields[1];
  next.sourceId = fields[2];
  next.map = fields[3];
  next.weapon = fields[4];
  int verified {};
  int ground {};
  int crouched {};
  int commandCount {};
  double speed {};

  if (!experimentInteger(fields[5], 0, 31, next.botIndex)
    || !experimentInteger(fields[6], 0, 1, verified)
    || !experimentNumber(fields[7], next.start.x)
    || !experimentNumber(fields[8], next.start.y)
    || !experimentNumber(fields[9], next.start.z)
    || !experimentNumber(fields[10], next.start.vx)
    || !experimentNumber(fields[11], next.start.vy)
    || !experimentNumber(fields[12], next.start.vz)) {
    return false;
  }

  double yaw {};
  double pitch {};

  if (!experimentNumber(fields[13], yaw) || !experimentNumber(fields[14], pitch)
    || !experimentInteger(fields[15], 0, 1, ground)
    || !experimentInteger(fields[16], 0, 1, crouched)
    || !experimentNumber(fields[17], speed)
    || !experimentInteger(fields[18], 1, ProbeBudget::kMaxFrames, next.maxFrames)
    || !experimentInteger(fields[19], 1, ProbeBudget::kMaxDurationMs, next.maxDurationMs)
    || !experimentNumber(fields[20], next.targetHeading)
    || !experimentNumber(fields[21], next.minimumProjectedTravel)
    || !experimentInteger(fields[22], 1, ProbeQueue::kMaxCommands, commandCount)
    || yaw < -180.0 || yaw >= 180.0 || pitch < -90.0 || pitch > 90.0
    || speed <= 0.0 || speed > 400.0) {
    return false;
  }

  next.verifiedStart = verified != 0;
  next.start.yaw = static_cast<float>(yaw);
  next.start.pitch = static_cast<float>(pitch);
  next.start.on_ground = static_cast<uint8_t>(ground);
  next.start.crouching = static_cast<uint8_t>(crouched);
  next.weaponSpeedCap = static_cast<float>(speed);
  std::size_t offset = headerEnd + 1;

  for (int index = 0; index < commandCount; ++index) {
    const std::size_t end = data.find('\n', offset);

    if (end == std::string::npos) {
      return false;
    }

    const auto row = splitExperimentFields(data.substr(offset, end - offset));

    if (row.size() != 7 || row[0] != "C") {
      return false;
    }

    double forward {};
    double strafe {};
    double delta {};
    int jump {};
    int duck {};
    ExperimentCommand command {};

    if (!experimentNumber(row[1], forward)
      || !experimentNumber(row[2], strafe)
      || !experimentNumber(row[3], delta)
      || !experimentInteger(row[4], 0, 1, jump)
      || !experimentInteger(row[5], 0, 1, duck)
      || !experimentInteger(row[6], 1, ProbeBudget::kMaxFrames, command.frames)
      || forward < -1.0 || forward > 1.0
      || strafe < -1.0 || strafe > 1.0
      || delta < -180.0 || delta > 180.0) {
      return false;
    }

    command.input = {
      static_cast<float>(forward),
      static_cast<float>(strafe),
      static_cast<float>(delta),
      static_cast<uint8_t>(jump),
      static_cast<uint8_t>(duck)
    };
    next.commands.push_back(command);
    offset = end + 1;
  }

  if (offset != data.size() || !validGroundExperiment(next)) {
    return false;
  }

  result = std::move(next);
  return true;
}

} // namespace aipb::explorer::bridge
