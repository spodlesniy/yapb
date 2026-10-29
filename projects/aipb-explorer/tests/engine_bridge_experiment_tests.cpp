#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

#include <aipb/explorer/engine_bridge_experiment_evaluation.h>

using namespace aipb::explorer::bridge;

static void check(bool condition) {
  if (!condition) {
    throw std::runtime_error("experiment framework regression");
  }
}

static GroundExperiment makeExperiment() {
  GroundExperiment e {};
  e.id = "ground_forward_01";
  e.sourceId = "independent_run_01";
  e.map = "de_dust2";
  e.weapon = "knife";
  e.botIndex = 1;
  e.verifiedStart = true;
  e.start = {};
  e.start.x = 256.0;
  e.start.y = 2512.0;
  e.start.z = -92.0;
  e.start.on_ground = 1;
  e.weaponSpeedCap = 250.0f;
  e.maxFrames = 4;
  e.maxDurationMs = 600;
  e.targetHeading = 0.0;
  e.minimumProjectedTravel = 12.0;
  e.commands.push_back({ { 1.0f, 0.0f, 0.0f, 0, 0 }, 2 });
  e.commands.push_back({ { 0.5f, 0.0f, 0.0f, 0, 0 }, 2 });
  return e;
}

static TraceMetadata metadata(const GroundExperiment &e) {
  TraceMetadata info {};
  std::snprintf(info.map, sizeof(info.map), "%s", e.map.c_str());
  info.initial = e.start;
  info.viewZ = 17.0f;
  return info;
}

static TraceBuffer record(
  const GroundExperiment &experiment,
  double unitsPerFrame,
  int frames = 4,
  int msec = 100
) {
  TraceBuffer trace {};
  AipbExplorerBridgeState current = experiment.start;

  for (int i = 0; i < frames; ++i) {
    TraceSample sample {};
    const AipbExplorerBridgeCommand command = i < 2
      ? experiment.commands[0].input : experiment.commands[1].input;
    sample.command = command;
    sample.maxSpeed = experiment.weaponSpeedCap;
    sample.msec = static_cast<uint8_t>(msec);
    sample.before = current;
    sample.after = current;
    sample.after.x += unitsPerFrame;
    sample.after.tick = current.tick + 1;
    check(trace.append(sample));
    current = sample.after;
  }

  trace.completed = true;
  return trace;
}

