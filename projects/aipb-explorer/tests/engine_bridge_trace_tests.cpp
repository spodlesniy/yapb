#include <cstdio>
#include <limits>
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
  require(trace.count == 1);
  require(!trace.append(sample));
  sample.before = after;
  sample.after = after;
  sample.after.tick++;
  sample.after.x++;
  require(trace.append(sample));
  trace.completed = true;
  require(!trace.append(sample));
  trace.clear();
  require(trace.count == 0 && !trace.completed && trace.botIndex == -1);
  sample.command.jump = 2;
  require(!trace.append(sample));
  sample.command.jump = 0;
  sample.msec = 0;
  require(!trace.append(sample));

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
