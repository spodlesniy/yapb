//
// AiPB - training transition recorder.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <cstddef>
#include <cstdint>

#include <ai/ai_action_result.h>
#include <ai/ai_combat_event.h>
#include <ai/ai_defuse_event.h>
#include <ai/ai_navigation_event.h>

namespace ai {

constexpr size_t kTrainingTransitionCapacity = 1024;
constexpr size_t kTrainingCombatEventCapacity = 4096;
constexpr size_t kTrainingNavigationEventCapacity = 2048;
constexpr size_t kTrainingDefuseEventCapacity = 512;

enum class TrainingRecordResult : uint8_t {
  Recorded,
  NoPendingAction,
  ActionMismatch,
  NonTerminalResult,
  BufferFull,
};

struct TrainingTransition {
  uint64_t episodeId {};
  Observation observation {};
  Action action {};
  float reward {};
  Observation nextObservation {};
  ActionResult result {};
};

class TrainingBuffer final {
private:
  TrainingTransition m_transitions[kTrainingTransitionCapacity] {};
  CombatEvent m_combatEvents[kTrainingCombatEventCapacity] {};
  NavigationEvent m_navigationEvents[kTrainingNavigationEventCapacity] {};
  DefuseEvent m_defuseEvents[kTrainingDefuseEventCapacity] {};
  size_t m_defuseEventCount {};
  size_t m_droppedDefuseEvents {};
  size_t m_navigationEventCount {};
  size_t m_droppedNavigationEvents {};
  size_t m_navigationCompactionCount {};
  size_t m_combatEventCount {};
  size_t m_droppedCombatEvents {};
  size_t m_size {};
  size_t m_droppedTransitions {};
  size_t m_episodeCount {};
  uint64_t m_nextEpisodeId { 1 };

  bool containsEpisode(uint64_t episodeId) const {
    for (size_t i = 0; i < m_size; ++i) {
      if (m_transitions[i].episodeId == episodeId) {
        return true;
      }
    }

    return false;
  }

public:
  uint64_t beginEpisode() {
    const auto episodeId = m_nextEpisodeId++;

    if (m_nextEpisodeId == 0) {
      ++m_nextEpisodeId;
    }
    return episodeId;
  }

  bool append(uint64_t episodeId, const Observation &observation, const Action &action, float reward,
              const Observation &nextObservation, const ActionResult &result) {
    if (m_size >= kTrainingTransitionCapacity) {
      ++m_droppedTransitions;
      return false;
    }

    if (!containsEpisode(episodeId)) {
      ++m_episodeCount;
    }

    auto &transition = m_transitions[m_size++];
    transition.episodeId = episodeId;
    transition.observation = observation;
    transition.action = action;
    transition.reward = reward;
    transition.nextObservation = nextObservation;
    transition.result = result;
    return true;
  }

  bool appendCombatEvent(const CombatEvent &event) {
    if (m_combatEventCount >= kTrainingCombatEventCapacity) {
      ++m_droppedCombatEvents;
      return false;
    }
    m_combatEvents[m_combatEventCount++] = event;
    return true;
  }

  size_t combatEventCount() const { return m_combatEventCount; }
  size_t droppedCombatEvents() const { return m_droppedCombatEvents; }
  const CombatEvent &combatEventAt(size_t index) const { return m_combatEvents[index]; }

  bool appendNavigationEvent(const NavigationEvent &event) {
    if (m_navigationEventCount >= kTrainingNavigationEventCapacity) {
      // Preserve an ordered sample of earlier events and make room for new ones.
      const size_t kept = kTrainingNavigationEventCapacity / 2;
      for (size_t i = 0; i < kept; ++i) {
        m_navigationEvents[i] = m_navigationEvents[i * 2];
      }
      m_navigationEventCount = kept;
      m_droppedNavigationEvents += kTrainingNavigationEventCapacity - kept;
      ++m_navigationCompactionCount;
    }
    m_navigationEvents[m_navigationEventCount++] = event;
    return true;
  }

  size_t navigationEventCount() const { return m_navigationEventCount; }
  size_t droppedNavigationEvents() const { return m_droppedNavigationEvents; }
  size_t navigationCompactionCount() const { return m_navigationCompactionCount; }
  const NavigationEvent &navigationEventAt(size_t index) const { return m_navigationEvents[index]; }

