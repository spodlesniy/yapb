#include <cstdio>
#include <stdexcept>
#include <string>

#include <aipb/explorer/engine_bridge_boundary_crossings.h>

using namespace aipb::explorer::bridge;

static void check(bool value) {
  if (!value) {
    throw std::runtime_error("boundary crossing regression");
  }
}

static std::string row(int step, double x, double end, double y = 8) {
  char buffer[768] {};
  std::snprintf(buffer, sizeof(buffer),
    "%d,%d,%d,Progress,direct,unknown,100,%.17g,%.17g,8,%.17g,%.17g,8,0,25,5,5,0\n",
    step, step + 3, step, x, y, end, y);
  return buffer;
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
    EvidenceTrialSet set {};
    set.trials.push_back(trial("a", row(0, 120, 136), row(4, 138, 152)));
    set.trials.push_back(trial("b", row(0, 116, 140), row(4, 142, 155)));
    BoundaryCrossingAudit result {};
    check(auditCoarseBoundaryCrossings(set, 124, 8, 8, 0, result));
    check(result.span.coarseCells == 2 && result.coarseBoundaryFaces == 1);
    check(result.facesWithProgress == 1 && result.facesWithMultipleProgressSources == 1);
    check(result.progressCrossingWindows == 2 && result.progressCrossingSources == 2);
    check(result.unsupportedFaces == 0);

    EvidenceTrialSet split {};
    split.trials.push_back(trial("a", row(0, 116, 124), row(4, 132, 140)));
    split.trials.push_back(trial("b", row(0, 118, 123), row(4, 135, 143)));
    check(auditCoarseBoundaryCrossings(split, 120, 8, 8, 0, result));
    check(result.coarseBoundaryFaces == 1 && result.facesWithAnyCrossing == 0);
    check(result.unsupportedFaces == 1);

    EvidenceTrialSet touching {};
    touching.trials.push_back(trial("a", row(0, 120, 128), row(4, 128, 140)));
    touching.trials.push_back(trial("b", row(0, 116, 128), row(4, 128, 144)));
    check(auditCoarseBoundaryCrossings(touching, 124, 8, 8, 0, result));
    check(result.coarseBoundaryFaces == 1 && result.progressCrossingWindows == 0);

    SpatialRecord record {};
    record.start[0] = 120;
    record.end[0] = 136;
    record.start[1] = record.end[1] = 32;
    record.start[2] = record.end[2] = 8;
    SpatialCell cell {};
    check(!crossingAtFace(record, 0, 1, cell));

    result.coarseBoundaryFaces = 77;
    check(!auditCoarseBoundaryCrossings(set, 1e40, 0, 0, 0, result));
    check(result.coarseBoundaryFaces == 77);
    return 0;
  }
  catch (const std::exception &error) {
    std::fprintf(stderr, "boundary crossing: %s\n", error.what());
    return 1;
  }
}
