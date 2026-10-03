//
// AiPB - YaPB action execution context.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_action_execution_context.h>

class Bot;

namespace ai {

// Adapts the generic AI action execution context to the live YaPB bot.
class YaPBActionExecutionContext final : public ActionExecutionContext {
private:
  Bot *m_bot {};

public:
  explicit YaPBActionExecutionContext(Bot &bot);

  bool isAlive() const override;
  bool allowsNavigationOverride() const override;
  bool navigationNodeExists(int node) const override;
  int navigationNodeForPosition(const Vec3 &position) const override;
  bool isNavigationTargetReached(int node) const override;

  void moveToNode(int node) override;
  void moveToPosition(const Vec3 &position, int node) override;
};

} // namespace ai
