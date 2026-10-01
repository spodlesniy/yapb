#pragma once

namespace ai::test {

using TestFunction = void (*)();

bool registerTest(const char *name, TestFunction function);

void expect(bool condition, const char *message);
void expectNear(float actual, float expected, float epsilon, const char *message);

int runAll();

} // namespace ai::test

#define AI_TEST(name)                                                                                                                      \
  static void name();                                                                                                                      \
  namespace {                                                                                                                              \
  [[maybe_unused]] const bool name##_registered = ::ai::test::registerTest(#name, &name);                                                  \
  }                                                                                                                                        \
  static void name()
