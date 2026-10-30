#include <cstdio>
#include <limits>
#include <stdexcept>

#include <aipb/explorer/engine_bridge_scenario_lifecycle.h>

using namespace aipb::explorer::scenario;

static void expect(bool value) {
  if (!value) {
    throw std::runtime_error("scenario lifecycle failure");
  }
}

int main() {
  try {
    // CS roundStartTime denotes the end of the engine freeze period.
    expect(scenarioRoundIsFrozen(100.0f, 106.0f));
    expect(scenarioRoundIsFrozen(105.999f, 106.0f));
    expect(!scenarioRoundIsFrozen(106.0f, 106.0f));
    expect(!scenarioRoundIsFrozen(107.0f, 106.0f));
    expect(!scenarioRoundIsFrozen(100.0f, 0.0f));
    expect(!scenarioRoundIsFrozen(-1.0f, 106.0f));
    expect(!scenarioRoundIsFrozen(100.0f, std::numeric_limits<float>::infinity()));
    RunLifecycleObservation o {};
    o.roundId = 8;
    o.map = "de_dust2";
    o.botPresent = true;
    o.botSame = true;
    o.botAlive = true;
    o.team = 2;
    o.speedValid = true;
    expect(checkRunLifecycle(o, "de_dust2", 8, 2) == RunLifecycleFailure::None);
    o.roundOver = true;
    expect(checkRunLifecycle(o, "de_dust2", 8, 2) == RunLifecycleFailure::RoundOver);
    o.roundOver = false;
    o.roundId = 9;
    expect(checkRunLifecycle(o, "de_dust2", 8, 2) == RunLifecycleFailure::RoundChanged);
    o.roundId = 8;
    o.map = "de_inferno";
    expect(checkRunLifecycle(o, "de_dust2", 8, 2) == RunLifecycleFailure::MapChanged);
    o.map = "de_dust2";
    o.botPresent = false;
    expect(checkRunLifecycle(o, "de_dust2", 8, 2) == RunLifecycleFailure::BotMissing);
    o.botPresent = true;
    o.botSame = false;
    expect(checkRunLifecycle(o, "de_dust2", 8, 2) == RunLifecycleFailure::BotReplaced);
    o.botSame = true;
    o.botAlive = false;
    expect(checkRunLifecycle(o, "de_dust2", 8, 2) == RunLifecycleFailure::BotDead);
    o.botAlive = true;
    o.team = 1;
    expect(checkRunLifecycle(o, "de_dust2", 8, 2) == RunLifecycleFailure::TeamChanged);
    o.team = 2;
    o.speedValid = false;
    expect(checkRunLifecycle(o, "de_dust2", 8, 2) == RunLifecycleFailure::SpeedInvalid);
    o.speedValid = true;
    o.explorerBusy = true;
    expect(checkRunLifecycle(o, "de_dust2", 8, 2) == RunLifecycleFailure::ExplorerConflict);
    return 0;
  }
  catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