  bool appendDefuseEvent(const DefuseEvent &event) {
    if (m_defuseEventCount >= kTrainingDefuseEventCapacity) {
      ++m_droppedDefuseEvents;
      return false;
    }
    m_defuseEvents[m_defuseEventCount++] = event;
    return true;
  }
  size_t defuseEventCount() const { return m_defuseEventCount; }
  size_t droppedDefuseEvents() const { return m_droppedDefuseEvents; }
  const DefuseEvent &defuseEventAt(size_t index) const { return m_defuseEvents[index]; }

  void clear() {
    for (size_t i = 0; i < kTrainingTransitionCapacity; ++i) {
      m_transitions[i] = {};
    }
    m_size = 0;
    m_combatEventCount = 0;
    m_droppedCombatEvents = 0;
    m_navigationEventCount = 0;
    m_droppedNavigationEvents = 0;
    m_navigationCompactionCount = 0;
    m_defuseEventCount = 0;
    m_droppedDefuseEvents = 0;
    m_droppedTransitions = 0;
    m_episodeCount = 0;
  }

  size_t droppedTransitions() const {
    return m_droppedTransitions;
  }

  void reset() {
    clear();
    m_nextEpisodeId = 1;
  }

  bool hasCapacity() const {
    return m_size < kTrainingTransitionCapacity;
  }

  bool isFull() const {
    return m_size >= kTrainingTransitionCapacity;
  }

  size_t size() const {
    return m_size;
  }

  bool empty() const {
    return m_size == 0;
  }

  size_t episodeCount() const {
    return m_episodeCount;
  }

  const TrainingTransition &at(size_t index) const {
    return m_transitions[index];
  }

  const TrainingTransition *data() const {
    return m_transitions;
  }
};

inline TrainingBuffer &getTrainingBuffer() {
  static TrainingBuffer buffer {};
  return buffer;
}

class TrainingRecorder final {
private:
  TrainingBuffer *m_buffer {};
  uint64_t m_episodeId {};
  bool m_pendingAction {};
  Observation m_pendingObservation {};
  Action m_pendingActionData {};

public:
  explicit TrainingRecorder(TrainingBuffer &buffer = getTrainingBuffer()) : m_buffer(&buffer) {
  }

  void beginEpisode() {
    discardPendingAction();
    m_episodeId = m_buffer->beginEpisode();
  }

  void endEpisode() {
    discardPendingAction();
    m_episodeId = 0;
  }

  bool startAction(const Observation &observation, const Action &action) {
    if (m_episodeId == 0 || m_pendingAction || action.type == ActionType::None) {
      return false;
    }

    m_pendingObservation = observation;
    m_pendingActionData = action;
    m_pendingAction = true;
    return true;
  }

  TrainingRecordResult finishAction(const Observation &nextObservation, const ActionResult &result, float reward) {
    if (!m_pendingAction) {
      return TrainingRecordResult::NoPendingAction;
    }

    if (result.action != m_pendingActionData.type) {
      return TrainingRecordResult::ActionMismatch;
    }

    if (!result.isTerminal()) {
      return TrainingRecordResult::NonTerminalResult;
    }

    auto recordedResult = result;

    if (recordedResult.elapsedTime <= 0.0f) {
      const float elapsedTime = nextObservation.gameTime - m_pendingObservation.gameTime;
      recordedResult.elapsedTime = elapsedTime > 0.0f ? elapsedTime : 0.0f;
    }

    const bool appended =
      m_buffer->append(m_episodeId, m_pendingObservation, m_pendingActionData, reward, nextObservation, recordedResult);

    if (!appended) {
      return TrainingRecordResult::BufferFull;
    }

    discardPendingAction();
    return TrainingRecordResult::Recorded;
  }

  void discardPendingAction() {
    m_pendingAction = false;
    m_pendingObservation = {};
    m_pendingActionData = {};
  }

  void reset() {
    m_episodeId = 0;
    discardPendingAction();
  }

  bool hasPendingAction() const {
    return m_pendingAction;
  }

  uint64_t episodeId() const {
    return m_episodeId;
  }

  const Observation &pendingObservation() const {
    return m_pendingObservation;
  }

  const Action &pendingAction() const {
    return m_pendingActionData;
  }

  TrainingBuffer &buffer() {
    return *m_buffer;
  }

  const TrainingBuffer &buffer() const {
    return *m_buffer;
  }
};

} // namespace ai
