#include <cstdio>
#include <stdexcept>

#include <aipb/explorer/goldsrc_environment.h>

using namespace aipb::explorer;

static void require(bool condition) {
  if (!condition) {
    throw std::runtime_error("GoldSrc bridge assertion failed");
  }
}

struct FakeEngine {
  AipbExplorerBridgeState state {};
  int actions {};
  bool rejects {};
  bool corrupt {};
};

static int resetEngine(void *ctx, const AipbExplorerBridgeState *start, AipbExplorerBridgeState *out) {
  auto &engine = *static_cast<FakeEngine *>(ctx);
  engine.state = *start;
  *out = engine.state;
  return 1;
}

static int stepEngine(void *ctx, const AipbExplorerBridgeCommand *cmd, AipbExplorerBridgeStep *out) {
  auto &engine = *static_cast<FakeEngine *>(ctx);
  ++engine.actions;
  if (engine.rejects) {
    return 0;
  }
  engine.state.x += cmd->forward;
  engine.state.y += cmd->strafe;
  engine.state.yaw += cmd->yaw_delta;
  engine.state.crouching = cmd->duck;
  ++engine.state.tick;
  *out = { engine.state, static_cast<uint8_t>(StepStatus::Applied), 0, 0 };
  if (engine.corrupt) {
    --out->state.tick;
  }
  return 1;
}

static int runTests() {
  FakeEngine engine;
  const AipbExplorerBridgeApi api { &engine, &resetEngine, &stepEngine, 0.01 };
  GoldSrcEnvironment environment(api);
  require(environment.tickSeconds() == 0.01);
  const MovementState start {};
  require(environment.reset(start) == start);
  const auto first = environment.step(MovementCommand { .forward = 1.0F });
  require(first.status == StepStatus::Applied && first.state.position.x == 1.0 && first.state.tick == 1);
  require(engine.actions == 1);
  const auto invalid = environment.step(MovementCommand { .forward = 2.0F });
  require(invalid.status == StepStatus::Invalid && invalid.state == first.state && engine.actions == 1);
  engine.rejects = true;
  const auto rejected = environment.step(MovementCommand {});
  require(rejected.status == StepStatus::Invalid && rejected.state == first.state);
  engine.rejects = false;
  engine.corrupt = true;
  bool caught = false;
  try {
    (void)environment.step(MovementCommand {});
  } catch (const std::runtime_error &) {
    caught = true;
  }
  require(caught);
  caught = false;
  try {
    (void)GoldSrcEnvironment(AipbExplorerBridgeApi { &engine, nullptr, nullptr, 0.01 });
  } catch (const std::invalid_argument &) {
    caught = true;
  }
  require(caught);
  caught = false;
  try {
    const AipbExplorerBridgeApi noReset { &engine, nullptr, &stepEngine, 0.01 };
    GoldSrcEnvironment withoutReset(noReset);
    (void)withoutReset.reset(start);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  require(caught);
  return 0;
}

int main() {
  try {
    return runTests();
  } catch (const std::exception &error) {
    std::fprintf(stderr, "aipb_explorer_goldsrc_environment: %s\n", error.what());
    return 1;
  }
}