int main() {
  try {
    const GroundExperiment original = makeExperiment();
    check(validGroundExperiment(original));

    std::string encoded {};
    check(formatGroundExperiment(original, encoded));
    check(encoded.find("aipb-experiment-v1,ground_forward_01") == 0);
    GroundExperiment parsed {};
    check(parseGroundExperiment(encoded, parsed));
    std::string repeated {};
    check(formatGroundExperiment(parsed, repeated) && encoded == repeated);
    check(parsed.commands.size() == 2 && parsed.maxFrames == 4);
    check(parsed.start.x == 256.0 && parsed.weaponSpeedCap == 250.0f);

    GroundExperiment unchanged = original;
    check(!parseGroundExperiment("aipb-experiment-v0\n", unchanged));
    check(unchanged.id == original.id);
    check(!parseGroundExperiment(encoded + "bad\n", unchanged));
    check(!parseGroundExperiment(encoded.substr(0, encoded.size() - 1), unchanged));
    std::string wrongVersion = encoded;
    wrongVersion.replace(0, std::strlen(kExperimentDescriptorVersion), "aipb-experiment-v0");
    check(!parseGroundExperiment(wrongVersion, unchanged));
    std::string badNumber = encoded;
    const auto where = badNumber.find(",250,4,600,");

    if (where != std::string::npos) {
      badNumber.replace(where + 1, 3, "nan");
      check(!parseGroundExperiment(badNumber, unchanged));
    }

    GroundExperiment invalid = original;
    invalid.verifiedStart = false;
    check(!validGroundExperiment(invalid));
    invalid = original;
    invalid.start.on_ground = 0;
    check(!validGroundExperiment(invalid));
    invalid = original;
    invalid.start.crouching = 1;
    check(!validGroundExperiment(invalid));
    invalid = original;
    invalid.commands[0].input.jump = 1;
    check(!validGroundExperiment(invalid));
    invalid = original;
    invalid.maxFrames = 3;
    check(!validGroundExperiment(invalid));
    invalid = original;
    invalid.commands[0].frames = 0;
    check(!validGroundExperiment(invalid));
    invalid = original;
    invalid.weaponSpeedCap = 0;
    check(!validGroundExperiment(invalid));
    invalid = original;
    invalid.id = "bad/name";
    check(!validGroundExperiment(invalid));

    TraceMetadata info = metadata(original);
    GroundExperimentResult result {};
    TraceBuffer success = record(original, 4.0);
    check(evaluateGroundExperiment(original, info, success, result));
    check(result.outcome == ExperimentOutcome::Success);
    check(result.reason == ExperimentReason::TargetReached);
    check(result.frames == 4 && result.elapsedMs == 400);
    check(result.projectedTravel == 16.0);
    check(result.observedStart.x == 256.0 && result.observedEnd.x == 272.0);
    std::string summary {};
    check(formatGroundExperimentResult(result, summary));
    check(summary.find("aipb-experiment-result-v1,ground_forward_01,independent_run_01,Success,TargetReached") == 0);

    TraceBuffer below = record(original, 1.0);
    check(evaluateGroundExperiment(original, info, below, result));
    check(result.outcome == ExperimentOutcome::Failure);
    check(result.reason == ExperimentReason::BelowTarget);

    TraceBuffer unfinished = record(original, 4.0, 2);
    unfinished.completed = false;
    check(evaluateGroundExperiment(original, info, unfinished, result));
    check(result.outcome == ExperimentOutcome::Inconclusive);
    check(result.reason == ExperimentReason::Incomplete);

    TraceMetadata otherMap = info;
    std::snprintf(otherMap.map, sizeof(otherMap.map), "%s", "de_inferno");
    check(evaluateGroundExperiment(original, otherMap, success, result));
    check(result.outcome == ExperimentOutcome::Diverged);
    check(result.reason == ExperimentReason::MapMismatch);

    TraceBuffer displaced = record(original, 4.0);
    displaced.samples[0].before.x += 1.0;
    check(evaluateGroundExperiment(original, info, displaced, result));
    check(result.reason == ExperimentReason::StartMismatch);

    TraceBuffer wrongInput = record(original, 4.0);
    wrongInput.samples[1].command.forward = 0.25f;
    check(evaluateGroundExperiment(original, info, wrongInput, result));
    check(result.outcome == ExperimentOutcome::Diverged);
    check(result.reason == ExperimentReason::CommandMismatch);

    TraceBuffer wrongSpeed = record(original, 4.0);
    wrongSpeed.samples[0].maxSpeed = 200.0f;
    check(evaluateGroundExperiment(original, info, wrongSpeed, result));
    check(result.reason == ExperimentReason::SpeedCapMismatch);

    TraceBuffer slow = record(original, 4.0, 4, 200);
    check(evaluateGroundExperiment(original, info, slow, result));
    check(result.reason == ExperimentReason::TimeLimit);

    TraceBuffer airborne = record(original, 4.0);
    airborne.samples[0].after.on_ground = 0;
    check(evaluateGroundExperiment(original, info, airborne, result));
    check(result.outcome == ExperimentOutcome::Inconclusive);
    check(result.reason == ExperimentReason::LeftGround);

    TraceBuffer corrupt = record(original, 4.0);
    corrupt.samples[1].after.tick = 99;
    check(evaluateGroundExperiment(original, info, corrupt, result));
    check(result.reason == ExperimentReason::InvalidTrace);

    result.id = "preserved";
    invalid = original;
    invalid.verifiedStart = false;
    check(!evaluateGroundExperiment(invalid, info, success, result));
    check(result.id == "preserved");
    return 0;
  }
  catch (const std::exception &error) {
    std::fprintf(stderr, "experiment framework: %s\n", error.what());
    return 1;
  }
}
