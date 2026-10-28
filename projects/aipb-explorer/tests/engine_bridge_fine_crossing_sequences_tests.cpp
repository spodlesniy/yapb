#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

#include <aipb/explorer/engine_bridge_fine_crossing_sequences.h>

using namespace aipb::explorer::bridge;

static void require(bool ok) {
  if (!ok) {
    throw std::runtime_error("fine crossing sequence regression");
  }
}

static std::string row(
  int step,
  const char *kind,
  double sx,
  double sy,
  double sz,
  double ex,
  double ey,
  double ez,
  double heading = 0
) {
  char output[768] {};
  const bool progress = std::string(kind) == "Progress";
  const int written = std::snprintf(output, sizeof(output),
    "%d,%d,%d,%s,%s,unknown,100,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,25,5,5,0\n",
    step, step + 3, step, kind, progress ? "direct" : "confirming",
    sx, sy, sz, ex, ey, ez, heading);
  require(written > 0 && static_cast<std::size_t>(written) < sizeof(output));

  return output;
}

static EvidenceTrial trial(const char *name) {
  EvidenceTrial source {};
  source.name = name;
  source.botIndex = 0;
  source.sourceSteps = 100;
  return source;
}

int main() {
  try {
    EvidenceTrialSet sources {};
    EvidenceTrial first = trial("first");
    first.rows.push_back(row(0, "Progress", 8, 8, 8, 112, 8, 8));
    first.rows.push_back(row(4, "Progress", 112, 8, 8, 8, 8, 8));
    sources.trials.push_back(first);

    FineSequenceAudit audit {};
    require(auditFineCrossingSequences(sources, 60, 8, 8, 0, audit));
    require(audit.records == 2 && audit.crossingRecords == 2);
    require(audit.chains == 2 && audit.orderedPairs == 4);
    require(audit.progressChains == 2 && audit.restrictionChains == 0);
    require(audit.chainSources == 1 && audit.ambiguousRecords == 0);
    require(audit.details[0].source == 0 && audit.details[0].firstStep == 0);
    require(audit.details[0].steps.size() == 3);
    require(audit.details[0].steps[0].lowerCell.x == 0);
    require(audit.details[0].steps[1].lowerCell.x == 1);
    require(audit.details[0].steps[2].lowerCell.x == 2);
    require(audit.details[0].steps[0].positive);
    require(audit.details[0].steps[0].position[0] == 32);
    require(audit.details[1].firstStep == 4);
    require(!audit.details[1].steps[0].positive);
    require(audit.details[1].steps[0].lowerCell.x == 2);
    require(audit.details[1].steps[2].lowerCell.x == 0);

    // A second source has independent provenance, but no record-to-record link.
    EvidenceTrial second = trial("second");
    second.rows.push_back(row(0, "BlockedCandidate", 8, 8, 8, 112, 8, 8));
    second.rows.push_back(row(4, "Progress", 8, 8, 8, 40, 8, 8));
    sources.trials.push_back(second);
    require(auditFineCrossingSequences(sources, 60, 8, 8, 0, audit));
    require(audit.records == 4 && audit.crossingRecords == 4);
    require(audit.chains == 3 && audit.orderedPairs == 6);
    require(audit.restrictionChains == 1 && audit.singleFaceRecords == 1);
    require(audit.chainSources == 2);
    require(audit.details[2].source == 1);
    require(audit.details[2].classification == MovementClassification::BlockedCandidate);

    // The heading filter does not use movement sign to reject backward travel.
    EvidenceTrialSet heading {};
    EvidenceTrial backwards = trial("backwards");
    backwards.rows.push_back(row(0, "Progress", 112, 8, 8, 8, 8, 8, 90));
    heading.trials.push_back(backwards);
    require(auditFineCrossingSequences(heading, 60, 8, 8, 0, audit));
    require(audit.records == 0 && audit.chains == 0);
    require(auditFineCrossingSequences(heading, 60, 8, 8, 90, audit));
    require(audit.records == 1 && audit.chains == 1);
    require(!audit.details[0].steps[0].positive && audit.details[0].headingBin == 2);

    // A strict crossing of the 128-unit boundary is still a single fine face.
    EvidenceTrialSet coarse {};
    EvidenceTrial crossing = trial("coarse");
    crossing.rows.push_back(row(0, "Progress", 104, 8, 8, 168, 8, 8));
    coarse.trials.push_back(crossing);
    require(auditFineCrossingSequences(coarse, 124, 8, 8, 0, audit));
    require(audit.chains == 1 && audit.orderedPairs == 1);
    require(audit.details[0].steps[0].lowerCell.x == 3);
    require(audit.details[0].steps[1].lowerCell.x == 4);

    // Y and Z crossings preserve actual axes and direction.
    EvidenceTrialSet axes {};
    EvidenceTrial y = trial("y");
    y.rows.push_back(row(0, "Progress", 8, 8, 8, 8, 112, 8));
    axes.trials.push_back(y);
    EvidenceTrial z = trial("z");
    z.rows.push_back(row(0, "DeflectedCandidate", 8, 8, 112, 8, 8, 8));
    axes.trials.push_back(z);
    require(auditFineCrossingSequences(axes, 8, 60, 60, 0, audit));
    require(audit.chains == 2 && audit.orderedPairs == 4);
    require(audit.details[0].steps[0].axis == 1);
    require(audit.details[1].steps[0].axis == 2);
    require(!audit.details[1].steps[0].positive);

    // Negative fine-cell coordinates use floor division correctly.
    EvidenceTrialSet negative {};
    EvidenceTrial minus = trial("negative");
    minus.rows.push_back(row(0, "Progress", -120, -8, -8, -8, -8, -8));
    negative.trials.push_back(minus);
    require(auditFineCrossingSequences(negative, -64, -8, -8, 0, audit));
    require(audit.chains == 1 && audit.details[0].steps.size() == 3);
    require(audit.details[0].steps[0].lowerCell.x == -4);

    // Endpoint-only contacts create no crossing event.
    EvidenceTrialSet endpoint {};
    EvidenceTrial touch = trial("touch");
    touch.rows.push_back(row(0, "Progress", 24, 8, 8, 32, 8, 8));
    endpoint.trials.push_back(touch);
    require(auditFineCrossingSequences(endpoint, 16, 8, 8, 0, audit));
    require(audit.records == 1 && audit.crossingRecords == 0);
    require(audit.chains == 0 && audit.ambiguousRecords == 0);

    // Grid-edge travel and diagonal corner crossings are not chains.
    EvidenceTrialSet ambiguous {};
    EvidenceTrial edge = trial("edge");
    edge.rows.push_back(row(0, "Progress", 8, 0, 8, 112, 0, 8));
    ambiguous.trials.push_back(edge);
    EvidenceTrial corner = trial("corner");
    corner.rows.push_back(row(0, "Progress", 8, 8, 8, 104, 104, 8));
    ambiguous.trials.push_back(corner);
    require(auditFineCrossingSequences(ambiguous, 60, 8, 8, 0, audit));
    require(audit.ambiguousRecords == 2 && audit.chains == 0);

    // A legitimate crossing before a corner cannot connect across it.
    EvidenceTrialSet gap {};
    EvidenceTrial split = trial("split");
    split.rows.push_back(row(0, "Progress", 8, 8, 8, 120, 56, 8));
    gap.trials.push_back(split);
    require(auditFineCrossingSequences(gap, 60, 8, 8, 0, audit));
    require(audit.ambiguousRecords == 1 && audit.chains == 0);

    audit.chains = 77;
    require(!auditFineCrossingSequences(sources, 1e40, 8, 8, 0, audit));
    require(!auditFineCrossingSequences(sources, 60, 8, 8, std::nan(""), audit));
    require(audit.chains == 77);
    return 0;
  }
  catch (const std::exception &error) {
    std::fprintf(stderr, "fine crossing sequences: %s\n", error.what());
    return 1;
  }
}
