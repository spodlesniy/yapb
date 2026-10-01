//
// AiPB - AI abstraction unit-test framework.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace ai::test {

namespace {

struct TestCase {
   const char *name {};
   TestFunction function {};
};

std::vector<TestCase> &getTests () {
   static std::vector<TestCase> tests {};
   return tests;
}

const char *g_currentTest {};
int g_failures {};

} // namespace

bool registerTest (const char *name, TestFunction function) {
   getTests ().push_back ({ name, function });
   return true;
}

void expect (bool condition, const char *message) {
   if (condition) {
      return;
   }

   if (g_currentTest != nullptr) {
      std::fprintf (stderr, "FAIL [%s]: %s\n", g_currentTest, message);
   }
   else {
      std::fprintf (stderr, "FAIL: %s\n", message);
   }

   ++g_failures;
}

void expectNear (float actual, float expected, float epsilon, const char *message) {
   expect (
      std::isfinite (actual)
      && std::isfinite (expected)
      && std::isfinite (epsilon)
      && epsilon >= 0.0f
      && std::fabs (actual - expected) <= epsilon,
      message
   );
}

int runAll () {
   for (const auto &test : getTests ()) {
      g_currentTest = test.name;
      test.function ();
   }

   g_currentTest = nullptr;

   if (g_failures != 0) {
      std::fprintf (stderr, "%d AI unit test assertion(s) failed.\n", g_failures);
      return 1;
   }

   std::printf ("AI unit tests passed (%zu test cases).\n", getTests ().size ());
   return 0;
}

} // namespace ai::test

int main () {
   return ai::test::runAll ();
}
