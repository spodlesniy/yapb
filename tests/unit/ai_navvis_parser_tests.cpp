#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

// AiPB - independent navigation viewer file parser tests.
#include "ai_test.h"

#include <ai/ai_navvis_format.h>

#include <cstdio>
#include <string>

namespace {
constexpr const char *kTestFile = "aipb_navvis_parser_test.nviz";
constexpr const char *kDigest = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

bool writeFixture(const std::string &contents) {
   std::FILE *file = std::fopen(kTestFile, "wb");
   if (!file) {
      return false;
   }
   const bool success = std::fwrite(contents.data(), 1, contents.size(), file) == contents.size();
   return std::fclose(file) == 0 && success;
}

std::string fixture(const std::string &vertices, const std::string &edges, unsigned nodeCount = 2, unsigned edgeCount = 1) {
   return std::string("AIPBNAVVIS 1 test_map ") + kDigest + "\nCOUNT " + std::to_string(nodeCount) + " " +
      std::to_string(edgeCount) + "\n" + vertices + edges + "END\n";
}

bool accepted(const std::string &contents, ai::navvis::Data &output) {
   if (!writeFixture(contents)) {
      ai::test::expect(false, "fixture writing succeeded");
      return false;
   }
   const bool success = ai::navvis::load(kTestFile, "test_map", output);
   std::remove(kTestFile);
   return success;
}
} // namespace

AI_TEST(testNavvisParserAcceptsSortedAndVerifiedData) {
   ai::navvis::Data data;
   const auto input = fixture("V 2 10.5 -3 45.25 2 1 1 0\nV 1 0 0 0 0 1 0 0\n", "E 2 1 1\n");
   ai::test::expect(accepted(input, data), "valid snapshot accepted");
   ai::test::expect(data.map == "test_map" && data.sha256 == kDigest, "map and fingerprint retained");
   ai::test::expect(data.points.size() == 2 && data.points[0].id == 1 && data.points[1].id == 2,
      "candidate IDs sorted by parser");
   ai::test::expect(data.edges.size() == 1 && data.edges[0].type == 1 && data.edges[0].from == 2,
      "directed jump edge retained");
}

AI_TEST(testNavvisParserRejectsMalformedSnapshotsWithoutMutation) {
   const auto vertices = std::string("V 1 0 0 0 0 1 0 0\nV 2 1 2 3 1 0 1 0\n");
   const auto edge = std::string("E 1 2 0\n");
   ai::navvis::Data unchanged;
   unchanged.map = "sentinel";
   auto reject = [&](const std::string &contents, const char *message) {
      ai::test::expect(!accepted(contents, unchanged), message);
      ai::test::expect(unchanged.map == "sentinel", "invalid snapshot leaves output untouched");
   };
   reject(fixture(vertices, "E 1 3 0\n"), "dangling endpoint rejected");
   reject(fixture(vertices, "E 1 2 3\n"), "unknown traversal rejected");
   reject(fixture("V 1 0 0 0 0 1 0 0\nV 1 1 2 3 0 1 0 0\n", edge), "duplicate ID rejected");
   reject(fixture("V 1 nan 0 0 0 1 0 0\nV 2 0 0 0 0 1 0 0\n", edge), "non-finite coordinate rejected");
   reject(fixture(vertices, "E 1 2 0 extra\n"), "extra token rejected");
   reject(fixture(vertices, edge) + "garbage", "trailing data rejected");
   reject(fixture(vertices, edge).substr(0, 30), "truncation rejected");
   reject(fixture(vertices, edge, 100001, 1), "oversized candidate count rejected");
   reject(fixture(vertices, edge).replace(0, 1, "B"), "wrong magic rejected");
   reject(fixture(vertices, edge).replace(13, 8, "wrongmap"), "wrong map name rejected");
   reject(fixture(vertices, edge).replace(0, 10, "BADHEADER "), "wrong header rejected");
   reject(fixture(vertices, edge).replace(11, 1, "2"), "wrong version rejected");
}
