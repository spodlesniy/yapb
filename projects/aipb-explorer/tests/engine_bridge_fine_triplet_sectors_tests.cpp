#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

#include <aipb/explorer/engine_bridge_fine_triplet_sectors.h>

using namespace aipb::explorer::bridge;

static void require(bool condition) {
  if (!condition) {
    throw std::runtime_error("fine triplet sectors regression");
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
  double yaw
) {
  char buffer[768] {};
  const int count = std::snprintf(buffer, sizeof(buffer),
    "%d,%d,%d,%s,%s,unknown,100,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,25,5,5,0\n",
    step, step + 3, step, classification,
    std::string(classification) == "Progress" ? "direct" : "confirming",
    sx, sy, sz, ex, ey, ez, yaw);
  require(count > 0 && static_cast<std::size_t>(count) < sizeof(buffer));
  return buffer;
}

static EvidenceTrial trial(const char *name) {
  EvidenceTrial result {};
  result.name = name;
  result.botIndex = 0;
  result.sourceSteps = 100;
  return result;
}

static void appendX(
  EvidenceTrial &source,
  int step,
  const char *classification,
  double yaw,
  bool reverse = false
) {
  source.rows.push_back(row(step, classification,
    reverse ? 112.0 : 8.0, 8, 8,
    reverse ? 8.0 : 112.0, 8, 8, yaw));
}

int main() {
  try {
    EvidenceTrialSet sources {};
    EvidenceTrial first = trial("first");
    appendX(first, 0, "Progress", 0);
    appendX(first, 4, "Progress", 90);
    appendX(first, 8, "Progress", 180);
    sources.trials.push_back(first);

    FineTripletAudit audit {};
    require(auditFineTripletSectors(sources, 60, 8, 8, audit));
    require(audit.sources == 1 && audit.chains == 3);
    require(audit.tripletWindows == 3 && audit.distinctTriplets == 1);
    require(audit.observedSources == 1 && audit.multipleSectorTriplets == 1);
    require(audit.progressEvidenceTriplets == 0 && audit.insufficientTriplets == 1);
    require(audit.details[0].outcome.progressBins == 0x15u);
    require(audit.details[0].outcome.evidence.strongProgressSources == 1);
    require(audit.details[0].triplet.first.lowerCell.x == 0);
    require(audit.details[0].triplet.second.lowerCell.x == 1);
    require(audit.details[0].triplet.third.lowerCell.x == 2);
    require(audit.details[0].triplet.first.positive);

    // Repeated windows and yaw sectors from one trial count as one source.
    EvidenceTrial second = trial("second");
    appendX(second, 0, "Progress", 270);
    appendX(second, 4, "Progress", 315);
    sources.trials.push_back(second);
    require(auditFineTripletSectors(sources, 60, 8, 8, audit));
    require(audit.progressEvidenceTriplets == 1);
    require(audit.details[0].outcome.evidence.strongProgressSources == 2);
    require(audit.details[0].outcome.evidence.observedSources == 2);

    // Reversed directed triplets are separate keys, even with identical yaw.
    EvidenceTrial reversed = trial("reversed");
    appendX(reversed, 0, "BlockedCandidate", 0, true);
    appendX(reversed, 4, "DeflectedCandidate", 90, true);
    sources.trials.push_back(reversed);
    require(auditFineTripletSectors(sources, 60, 8, 8, audit));
    require(audit.distinctTriplets == 2);
    require(audit.progressEvidenceTriplets == 1);
    require(audit.insufficientTriplets == 1);
    require(audit.crossSectorConflictTriplets == 0);

    // Weak opposite evidence in another sector makes the forward key conflicted.
    EvidenceTrial opposed = trial("opposed");
    appendX(opposed, 0, "DeflectedCandidate", 45);
    sources.trials.push_back(opposed);
    require(auditFineTripletSectors(sources, 60, 8, 8, audit));
    require(audit.conflictedTriplets == 1);
    require(audit.crossSectorConflictTriplets == 1);
    require(audit.progressEvidenceTriplets == 0);

    // A single source containing conflicting classifications is still one vote.
    EvidenceTrialSet mixed {};
    EvidenceTrial mixedSource = trial("mixed");
    appendX(mixedSource, 0, "Progress", 0);
    appendX(mixedSource, 4, "BlockedCandidate", 90);
    mixed.trials.push_back(mixedSource);
    require(auditFineTripletSectors(mixed, 60, 8, 8, audit));
    require(audit.distinctTriplets == 1 && audit.conflictedTriplets == 1);
    require(audit.details[0].outcome.evidence.mixedSources == 1);
    require(audit.details[0].outcome.evidence.observedSources == 1);

    EvidenceTrialSet restrictions {};
    EvidenceTrial r1 = trial("r1");
    appendX(r1, 0, "BlockedCandidate", 0);
    appendX(r1, 4, "BlockedCandidate", 90);
    restrictions.trials.push_back(r1);
    EvidenceTrial r2 = trial("r2");
    appendX(r2, 0, "DeflectedCandidate", 180);
    appendX(r2, 4, "DeflectedCandidate", 270);
    restrictions.trials.push_back(r2);
    require(auditFineTripletSectors(restrictions, 60, 8, 8, audit));
    require(audit.restrictionEvidenceTriplets == 1);

    // A two-face chain has no triple; adjacent records are never joined.
    EvidenceTrialSet shortSequences {};
    EvidenceTrial shortTrial = trial("short");
    shortTrial.rows.push_back(row(0, "Progress", 8, 8, 8, 80, 8, 8, 0));
    shortTrial.rows.push_back(row(4, "Progress", 80, 8, 8, 112, 8, 8, 0));
    shortSequences.trials.push_back(shortTrial);
    require(auditFineTripletSectors(shortSequences, 60, 8, 8, audit));
    require(audit.chains == 1 && audit.distinctTriplets == 0);

    // Edge/corner crossing ambiguity must not yield an invented triplet.
    EvidenceTrialSet ambiguous {};
    EvidenceTrial edge = trial("edge");
    edge.rows.push_back(row(0, "Progress", 8, 0, 8, 112, 0, 8, 0));
    ambiguous.trials.push_back(edge);
    require(auditFineTripletSectors(ambiguous, 60, 8, 8, audit));
    require(audit.distinctTriplets == 0 && audit.tripletWindows == 0);

    // Y and Z axes remain independent physical triplets.
    EvidenceTrialSet axes {};
    EvidenceTrial axisY = trial("y");
    axisY.rows.push_back(row(0, "Progress", 8, 8, 8, 8, 112, 8, 0));
    axes.trials.push_back(axisY);
    EvidenceTrial axisZ = trial("z");
    axisZ.rows.push_back(row(0, "Progress", 8, 8, 112, 8, 8, 8, 0));
    axes.trials.push_back(axisZ);
    require(auditFineTripletSectors(axes, 8, 60, 60, audit));
    require(audit.distinctTriplets == 2);
    require(audit.insufficientTriplets == 2);

    // An invalid query never partially overwrites the prior audit.
    audit.distinctTriplets = 77;
    require(!auditFineTripletSectors(sources, 1e40, 8, 8, audit));
    require(audit.distinctTriplets == 77);
    return 0;
  }
  catch (const std::exception &error) {
    std::fprintf(stderr, "fine triplet sectors: %s\n", error.what());
    return 1;
  }
}
