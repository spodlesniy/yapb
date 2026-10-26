// AiPB - independent viewer proximity and index regression tests.
#include "ai_test.h"

#include <ai/ai_navvis_select.h>

#include <vector>

namespace {

struct Candidate {
   unsigned id {};
   float x {}, y {}, z {};
   int source {};
};

} // namespace

AI_TEST(testNavvisPlayerIndexIsZeroBased) {
   ai::test::expect(ai::navvis::validPlayerIndex(0, 32), "first player slot accepted");
   ai::test::expect(ai::navvis::validPlayerIndex(31, 32), "last player slot accepted");
   ai::test::expect(!ai::navvis::validPlayerIndex(-1, 32), "negative slot rejected");
   ai::test::expect(!ai::navvis::validPlayerIndex(32, 32), "out-of-range slot rejected");
}

AI_TEST(testNavvisSelectsNearestRatherThanFirstIds) {
   const std::vector<Candidate> candidates {
      { 1, 90.0f, 0.0f, 0.0f, 0 },
      { 2, 15.0f, 0.0f, 0.0f, 0 },
      { 3, 20.0f, 0.0f, 0.0f, 0 },
      { 4, 10.0f, 0.0f, 0.0f, 0 }
   };
   ai::navvis::NearbyPoint<Candidate> selected[2] {};
   const auto count = ai::navvis::selectNearby(candidates, 1u, 0.0f, 0.0f, 0.0f, 100.0f, selected);
   ai::test::expect(count == 2, "bounded nearest selection retained two points");
   ai::test::expect(selected[0].point->id == 4 && selected[1].point->id == 2,
      "nearest nodes sorted by distance instead of file order");
}

AI_TEST(testNavvisNearbyFiltersAndTieBreaks) {
   const std::vector<Candidate> candidates {
      { 8, 5.0f, 0.0f, 0.0f, 1 },
      { 7, 5.0f, 0.0f, 0.0f, 0 },
      { 2, -5.0f, 0.0f, 0.0f, 0 },
      { 1, 20.0f, 0.0f, 0.0f, 0 }
   };
   ai::navvis::NearbyPoint<Candidate> selected[2] {};
   const auto count = ai::navvis::selectNearby(candidates, 1u, 0.0f, 0.0f, 0.0f, 5.0f, selected);
   ai::test::expect(count == 2, "radius and source mask applied");
   ai::test::expect(selected[0].point->id == 2 && selected[1].point->id == 7,
      "equal distances ordered deterministically by candidate ID");
   ai::test::expect(ai::navvis::selectNearby(candidates, 0u, 0.0f, 0.0f, 0.0f, 5.0f, selected) == 0,
      "disabled point categories render no points");
}
