#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

#include <aipb/explorer/engine_bridge_fine_sequence_pairs.h>

using namespace aipb::explorer::bridge;

static void require(bool ok) {
  if (!ok) {
    throw std::runtime_error("fine pair regression");
  }
}

static std::string row(int step, const char *kind, double start, double end) {
  char buffer[768] {};
  const int length = std::snprintf(buffer, sizeof(buffer),
    "%d,%d,%d,%s,%s,unknown,100,%.17g,8,8,%.17g,8,8,0,25,5,5,0\n",
    step, step + 3, step, kind,
    std::string(kind) == "Progress" ? "direct" : "confirming",
    start, end);
  require(length > 0 && static_cast<std::size_t>(length) < sizeof(buffer));
  return buffer;
}

static EvidenceTrial trial(const char *name, const char *kind, int windows, bool reverse = false) {
  EvidenceTrial t {};
  t.name = name;
  t.botIndex = 0;
  t.sourceSteps = 100;
  for (int i = 0; i < windows; ++i) {
    t.rows.push_back(row(i * 4, kind, reverse ? 112 : 8, reverse ? 8 : 112));
  }
  return t;
}

int main() {
  try {
    FinePairAudit audit {};
    EvidenceTrialSet sources {};
    sources.trials.push_back(trial("one", "Progress", 5));
    require(auditFineSequencePairs(sources, 60, 8, 8, 0, audit));
    require(audit.sequences.chains == 5 && audit.sequences.orderedPairs == 10);
    require(audit.distinctPairs == 2 && audit.observedSources == 1);
    require(audit.insufficientPairs == 2);
    require(audit.details[0].consensus.strongProgressSources == 1);

    sources.trials.push_back(trial("weak", "Progress", 1));
    require(auditFineSequencePairs(sources, 60, 8, 8, 0, audit));
    require(audit.details[0].consensus.status == DirectionAssessmentStatus::Insufficient);
    require(audit.details[0].consensus.weakSources == 1);
    sources.trials.push_back(trial("second", "Progress", 2));
    require(auditFineSequencePairs(sources, 60, 8, 8, 0, audit));
    require(audit.progressEvidencePairs == 2);
    require(audit.details[0].consensus.strongProgressSources == 2);

    sources.trials.push_back(trial("reverse", "BlockedCandidate", 2, true));
    require(auditFineSequencePairs(sources, 60, 8, 8, 0, audit));
    require(audit.distinctPairs == 4 && audit.progressEvidencePairs == 2);
    require(audit.insufficientPairs == 2);

    sources.trials.push_back(trial("opposite", "DeflectedCandidate", 1));
    require(auditFineSequencePairs(sources, 60, 8, 8, 0, audit));
    require(audit.conflictedPairs == 2 && audit.insufficientPairs == 2);
    require(audit.details[0].consensus.restrictionSources == 1);

    EvidenceTrialSet restrictions {};
    restrictions.trials.push_back(trial("r1", "BlockedCandidate", 2));
    restrictions.trials.push_back(trial("r2", "DeflectedCandidate", 2));
    require(auditFineSequencePairs(restrictions, 60, 8, 8, 0, audit));
    require(audit.restrictionEvidencePairs == 2);

    EvidenceTrialSet mixed {};
    EvidenceTrial one = trial("mixed", "Progress", 2);
    one.rows.push_back(row(8, "BlockedCandidate", 8, 112));
    mixed.trials.push_back(one);
    require(auditFineSequencePairs(mixed, 60, 8, 8, 0, audit));
    require(audit.conflictedPairs == 2);
    require(audit.details[0].consensus.mixedSources == 1);

    EvidenceTrialSet empty {};
    empty.trials.push_back(trial("single", "Progress", 1));
    empty.trials[0].rows[0] = row(0, "Progress", 24, 40);
    require(auditFineSequencePairs(empty, 60, 8, 8, 0, audit));
    require(audit.distinctPairs == 0 && audit.sequences.singleFaceRecords == 1);
    require(auditFineSequencePairs(sources, 60, 8, 8, 90, audit));
    require(audit.distinctPairs == 0);
    audit.distinctPairs = 77;
    require(!auditFineSequencePairs(sources, 1e40, 8, 8, 0, audit));
    require(!auditFineSequencePairs(sources, 60, 8, 8, std::nan(""), audit));
    require(audit.distinctPairs == 77);
    return 0;
  }
  catch (const std::exception &error) {
    std::fprintf(stderr, "fine pairs: %s\n", error.what());
    return 1;
  }
}
