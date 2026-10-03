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
  bool m_huntTargetActive {};
  int m_huntTargetPlayer { -1 };
  Vector m_huntTargetOrigin {};
  bool m_huntNavigationTaskCreated {};

public:
  explicit YaPBActionExecutionContext(Bot &bot);

  bool isAlive() const override;
  bool allowsNavigationOverride() const override;
  bool navigationNodeExists(int node) const override;
  int navigationNodeForPosition(const Vec3 &position) const override;
  bool isNavigationTargetReached(int node) const override;

  void moveToNode(int node) override;
  void moveToPosition(const Vec3 &position, int node) override;

  bool attackTarget(int targetPlayer) override;
  void cancelAttackTarget(int targetPlayer) override;

  bool huntTarget(int targetPlayer) override;
  bool isHuntTargetReached(int targetPlayer) const override;
  void cancelHuntTarget(int targetPlayer) override;
};

} // namespace ai
