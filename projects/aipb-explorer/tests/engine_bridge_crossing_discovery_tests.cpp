#include <cstdio>
#include <stdexcept>
#include <string>

#include <aipb/explorer/engine_bridge_crossing_discovery.h>

using namespace aipb::explorer::bridge;

static void check(bool valid) {
  if (!valid) {
    throw std::runtime_error("crossing discovery regression");
  }
}

static std::string row(int step, double a, double b, double y = 8) {
  char line[768] {};
  std::snprintf(line, sizeof(line),
    "%d,%d,%d,Progress,direct,unknown,100,%.17g,%.17g,8,%.17g,%.17g,8,0,25,5,5,0\n",
    step, step + 3, step, a, y, b, y);
  return line;
}

static EvidenceTrial trial(const char *name, std::string first, std::string second) {
  EvidenceTrial t {};
  t.name = name;
  t.botIndex = 0;
  t.sourceSteps = 100;
  t.rows = { first, second };
  return t;
}

int main() {
  try {
    EvidenceTrialSet data {};
    data.trials.push_back(trial("a", row(0, 120, 136), row(4, 138, 155)));
    data.trials.push_back(trial("b", row(0, 116, 140), row(4, 120, 128)));
    CrossingDiscovery result {};
    check(discoverSourceCrossings(data, 124, 8, 8, 0, result));
    check(result.faces == 1 && result.progressFaces == 1);
    check(result.progressWindows == 2 && result.progressSources == 2);
    check(result.multipleProgressSourceFaces == 1);
    check(result.details[0].axis == 0 && result.details[0].lowerCell.x == 3);
    check(result.details[0].position[0] == 128);

    EvidenceTrialSet touching {};
    touching.trials.push_back(trial("touch", row(0, 120, 128), row(4, 128, 150)));
    check(discoverSourceCrossings(touching, 124, 8, 8, 0, result));
    check(result.faces == 0);
    check(discoverSourceCrossings(data, 124, 8, 8, 90, result));
    check(result.faces == 0);
    result.faces = 99;
    check(!discoverSourceCrossings(data, 1e40, 8, 8, 0, result));
    check(result.faces == 99);
    return 0;
  }
  catch (const std::exception &e) {
    std::fprintf(stderr, "crossing discovery: %s\n", e.what());
    return 1;
  }
}
