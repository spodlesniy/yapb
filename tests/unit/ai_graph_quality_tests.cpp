// AiPB - read-only graph quality analyzer unit tests.
#include "ai_test.h"

#include <ai/ai_graph_quality.h>

using ai::test::expect;

namespace {

struct QualityFixture {
  int links[8][ai::kGraphQualityMaxLinks] {};
  bool goals[8] {};
  bool rescues[8] {};
  bool terroristStarts[8] {};
  bool ctStarts[8] {};

  QualityFixture() {
    for (auto &node : links) {
      for (int &edge : node) {
        edge = -1;
      }
    }
  }

  ai::GraphQualityReport evaluate(int count) const {
    return ai::measureGraphQuality(count, terroristStarts, ctStarts,
        [&](int node, int slot) { return links[node][slot]; },
        [&](int node) { return goals[node]; },
        [&](int node) { return rescues[node]; });
  }
};

} // namespace

AI_TEST(testGraphQualityEmptyAndInvalid) {
  QualityFixture fixture {};
  const auto empty = fixture.evaluate(0);
  expect(empty.valid && empty.nodes == 0 && empty.weakComponents == 0, "empty graph is valid and has no components");
  expect(!fixture.evaluate(-1).valid, "negative node counts rejected");
  expect(!fixture.evaluate(ai::kGraphQualityMaxNodes + 1).valid, "oversized graph rejected before access");
}

AI_TEST(testGraphQualityTopologyAnomalies) {
  QualityFixture fixture {};
  fixture.links[0][0] = 1;
  fixture.links[0][1] = 1;
  fixture.links[0][2] = 0;
  fixture.links[0][3] = 4;
  fixture.links[0][4] = -2;
  fixture.links[1][0] = 2;
  const auto report = fixture.evaluate(4);
  expect(report.valid && report.nodes == 4, "graph retains node count");
  expect(report.edges == 2 && report.duplicateLinks == 1, "valid unique non-self directed links counted once");
  expect(report.invalidLinks == 2 && report.selfLinks == 1, "invalid and self links accounted separately");
  expect(report.oneWayLinks == 2, "missing reverse connections detected");
  expect(report.weakComponents == 2 && report.largestWeakComponent == 3, "weak components treat directed edges as undirected");
  expect(report.isolatedNodes == 1, "isolated nodes require zero incoming and outgoing links");
}

AI_TEST(testGraphQualityDirectedReachabilityAndSpawns) {
  QualityFixture fixture {};
  fixture.links[0][0] = 1;
  fixture.links[1][0] = 0;
  fixture.links[1][1] = 2;
  fixture.links[3][0] = 2;
  fixture.terroristStarts[0] = true;
  fixture.ctStarts[3] = true;
  fixture.goals[2] = true;
  fixture.goals[4] = true;
  fixture.rescues[1] = true;
  fixture.rescues[4] = true;
  const auto report = fixture.evaluate(5);
  expect(report.edges == 4 && report.oneWayLinks == 2, "bidirectional and one-way edges distinguished");
  expect(report.weakComponents == 2 && report.largestWeakComponent == 4, "shared outgoing destination joins weak components");
  expect(report.terroristSources == 1 && report.ctSources == 1, "caller-selected spawn nodes counted");
  expect(report.goals == 2 && report.rescues == 2, "objective tags counted independently");
  expect(report.terroristReachableGoals == 1 && report.ctReachableGoals == 1, "goal coverage follows directed paths");
  expect(report.ctReachableRescues == 0, "CT cannot reach rescue node by reverse of one-way path");
}

AI_TEST(testGraphQualityNoSpawnIsUnknownNotCoverageFailure) {
  QualityFixture fixture {};
  fixture.links[0][0] = 1;
  fixture.goals[1] = true;
  const auto report = fixture.evaluate(2);
  expect(report.goals == 1 && report.terroristSources == 0 && report.ctSources == 0, "goal with no spawn source is measurable");
  expect(report.terroristReachableGoals == 0 && report.ctReachableGoals == 0, "no source yields zero reachable, not inferred spawn coverage");
}
