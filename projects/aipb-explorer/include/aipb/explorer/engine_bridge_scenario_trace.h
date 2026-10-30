#pragma once

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include <aipb/explorer/engine_bridge_scenario_result.h>
#include <aipb/explorer/engine_bridge_trace_io.h>

namespace aipb::explorer::scenario {

inline constexpr const char *kScenarioTraceVersion = "aipb-scenario-trace-v1";
inline constexpr const char *kScenarioTraceColumns =
  "frame,action,msec,forward_speed,strafe_speed,max_speed,buttons,old_buttons,"
  "applied_yaw,applied_pitch,"
  "bx,by,bz,bvx,bvy,bvz,byaw,bpitch,bground,bduck,"
  "ax,ay,az,avx,avy,avz,ayaw,apitch,aground,aduck\n";

struct ScenarioTraceFrame {
  int frameIndex {};
  std::size_t actionIndex {};
  int milliseconds {};
  float forwardSpeed {};
  float strafeSpeed {};
  float maxSpeed {};
  unsigned buttons {};
  int oldButtons {};
  float appliedYaw {};
  float appliedPitch {};
  ScenarioResultPose before {};
  ScenarioResultPose after {};
};

inline bool validScenarioTraceFrame(const ScenarioTraceFrame &frame) {
  return frame.frameIndex >= 0 && frame.frameIndex < ActionScheduler::kMaxFrames
    && frame.actionIndex < static_cast<std::size_t>(kMaxActions)
    && frame.milliseconds >= 1 && frame.milliseconds <= ActionScheduler::kMaxFrameMs
    && std::isfinite(frame.forwardSpeed) && std::isfinite(frame.strafeSpeed)
    && std::isfinite(frame.maxSpeed) && frame.maxSpeed >= 0.0f
    && frame.buttons <= 65535u
    && std::isfinite(frame.appliedYaw) && std::isfinite(frame.appliedPitch)
    && validScenarioResultPose(frame.before) && validScenarioResultPose(frame.after);
}

// The trace includes only engine-applied moves, even when scheduler confirmation
// fails afterward. Frame indices are contiguous and actions never go backward.
class ScenarioTrace {
public:
  void clear() {
    frames_.clear();
  }

  std::size_t size() const {
    return frames_.size();
  }

  const std::vector<ScenarioTraceFrame> &frames() const {
    return frames_;
  }

  bool append(const ScenarioTraceFrame &frame) {
    if (!validScenarioTraceFrame(frame)
      || frames_.size() >= static_cast<std::size_t>(ActionScheduler::kMaxFrames)
      || frame.frameIndex != static_cast<int>(frames_.size())) {
      return false;
    }

    if (!frames_.empty()) {
      const auto previous = frames_.back().actionIndex;
      if (frame.actionIndex < previous || frame.actionIndex > previous + 1) {
        return false;
      }
    }
    else if (frame.actionIndex != 0) {
      return false;
    }

    frames_.push_back(frame);
    return true;
  }

private:
  std::vector<ScenarioTraceFrame> frames_ {};
};

inline bool formatScenarioTraceFrame(
  char *buffer, std::size_t capacity, const ScenarioTraceFrame &frame
) {
  if (!buffer || capacity == 0 || !validScenarioTraceFrame(frame)) {
    return false;
  }

  const auto &b = frame.before;
  const auto &a = frame.after;
  const int written = std::snprintf(buffer, capacity,
    "%d,%u,%d,%.9g,%.9g,%.9g,%u,%d,%.9g,%.9g,"
    "%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%d,%d,"
    "%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%d,%d\n",
    frame.frameIndex, static_cast<unsigned>(frame.actionIndex), frame.milliseconds,
    frame.forwardSpeed, frame.strafeSpeed, frame.maxSpeed,
    frame.buttons, frame.oldButtons, frame.appliedYaw, frame.appliedPitch,
    b.position[0], b.position[1], b.position[2],
    b.velocity[0], b.velocity[1], b.velocity[2], b.yaw, b.pitch,
    b.onGround ? 1 : 0, b.crouching ? 1 : 0,
    a.position[0], a.position[1], a.position[2],
    a.velocity[0], a.velocity[1], a.velocity[2], a.yaw, a.pitch,
    a.onGround ? 1 : 0, a.crouching ? 1 : 0);
  return written > 0 && static_cast<std::size_t>(written) < capacity;
}

// A complete attempt is identified by result.csv, written after trace.csv.
// Both files are exclusive, and each run first acquires its own new directory.
inline bool saveScenarioRunBundle(
  const std::filesystem::path &resultsRoot,
  const std::string &timestamp,
  const ScenarioResult &result,
  const ScenarioTrace &trace,
  std::string &savedDirectory
) {
  namespace fs = std::filesystem;
  savedDirectory.clear();

  char summary[2048] {};
  if (timestamp.size() != 15 || timestamp[8] != '_'
    || !formatScenarioResult(summary, sizeof(summary), result)) {
    return false;
  }
  for (std::size_t i = 0; i < timestamp.size(); ++i) {
    const char c = timestamp[i];
    if (i != 8 && (c < '0' || c > '9')) {
      return false;
    }
  }

  char line[1024] {};
  for (const auto &frame : trace.frames()) {
    if (!formatScenarioTraceFrame(line, sizeof(line), frame)) {
      return false;
    }
  }

  std::error_code error {};
  fs::create_directories(resultsRoot, error);
  if (error) {
    return false;
  }

  for (int suffix = 0; suffix < 1000; ++suffix) {
    char name[32] {};
    std::snprintf(name, sizeof(name), "%s_%03d", timestamp.c_str(), suffix);
    const fs::path runDirectory = resultsRoot / name;
    error.clear();
    if (!fs::create_directory(runDirectory, error)) {
      // Existing directories are not errors for create_directory on every CRT.
      if (!error || error == std::errc::file_exists) {
        continue;
      }
      return false;
    }

    const fs::path traceFile = runDirectory / "trace.csv";
    const fs::path resultFile = runDirectory / "result.csv";
    const auto tracePath = traceFile.string();
    const auto resultPath = resultFile.string();
    std::FILE *file = bridge::openTraceFile(tracePath.c_str(), "wx");
    bool ok = file != nullptr;
    if (ok) {
      ok = std::fprintf(file, "%s\n", kScenarioTraceVersion) >= 0
        && std::fputs(kScenarioTraceColumns, file) >= 0;
      for (const auto &frame : trace.frames()) {
        if (!ok) {
          break;
        }
        ok = formatScenarioTraceFrame(line, sizeof(line), frame)
          && std::fputs(line, file) >= 0;
      }
      if (std::fclose(file) != 0) {
        ok = false;
      }
    }

    if (ok) {
      file = bridge::openTraceFile(resultPath.c_str(), "wx");
      ok = file != nullptr;
      if (ok) {
        ok = std::fputs(summary, file) >= 0;
        if (std::fclose(file) != 0) {
          ok = false;
        }
      }
    }

    if (!ok) {
      std::error_code ignored {};
      fs::remove(resultFile, ignored);
      fs::remove(traceFile, ignored);
      fs::remove(runDirectory, ignored);
      return false;
    }

    savedDirectory = runDirectory.string();
    return true;
  }

  return false;
}

} // namespace aipb::explorer::scenario
