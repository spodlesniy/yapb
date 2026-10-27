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
  command.forward = std::numeric_limits<float>::infinity();
  require(!valid(command));
  command.forward = 0;
  command.jump = 2;
  require(!valid(command));
  command.jump = 0;
  command.yaw_delta = 200;
  require(!valid(command));
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
