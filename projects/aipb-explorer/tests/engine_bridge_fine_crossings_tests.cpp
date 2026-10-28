#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

#include <aipb/explorer/engine_bridge_fine_crossings.h>

using namespace aipb::explorer::bridge;

static void require(bool ok) {
  if (!ok) {
    throw std::runtime_error("fine crossing regression");
  }
}

static std::string row(
  int step,
  const char *classification,
  double sx,
  double sy,
  double sz,
  double ex,
  double ey,
  double ez,
  double heading = 0
) {
  char out[768] {};
  const int written = std::snprintf(out, sizeof(out),
    "%d,%d,%d,%s,%s,unknown,100,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,25,5,5,0\n",
    step, step + 3, step, classification,
    std::string(classification) == "Progress" ? "direct" : "confirming",
    sx, sy, sz, ex, ey, ez, heading);
  require(written > 0 && static_cast<std::size_t>(written) < sizeof(out));
  return out;
}

static EvidenceTrial trial(const char *name, const std::string &data) {
  EvidenceTrial source {};
  source.name = name;
  source.botIndex = 0;
  source.sourceSteps = 100;
  source.rows.push_back(data);
  return source;
}

int main() {
  try {
    EvidenceTrialSet sources {};
    sources.trials.push_back(trial("a", row(0, "Progress", 8, 8, 8, 112, 8, 8)));
    FineCrossingDiscovery audit {};
    require(discoverFineCrossings(sources, 60, 8, 8, 0, audit));
    require(audit.faces == 3 && audit.interiorFaces == 3 && audit.coarseFaces == 0);
    require(audit.progressWindows == 1 && audit.progressSources == 1);
    require(audit.details[0].positiveProgress == 1);
    require(audit.details[0].position[0] == 64);
    require(audit.details[0].positiveSources.size() == 1);
    sources.trials.push_back(trial("b", row(0, "BlockedCandidate", 112, 8, 8, 8, 8, 8)));
    require(discoverFineCrossings(sources, 60, 8, 8, 0, audit));
    require(audit.faces == 3 && audit.blockedWindows == 1);
    require(audit.details[0].negativeBlocked == 1);
    require(audit.details[0].negativeSources.size() == 1);
    require(audit.restrictionSources == 1);

    // A crossing of the 128-unit plane is counted once at fine resolution.
    EvidenceTrialSet coarse {};
    coarse.trials.push_back(trial("coarse", row(0, "Progress", 120, 8, 8, 136, 8, 8)));
    require(discoverFineCrossings(coarse, 124, 8, 8, 0, audit));
    require(audit.faces == 1 && audit.coarseFaces == 1 && audit.interiorFaces == 0);
    require(audit.details[0].lowerCell.x == 3 && audit.details[0].position[0] == 128);

    // Endpoint-only contact, face-edge intersection and tangency are excluded.
    EvidenceTrialSet touching {};
    touching.trials.push_back(trial("endpoint", row(0, "Progress", 24, 8, 8, 32, 8, 8)));
    touching.trials.push_back(trial("edge", row(0, "Progress", 24, 0, 8, 40, 0, 8)));
    touching.trials.push_back(trial("plane", row(0, "Progress", 32, 8, 8, 32, 24, 8)));
    require(discoverFineCrossings(touching, 16, 8, 8, 0, audit));
    require(audit.faces == 0);

    // Negative coordinates and axis Y/Z strict crossings.
    EvidenceTrialSet negative {};
    negative.trials.push_back(trial("negative", row(0, "Progress", -40, -8, -8, -24, -8, -8)));
    require(discoverFineCrossings(negative, -20, -8, -8, 0, audit));
    require(audit.faces == 1 && audit.details[0].lowerCell.x == -2);
    EvidenceTrialSet other {};
    other.trials.push_back(trial("y", row(0, "Progress", 8, 24, 8, 8, 40, 8)));
    other.trials.push_back(trial("z", row(0, "DeflectedCandidate", 8, 8, 24, 8, 8, 40)));
    require(discoverFineCrossings(other, 8, 8, 8, 0, audit));
    require(audit.faces == 2 && audit.deflectedWindows == 1);

    require(discoverFineCrossings(sources, 60, 8, 8, 90, audit));
    require(audit.faces == 0);
    audit.faces = 77;
    require(!discoverFineCrossings(sources, 1e40, 8, 8, 0, audit));
    require(!discoverFineCrossings(sources, 60, 8, 8, std::nan(""), audit));
    require(audit.faces == 77);
    return 0;
  }
  catch (const std::exception &error) {
    std::fprintf(stderr, "fine crossings: %s\n", error.what());
    return 1;
  }
}
