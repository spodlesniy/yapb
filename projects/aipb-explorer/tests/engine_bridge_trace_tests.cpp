#include <cstdio>
#include <limits>
#include <utility>
#include <stdexcept>

#include <aipb/explorer/engine_bridge_trace.h>

using namespace aipb::explorer::bridge;

static void require(bool ok) {
  if (!ok) {
    throw std::runtime_error("trace contract failed");
  }
}

static int runTests() {
  static TraceBuffer trace {};
  // An empty global trace must not reserve a 4096-step array in the DLL.
  static_assert(sizeof(TraceBuffer) < 128, "TraceBuffer descriptor must remain small");
  require(!trace.samples && trace.count == 0);
  trace.botIndex = 1;
  AipbExplorerBridgeState before {};
  before.yaw = -179.0f;
  before.on_ground = 1;
  AipbExplorerBridgeState after = before;
  after.tick = 1;
  after.yaw = -178.0f;
  after.x = 1.0;
  TraceSample sample {};
  sample.command.forward = 1.0f;
  sample.before = before;
  sample.after = after;
  sample.maxSpeed = 250.0f;
  sample.msec = 2;
  require(trace.append(sample));
  require(trace.count == 1 && trace.samples != nullptr);
  require(!trace.append(sample));
  sample.before = after;
  sample.after = after;
  sample.after.tick++;
  sample.after.x++;
  require(trace.append(sample));
  trace.completed = true;
  require(!trace.append(sample));
  // An explicit clear invalidates the completed in-memory recording.
  trace.clear();
  require(trace.count == 0 && !trace.completed && trace.botIndex == -1 && !trace.samples);
  sample.command.jump = 2;
  require(!trace.append(sample));
  sample.command.jump = 0;
  sample.msec = 0;
  require(!trace.append(sample));
  // Once cleared, a fresh recording is accepted without old steps.
  sample.msec = 2;
  require(trace.append(sample) && trace.count == 1 && !trace.completed);
  trace.clear();
  require(trace.count == 0 && trace.botIndex == -1 && !trace.completed && !trace.samples);

  // Move a completed trace as load() does: the sample allocation is transferred.
  TraceBuffer staged {};
  staged.botIndex = 2;
  sample.before = before;
  sample.after = after;
  sample.after.tick = before.tick + 1;
  require(staged.append(sample));
  staged.completed = true;
  TraceSample *const allocation = staged.samples.get();
  trace = std::move(staged);
  staged.clear();
  require(trace.completed && trace.count == 1 && trace.botIndex == 2);
  require(trace.samples.get() == allocation && !staged.samples && staged.count == 0);
  trace.clear();
  require(!trace.samples && trace.count == 0);

  // A heap-backed recording still enforces the exact 4096-step hard limit.
  TraceBuffer full {};
  TraceSample step {};
  step.msec = 2;
  step.maxSpeed = 250.0f;
  for (int i = 0; i < ProbeBudget::kMaxSafetyFrames; ++i) {
    step.before.tick = static_cast<uint64_t>(i);
    step.after.tick = static_cast<uint64_t>(i + 1);
    require(full.append(step));
  }
  require(full.count == ProbeBudget::kMaxSafetyFrames && !full.append(step));
  full.clear();
  require(!full.samples && full.count == 0);

  // A completed trace can be extended only from the exact observed endpoint.
  TraceBuffer joined {};
  joined.botIndex = 1;
  TraceSample first {};
  first.command.forward = 1.0f;
  first.maxSpeed = 250.0f;
  first.msec = 2;
  first.before = before;
  first.after = before;
  first.after.tick = 1;
  first.after.x = 4.0;
  require(joined.append(first));
  joined.completed = true;
  require(canAppendTrace(joined, 1, first.after));
  require(!canAppendTrace(joined, 2, first.after));
  auto changed = first.after;
  changed.x += 1.0;
  require(!canAppendTrace(joined, 1, changed));
  changed = first.after;
  changed.vy += 2.0;
  require(!canAppendTrace(joined, 1, changed));
  changed = first.after;
  changed.crouching = 1;
  require(!canAppendTrace(joined, 1, changed));
  changed = first.after;
  changed.yaw = -90.0f;
  require(!canAppendTrace(joined, 1, changed));
  joined.completed = false;
  TraceSample second = first;
  second.before = first.after;
  second.after = second.before;
  second.after.tick = 2;
  second.after.x += 3.0;
  require(joined.append(second));
  require(joined.count == 2 && joined.samples[1].before.tick == 1);
  joined.completed = true;
  require(canAppendTrace(joined, 1, second.after));
  joined.clear();
  require(!canAppendTrace(joined, 1, second.after));

  // Replay input demand is capped at the recorded movement scale, not peak momentum.
  TraceBuffer speedTrace {};
  TraceSample move {};
  move.maxSpeed = 240.0f;
  move.msec = 2;
  move.command.forward = 1.0f;
  move.before = before;
  move.after = move.before;
  move.after.tick = 1;
  move.after.vx = 270.0;
  require(speedTrace.append(move));
  TraceSample diagonal = move;
  diagonal.before = move.after;
  diagonal.after = diagonal.before;
  diagonal.after.tick = 2;
  diagonal.command.strafe = 1.0f;
  diagonal.maxSpeed = 250.0f;
  diagonal.after.vx = 310.0;
  require(speedTrace.append(diagonal));
  TraceSample partial = diagonal;
  partial.before = diagonal.after;
  partial.after = partial.before;
  partial.after.tick = 3;
  partial.command.forward = 0.5f;
  partial.command.strafe = 0.0f;
  partial.maxSpeed = 300.0f;
  partial.after.vx = 280.0;
  require(speedTrace.append(partial));
  ReplaySpeedRequirement req {};
  require(!replaySpeedRequirement(speedTrace, req));
  speedTrace.completed = true;
  require(replaySpeedRequirement(speedTrace, req));
  require(req.maximumRequested == 250.0f && req.requestedStep == 1);
  require(req.peakObservedHorizontal == 310.0);
  require(requestedHorizontalSpeed(partial) == 150.0f);
  require(!(req.maximumRequested <= 230.0f));
  require(req.maximumRequested <= 250.0f);
  speedTrace.samples[1].after.vx = std::numeric_limits<double>::quiet_NaN();
  req.requestedStep = 999;
  require(!replaySpeedRequirement(speedTrace, req) && req.requestedStep == 999);
  speedTrace.clear();

  const ReplayDifference same = compareStates(before, before);
  require(same.within(0.01, 0.01, 0.01f));
  AipbExplorerBridgeState rotated = before;
  rotated.yaw = 179.0f;
  require(compareStates(before, rotated).yawDegrees == 2.0f);
  require(!compareStates(before, rotated).within(1.0, 1.0, 1.0f));
  rotated = before;
  rotated.x = 100.0;
  require(!compareStates(before, rotated).within(5.0, 10.0, 3.0f));
  rotated = before;
  rotated.crouching = 1;
  require(!compareStates(before, rotated).within(5.0, 10.0, 3.0f));
  rotated = before;
  rotated.x = 0.01;
  require(compareStates(before, rotated).within(5.0, 10.0, 3.0f));
  rotated.x = std::numeric_limits<double>::quiet_NaN();
  require(!compareStates(before, rotated).within(5.0, 10.0, 3.0f));
  return 0;
}

int main() {
  try {
    return runTests();
  } catch (const std::exception &error) {
    std::fprintf(stderr, "aipb_explorer_trace: %s\n", error.what());
    return 1;
  }
}
