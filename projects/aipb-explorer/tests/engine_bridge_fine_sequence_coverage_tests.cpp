#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

#include <aipb/explorer/engine_bridge_fine_sequence_coverage.h>

using namespace aipb::explorer::bridge;

static void require(bool condition) {
  if (!condition) {
    throw std::runtime_error("fine sequence coverage regression");
  }
}

static std::string row(
  int step,
  const char *classification,
  double start,
  double end,
  double y,
  double heading
) {
  char buffer[768] {};
  const int written = std::snprintf(buffer, sizeof(buffer),
    "%d,%d,%d,%s,%s,unknown,100,%.17g,%.17g,8,%.17g,%.17g,8,%.17g,25,5,5,0\n",
    step, step + 3, step, classification,
    std::string(classification) == "Progress" ? "direct" : "confirming",
    start, y, end, y, heading);
  require(written > 0 && static_cast<std::size_t>(written) < sizeof(buffer));
  return buffer;
}

static EvidenceTrial trial(const char *name) {
  EvidenceTrial result {};
  result.name = name;
  result.botIndex = 0;
  result.sourceSteps = 100;
  return result;
}

int main() {
  try {
    EvidenceTrialSet sources {};
    EvidenceTrial first = trial("first");
    first.rows.push_back(row(0, "Progress", 8, 24, 8, 0));
    first.rows.push_back(row(4, "Progress", 8, 40, 8, 0));
    first.rows.push_back(row(8, "Progress", 8, 80, 8, 0));
    first.rows.push_back(row(12, "Progress", 8, 112, 8, 90));
    first.rows.push_back(row(16, "BlockedCandidate", 8, 112, 8, 90));
    first.rows.push_back(row(20, "Progress", 8, 112, 0, 180));
    first.rows.push_back(row(24, "Progress", 112, 8, 8, 180));
    sources.trials.push_back(first);

    EvidenceTrial second = trial("second");
    second.rows.push_back(row(0, "Progress", 8, 80, 8, 0));
    second.rows.push_back(row(4, "Progress", 8, 112, 8, 90));
    sources.trials.push_back(second);

    FineCoverageAudit audit {};
    require(auditFineSequenceCoverage(sources, 60, 8, 8, audit));
    require(audit.sources == 2 && audit.queryCoarse.x == 0);
    require(audit.records == 9 && audit.zeroFaceRecords == 1);
    require(audit.singleFaceRecords == 1 && audit.twoFaceChains == 2);
    require(audit.threePlusChains == 4 && audit.ambiguousRecords == 1);
    require(audit.discontinuousRecords == 0 && audit.pairWindows == 10);
    require(audit.tripletWindows == 4 && audit.longestFaces == 3);
    require(audit.chainSources == 2 && audit.tripletSources == 2);

    const FineCoverageBin &forward = audit.bins[0];
    require(forward.headingBin == 0 && forward.records == 4);
    require(forward.zeroFaceRecords == 1 && forward.singleFaceRecords == 1);
    require(forward.twoFaceChains == 2 && forward.threePlusChains == 0);
    require(forward.pairWindows == 2 && forward.tripletWindows == 0);
    require(forward.chainSources == 2 && forward.tripletSources == 0);

    const FineCoverageBin &sideways = audit.bins[2];
    require(sideways.records == 3 && sideways.threePlusChains == 3);
    require(sideways.pairWindows == 6 && sideways.tripletWindows == 3);
    require(sideways.chainSources == 2 && sideways.tripletSources == 2);
    require(sideways.longestFaces == 3);

    const FineCoverageBin &backward = audit.bins[4];
    require(backward.records == 2 && backward.ambiguousRecords == 1);
    require(backward.threePlusChains == 1 && backward.tripletWindows == 1);
    require(backward.chainSources == 1 && backward.tripletSources == 1);

    for (int bin = 0; bin < kDirectionBins; ++bin) {
      if (bin != 0 && bin != 2 && bin != 4) {
        require(audit.bins[bin].records == 0);
        require(audit.bins[bin].tripletWindows == 0);
      }
    }

    require(audit.longest.size() == 6);
    require(audit.longest[0].source == 0 && audit.longest[0].firstStep == 12);
    require(audit.longest[0].headingBin == 2 && audit.longest[0].faces == 3);
    require(audit.longest[1].classification == MovementClassification::BlockedCandidate);
    require(audit.longest[2].firstStep == 24 && audit.longest[2].headingBin == 4);
    require(audit.longest[3].source == 1 && audit.longest[3].firstStep == 4);

    // A four-face segment yields two overlapping triplet windows, not sources.
    EvidenceTrialSet longSegment {};
    EvidenceTrial longTrial = trial("long");
    longTrial.rows.push_back(row(0, "Progress", 8, 152, 8, 0));
    longSegment.trials.push_back(longTrial);
    require(auditFineSequenceCoverage(longSegment, 60, 8, 8, audit));
    require(audit.records == 1 && audit.threePlusChains == 1);
    require(audit.pairWindows == 3 && audit.tripletWindows == 2);
    require(audit.longestFaces == 4 && audit.tripletSources == 1);

    // Separate one-face records never join into a longer chain.
    EvidenceTrialSet separated {};
    EvidenceTrial onlySingles = trial("singles");
    onlySingles.rows.push_back(row(0, "Progress", 24, 40, 8, 0));
    onlySingles.rows.push_back(row(4, "Progress", 56, 72, 8, 0));
    separated.trials.push_back(onlySingles);
    require(auditFineSequenceCoverage(separated, 60, 8, 8, audit));
    require(audit.records == 2 && audit.singleFaceRecords == 2);
    require(audit.twoFaceChains == 0 && audit.threePlusChains == 0);
    require(audit.tripletWindows == 0 && audit.chainSources == 0);
    require(audit.longest.empty());

    // An empty/invalid input preserves the previous successful result.
    audit.records = 77;
    require(!auditFineSequenceCoverage(sources, 1e40, 8, 8, audit));
    require(!auditFineSequenceCoverage(sources, 60, 8, std::nan(""), audit));
    EvidenceTrialSet empty {};
    require(!auditFineSequenceCoverage(empty, 60, 8, 8, audit));
    require(audit.records == 77);
    return 0;
  }
  catch (const std::exception &error) {
    std::fprintf(stderr, "fine sequence coverage: %s\n", error.what());
    return 1;
  }
}
