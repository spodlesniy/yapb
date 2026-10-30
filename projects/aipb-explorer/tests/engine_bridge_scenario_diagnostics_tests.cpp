#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include <aipb/explorer/engine_bridge_scenario_trace.h>

using namespace aipb::explorer::scenario;

static void check(bool ok) {
  if (!ok) {
    throw std::runtime_error("scenario diagnostics contract failed");
  }
}

int main() {
  namespace fs = std::filesystem;
  try {
    ScenarioStallDetector detector {};
    ScenarioResultPose still {};
    for (int i = 0; i < 19; ++i) {
      check(detector.observe(10, 250, 0, still, still) == ScenarioProgressEvent::None);
    }
    check(detector.observe(10, 250, 0, still, still) == ScenarioProgressEvent::Stall);
    check(detector.observe(10, 250, 0, still, still) == ScenarioProgressEvent::None);
    ScenarioResultPose moving = still;
    moving.position[0] = 1.0;
    moving.velocity[0] = 200;
    check(detector.observe(10, 250, 0, still, moving) == ScenarioProgressEvent::Recovered);
    check(detector.observe(10, 250, 0, moving, moving) == ScenarioProgressEvent::None);
    check(detector.observe(10, 0, 0, moving, moving) == ScenarioProgressEvent::None);
    for (int i = 0; i < 20; ++i) {
      const auto event = detector.observe(10, 250, 0, moving, moving);
      check(event == (i == 19 ? ScenarioProgressEvent::Stall : ScenarioProgressEvent::None));
    }
    detector.clear();
    check(detector.observe(10, 250, 0, still, still) == ScenarioProgressEvent::None);
    for (int i = 0; i < 30; ++i) {
      ScenarioResultPose next = still;
      next.position[0] = 0.2;
      next.velocity[0] = 1;
      check(detector.observe(10, 250, 0, still, next) == ScenarioProgressEvent::None);
    }

    ScenarioDiagnostics diagnostics {};
    ScenarioDiagnosticSample sample {};
    sample.kind = ScenarioDiagnosticKind::Start;
    sample.gameTime = 20;
    sample.roundStartTime = 10;
    sample.freezeTime = 6;
    sample.flags = 64;
    sample.maxSpeed = 250;
    check(diagnostics.append(sample));
    sample.kind = ScenarioDiagnosticKind::Stall;
    sample.frame = 20;
    sample.action = 0;
    sample.commandMs = 200;
    sample.forwardSpeed = 250;
    sample.obstacleFraction = 0.75;
    sample.obstacleStartSolid = false;
    check(diagnostics.append(sample));
    char csv[1024] {};
    check(formatScenarioDiagnosticSample(csv, sizeof(csv), sample));
    check(std::string(csv).find("stall,20,0,200,") == 0);
    check(std::string(csv).find(",0.75,0,0\n") != std::string::npos);
    check(!formatScenarioDiagnosticSample(csv, 4, sample));
    sample.friction = 1;
    for (std::size_t i = diagnostics.samples().size(); i < ScenarioDiagnostics::kMaxSamples; ++i) {
      check(diagnostics.append(sample));
    }
    check(!diagnostics.append(sample));
    diagnostics.clear();
    check(diagnostics.samples().empty());
    sample.frame = -1;
    sample.action = -1;
    sample.kind = ScenarioDiagnosticKind::Start;
    check(diagnostics.append(sample));

    ScenarioTrace trace {};
    ScenarioTraceFrame frame {};
    frame.frameIndex = 0;
    frame.actionIndex = 0;
    frame.milliseconds = 10;
    frame.maxSpeed = 250;
    check(trace.append(frame));
    ScenarioResult result {};
    result.name = "ground_accel_coast_01";
    result.map = "de_dust2";
    result.botIndex = 1;
    result.team = 2;
    result.status = ActionScheduleStatus::Completed;
    result.reason = "finished";
    result.frames = 1;
    result.commandMs = 10;
    result.startGameMs = 20000;
    result.endGameMs = 21000;
    result.endSource = ScenarioEndSource::Live;
    const fs::path root = "aipb_scenario_diagnostics_test_tmp";
    std::error_code ec {};
    fs::remove_all(root, ec);
    std::string location {};
    check(saveScenarioRunBundle(root, "20261030_120000", result, trace, location, &diagnostics));
    check(fs::exists(fs::path(location) / "diagnostics.csv"));
    check(fs::exists(fs::path(location) / "result.csv"));
    std::ifstream in(fs::path(location) / "diagnostics.csv");
    std::string line {};
    check(static_cast<bool>(std::getline(in, line)) && line == kScenarioDiagnosticsVersion);
    check(static_cast<bool>(std::getline(in, line)) && line.find("kind,frame,") == 0);
    check(static_cast<bool>(std::getline(in, line)) && line.find("start,") == 0);
    check(!static_cast<bool>(std::getline(in, line)));
    in.close();
    std::string second {};
    check(saveScenarioRunBundle(root, "20261030_120000", result, trace, second));
    check(!fs::exists(fs::path(second) / "diagnostics.csv"));
    check(fs::exists(fs::path(second) / "result.csv"));
    fs::remove_all(root, ec);
    return 0;
  }
  catch (const std::exception &e) {
    std::fprintf(stderr, "scenario diagnostics: %s\n", e.what());
    return 1;
  }
}
