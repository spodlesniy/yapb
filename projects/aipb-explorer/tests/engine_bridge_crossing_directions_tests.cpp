#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>

#include <aipb/explorer/engine_bridge_crossing_directions.h>

using namespace aipb::explorer::bridge;

static void require(bool condition) {
  if (!condition) {
    throw std::runtime_error("crossing directions regression");
  }
}

static std::string row(int step, const char *kind,
  double sx, double sy, double sz, double ex, double ey, double ez, double yaw) {
  char out[768] {};
  const bool progress = std::string(kind) == "Progress";
  const int written = std::snprintf(out, sizeof(out),
    "%d,%d,%d,%s,%s,unknown,100,%.17g,%.17g,%.17g,"
    "%.17g,%.17g,%.17g,%.17g,25,5,5,0\n",
    step, step + 3, step, kind, progress ? "direct" : "confirming",
    sx, sy, sz, ex, ey, ez, yaw);
  require(written > 0 && static_cast<std::size_t>(written) < sizeof(out));
  return out;
}

static EvidenceTrial trial(const char *name, std::string first) {
  EvidenceTrial t {};
  t.name = name;
  t.botIndex = 0;
  t.sourceSteps = 100;
  t.rows.push_back(std::move(first));
  return t;
}

int main() {
  try {
    // Two directions on one face in the same requested-heading sector.
    EvidenceTrialSet both {};
    both.trials.push_back(trial("east",
      row(0, "Progress", 120, 8, 8, 136, 8, 8, 0)));
    both.trials.push_back(trial("west",
      row(4, "Progress", 140, 8, 8, 120, 8, 8, 0)));
    CrossingDirectionAudit audit {};
    require(auditCrossingDirections(both, 124, 8, 8, 0, audit));
    require(audit.discovered.faces == 1 && audit.discovered.progressWindows == 2);
    require(audit.positiveFaces == 1 && audit.negativeFaces == 1);
    require(audit.bidirectionalFaces == 1 && audit.againstHeadingFaces == 1);
    require(audit.againstHeadingWindows == 1);
    require(audit.details.size() == 1);
    const DirectedCrossingFace &face = audit.details[0];
    require(face.discovered.axis == 0 && face.discovered.lowerCell.x == 3);
    require(face.positiveProgress == 1 && face.negativeProgress == 1);
    require(face.positiveSources.size() == 1 && face.negativeSources.size() == 1);
    require(face.withHeading == 1 && face.againstHeading == 1);
    require(face.againstHeadingSources.size() == 1);

    // A reverse crossing can also align with a west-facing requested yaw.
    EvidenceTrialSet reverse {};
    reverse.trials.push_back(trial("reverse",
      row(0, "Progress", 140, 8, 8, 120, 8, 8, 180)));
    require(auditCrossingDirections(reverse, 124, 8, 8, 180, audit));
    require(audit.negativeFaces == 1 && audit.positiveFaces == 0);
    require(audit.details[0].withHeading == 1 && audit.againstHeadingWindows == 0);

    // Strafe-like movement across X is tangential to a north-facing yaw.
    EvidenceTrialSet transverse {};
    transverse.trials.push_back(trial("transverse",
      row(0, "Progress", 120, 8, 8, 136, 8, 8, 90)));
    require(auditCrossingDirections(transverse, 124, 8, 8, 90, audit));
    require(audit.transverseHeadingWindows == 1 && audit.againstHeadingWindows == 0);
    require(audit.details[0].transverseHeading == 1);

    // Requested yaw is horizontal; Z crossing must not be marked opposed.
    EvidenceTrialSet vertical {};
    vertical.trials.push_back(trial("vertical",
      row(0, "Progress", 8, 8, 120, 8, 8, 136, 0)));
    require(auditCrossingDirections(vertical, 8, 8, 124, 0, audit));
    require(audit.verticalAxisWindows == 1 && audit.againstHeadingWindows == 0);
    require(audit.details[0].discovered.axis == 2);

    // Restriction candidates retain their class and their travel sign.
    EvidenceTrialSet restriction {};
    restriction.trials.push_back(trial("restricted",
      row(0, "BlockedCandidate", 140, 8, 8, 120, 8, 8, 0)));
    require(auditCrossingDirections(restriction, 124, 8, 8, 0, audit));
    require(audit.details[0].negativeRestriction == 1);
    require(audit.details[0].negativeProgress == 0);
    require(audit.againstHeadingWindows == 1);

    // One long source window crosses two faces but is one record.
    EvidenceTrialSet longTrace {};
    longTrace.trials.push_back(trial("long",
      row(0, "Progress", 100, 8, 8, 300, 8, 8, 180)));
    require(auditCrossingDirections(longTrace, 150, 8, 8, 180, audit));
    require(audit.discovered.faces == 2 && audit.discovered.progressWindows == 1);
    require(audit.positiveFaces == 2 && audit.againstHeadingFaces == 2);
    require(audit.againstHeadingWindows == 1);

    // No crossing, invalid coordinates and bin mismatch stay conservative.
    require(auditCrossingDirections(both, 124, 8, 8, 90, audit));
    require(audit.discovered.faces == 0 && audit.details.empty());
    audit.positiveFaces = 77;
    require(!auditCrossingDirections(both, 1e40, 8, 8, 0, audit));
    require(!auditCrossingDirections(both, 124, 8, 8, std::nan(""), audit));
    require(audit.positiveFaces == 77);
    return 0;
  }
  catch (const std::exception &error) {
    std::fprintf(stderr, "crossing directions: %s\n", error.what());
    return 1;
  }
}
