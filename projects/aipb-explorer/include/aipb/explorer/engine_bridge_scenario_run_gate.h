#pragma once

#include <cstddef>
#include <utility>

#include <aipb/explorer/engine_bridge_scenario_start.h>

namespace aipb::explorer::scenario {

enum class RunStartStatus {
  Ready,
  InvalidScenario,
  UnsupportedAction,
  StartMismatch,
  StartUnverified
};

inline const char *runStartStatusName(RunStartStatus status) {
  switch (status) {
  case RunStartStatus::Ready: return "Ready";
  case RunStartStatus::InvalidScenario: return "InvalidScenario";
  case RunStartStatus::UnsupportedAction: return "UnsupportedAction";
  case RunStartStatus::StartMismatch: return "StartMismatch";
  case RunStartStatus::StartUnverified: return "StartUnverified";
  }

  return "InvalidScenario";
}

struct RunStartDecision {
  RunStartStatus status { RunStartStatus::InvalidScenario };
  std::size_t actionIndex {};
  const char *unsupportedField {};
  ScenarioStartAssessment assessment {};

  bool ready() const {
    return status == RunStartStatus::Ready;
  }
};

// Every attempt requires a fresh observation; a previous successful preflight
// is not an execution permit. Unsupported controls reject the whole scenario.
inline RunStartDecision checkScenarioRunStart(
  const Scenario &scenario,
  const ScenarioStartObservation &actual,
  int expectedPrimaryId,
  int expectedSecondaryId,
  int expectedSelectedId
) {
  if (!validScenario(scenario)) {
    return {};
  }

  for (std::size_t i = 0; i < scenario.actions.size(); ++i) {
    const Action &action = scenario.actions[i];

    if (action.attack) {
      return { RunStartStatus::UnsupportedAction, i, "attack", {} };
    }
    if (action.attack2) {
      return { RunStartStatus::UnsupportedAction, i, "attack2", {} };
    }
    if (action.use) {
      return { RunStartStatus::UnsupportedAction, i, "use", {} };
    }
    if (action.select != "none") {
      return { RunStartStatus::UnsupportedAction, i, "select", {} };
    }
  }

  auto assessment = assessScenarioStart(
    scenario, actual, expectedPrimaryId, expectedSecondaryId, expectedSelectedId);

  if (assessment.mismatches != 0) {
    return { RunStartStatus::StartMismatch, 0, nullptr, std::move(assessment) };
  }
  if (assessment.unverified != 0) {
    return { RunStartStatus::StartUnverified, 0, nullptr, std::move(assessment) };
  }

  return { RunStartStatus::Ready, 0, nullptr, std::move(assessment) };
}

} // namespace aipb::explorer::scenario
