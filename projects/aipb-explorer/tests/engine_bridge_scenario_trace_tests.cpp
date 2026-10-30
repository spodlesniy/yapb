#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include <aipb/explorer/engine_bridge_scenario_trace.h>

using namespace aipb::explorer::scenario;

static void check(bool ok) {
  if (!ok) {
    throw std::runtime_error("scenario trace contract failed");
  }
}

int main() {
  namespace fs = std::filesystem;
  try {
    ScenarioTrace trace {};
    ScenarioTraceFrame frame {};
    frame.frameIndex = 0;
    frame.actionIndex = 0;
    frame.milliseconds = 10;
    frame.maxSpeed = 250;
    frame.forwardSpeed = 250;
    frame.before.position[0] = 448;
    frame.after.position[0] = 449;
    check(trace.append(frame));
    check(!trace.append(frame));
    frame.frameIndex = 2;
    check(!trace.append(frame));
    frame.frameIndex = 1;
    frame.actionIndex = 1;
    frame.before = trace.frames().back().after;
    frame.after.position[0] = 451;
    check(trace.append(frame));
    check(trace.size() == 2);
    char line[1024] {};
    check(formatScenarioTraceFrame(line, sizeof(line), frame));
    check(std::string(line).find("1,1,10,250,0,250,") == 0);
    check(!formatScenarioTraceFrame(line, 4, frame));
    frame.frameIndex = 2;
    frame.actionIndex = 0;
    check(!trace.append(frame));
    ScenarioResult result {};
    result.name = "ground_forward_02";
    result.map = "de_dust2";
    result.botIndex = 1;
    result.team = 2;
    result.status = ActionScheduleStatus::Completed;
    result.reason = "finished";
    result.frames = 2;
    result.commandMs = 20;
    result.startGameMs = 1;
    result.endGameMs = 2;
    result.endSource = ScenarioEndSource::Live;
    const fs::path root = "aipb_scenario_trace_test_tmp";
    std::error_code ec {};
    fs::remove_all(root, ec);
    std::string location {};
    check(saveScenarioRunBundle(root, "20261029_232410", result, trace, location));
    check(fs::exists(fs::path(location) / "trace.csv"));
    check(fs::exists(fs::path(location) / "result.csv"));
    std::ifstream in(fs::path(location) / "trace.csv");
    std::string line1 {}, line2 {}, line3 {}, line4 {};
    check(static_cast<bool>(std::getline(in, line1)));
    check(static_cast<bool>(std::getline(in, line2)));
    check(static_cast<bool>(std::getline(in, line3)));
    check(static_cast<bool>(std::getline(in, line4)));
    check(line1 == kScenarioTraceVersion);
    check(line2.find("frame,action,msec") == 0);
    check(line3.find("0,0,10") == 0 && line4.find("1,1,10") == 0);
    check(!static_cast<bool>(std::getline(in, line1)));
    in.close();
    std::string second {};
    check(saveScenarioRunBundle(root, "20261029_232410", result, trace, second));
    check(location != second);
    check(fs::path(second).filename() == "20261029_232410_001");
    check(!saveScenarioRunBundle(root, "../unsafe", result, trace, second));
    result.reason = "invalid,reason";
    check(!saveScenarioRunBundle(root, "20261029_232410", result, trace, second));
    trace.clear();
    check(trace.size() == 0 && trace.frames().empty());
    frame.frameIndex = 0;
    frame.actionIndex = 0;
    check(trace.append(frame));
    check(trace.size() == 1);
    fs::remove_all(root, ec);
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "scenario trace: %s\n", e.what());
    return 1;
  }
}
