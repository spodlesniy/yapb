//
// AiPB - AI action state.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_action_result.h>

namespace ai {

// Tracks one in-flight action without coupling its lifecycle to the game runtime.
class ActionState final {
private:
  Action m_action {};
  ActionResult m_result {};
  bool m_active {};

public:
  bool start(const Action &action) {
    if (m_active) {
      return false;
    }

    m_action = action;
    m_result = {};
    m_result.action = action.type;
    m_active = true;
    return true;
  }

  bool updateResult(const ActionResult &result) {
    if (!m_active || result.action != m_action.type) {
      return false;
    }

    m_result = result;
    m_active = !result.isTerminal();
    return true;
  }

  bool cancel() {
    if (!m_active) {
      return false;
    }

    m_result.action = m_action.type;
    m_result.type = ActionResultType::Interrupted;
    m_active = false;
    return true;
  }

  void reset() {
    m_action = {};
    m_result = {};
    m_active = false;
  }

  bool isActive() const {
    return m_active;
  }

  const Action &action() const {
    return m_action;
  }

  const ActionResult &result() const {
    return m_result;
  }
};

} // namespace ai
