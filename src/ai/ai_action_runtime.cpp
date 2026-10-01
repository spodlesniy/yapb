//
// AiPB - AI action runtime.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include <ai/ai_action_runtime.h>

namespace ai {

ActionRuntime::ActionRuntime(ActionExecutor &executor) : m_pipeline(executor, m_actionState), m_loop(m_controller, m_pipeline) {
}

void ActionRuntime::setMode(ControlMode mode) {
  if (mode == m_controller.getMode()) {
    return;
  }

  if (mode != ControlMode::Neural) {
    m_loop.cancel();
  }

  m_controller.setMode(mode);
}

ControlMode ActionRuntime::getMode() const {
  return m_controller.getMode();
}

void ActionRuntime::setPolicy(const Policy *policy) {
  if (policy == m_controller.getPolicy()) {
    return;
  }

  m_loop.cancel();
  m_controller.setPolicy(policy);
}

const Policy *ActionRuntime::getPolicy() const {
  return m_controller.getPolicy();
}

bool ActionRuntime::isControlEnabled() const {
  return m_controller.getMode() == ControlMode::Neural && m_controller.getPolicy() != nullptr;
}

Controller &ActionRuntime::controller() {
  return m_controller;
}

const Controller &ActionRuntime::controller() const {
  return m_controller;
}

ActionState &ActionRuntime::actionState() {
  return m_actionState;
}

const ActionState &ActionRuntime::actionState() const {
  return m_actionState;
}

ActionResult ActionRuntime::step(const Observation &observation, bool allowDecision) {
  return m_loop.step(observation, allowDecision);
}

bool ActionRuntime::cancel() {
  return m_loop.cancel();
}

void ActionRuntime::reset() {
  m_loop.reset();
}

bool ActionRuntime::isActive() const {
  return m_loop.isActive();
}

const Action &ActionRuntime::activeAction() const {
  return m_loop.activeAction();
}

const ActionResult &ActionRuntime::result() const {
  return m_loop.result();
}

} // namespace ai
