#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>

#include <aipb/explorer/engine_bridge_math.h>

using namespace aipb::explorer::bridge;

static void require(bool condition) {
  if (!condition) {
    throw std::runtime_error("bridge-math assertion failed");
  }
}

static int runTests() {
  AipbExplorerBridgeCommand command { 1, 1, 45, 1, 0 };
  require(valid(command));
  float forward = 0.0f;
  float strafe = 0.0f;
  translateAxes(command, 250, forward, strafe);
  require(std::abs(std::hypot(forward, strafe) - 250.0f) < 0.01f);
  command.strafe = 0;
  translateAxes(command, 250, forward, strafe);
  require(forward == 250 && strafe == 0);
  require(yaw(170, 30) == -160);
  require(yaw(-170, -30) == 160);
  require(yaw(179, 1) == -180);
  require(initialViewYaw(-90.0f, 0.0f) == -90.0f);
  require(initialViewYaw(90.0f, -45.0f) == 90.0f);
  require(yaw(initialViewYaw(-90.0f, 0.0f), 0.0f) == -90.0f);
  require(yaw(initialViewYaw(-90.0f, 0.0f), 3.0f) == -87.0f);
  require(initialViewYaw(270.0f, 0.0f) == -90.0f);
  require(initialViewYaw(std::numeric_limits<float>::quiet_NaN(), 15.0f) == 15.0f);
  command.forward = std::numeric_limits<float>::infinity();
  require(!valid(command));
  command.forward = 0;
  command.jump = 2;
  require(!valid(command));
  command.jump = 0;
  command.yaw_delta = 200;
  require(!valid(command));

  ProbeBudget frames { 30, false };
  require(frames.valid());
  for (int n = 0; n < 30; ++n) {
    require(!frames.complete());
    frames.consume(2);
  }
  require(frames.complete() && frames.executed == 30 && frames.simulatedMs == 60);
  frames.consume(2);
  require(frames.executed == 30);

  ProbeBudget timed { 1000, true };
  require(timed.valid());
  for (int n = 0; n < 499; ++n) {
    timed.consume(2);
  }
  require(!timed.complete());
  timed.consume(2);
  require(timed.complete() && timed.executed == 500 && timed.simulatedMs == 1000);
  timed.consume(2);
  require(timed.executed == 500);

  ProbeBudget irregular { 400, true };
  irregular.consume(255);
  irregular.consume(150);
  require(irregular.complete() && irregular.simulatedMs == 405 && irregular.executed == 2);
  ProbeBudget invalidFrames { 129, false };
  ProbeBudget invalidMs { 2001, true };
  ProbeBudget zero { 0, true };
  ProbeBudget maxMs { 2000, true };
  require(!invalidFrames.valid() && !invalidMs.valid() && !zero.valid() && maxMs.valid());
  maxMs.consume(0);
  maxMs.consume(256);
  require(maxMs.executed == 0 && maxMs.simulatedMs == 0);

  return 0;
}

int main() {
  try {
    return runTests();
  } catch (const std::exception &error) {
    std::fprintf(stderr, "aipb_explorer_bridge_math: %s\n", error.what());
    return 1;
  }
}
