//
// AiPB - AI action execution context.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_observation.h>

namespace ai {

// Provides the runtime capabilities required by BotActionExecutor without exposing engine or YaPB types.
class ActionExecutionContext {
public:
  virtual ~ActionExecutionContext() = default;

  virtual bool isAlive() const = 0;
  virtual bool allowsNavigationOverride() const = 0;
  virtual bool navigationNodeExists(int node) const = 0;
  virtual int navigationNodeForPosition(const Vec3 &position) const = 0;
  virtual bool isNavigationTargetReached(int node) const = 0;

  virtual void moveToNode(int node) = 0;
  virtual void moveToPosition(const Vec3 &position, int node) = 0;

  virtual bool attackTarget(int targetPlayer) = 0;
  virtual void cancelAttackTarget(int targetPlayer) = 0;

  virtual bool followPlayer(int targetPlayer) = 0;
  virtual void cancelFollowPlayer(int targetPlayer) = 0;

  virtual bool huntTarget(int targetPlayer) = 0;
  virtual bool isHuntTargetReached(int targetPlayer) const = 0;
  virtual void cancelHuntTarget(int targetPlayer) = 0;

  virtual bool seekCover() = 0;
  virtual bool isSeekCoverReached() const = 0;
  virtual void cancelSeekCover() = 0;

  virtual bool escapeFromBomb() = 0;
  virtual bool isEscapeFromBombReached() const = 0;
  virtual void cancelEscapeFromBomb() = 0;

  virtual bool plantBomb() = 0;
  virtual void cancelPlantBomb() = 0;

  virtual bool defuseBomb() = 0;
  virtual void cancelDefuseBomb() = 0;

  virtual bool pickupItem() = 0;
  virtual void cancelPickupItem() = 0;

  virtual bool fireBreakable() = 0;
  virtual void cancelFireBreakable() = 0;

  virtual bool camp() = 0;
  virtual void cancelCamp() = 0;

  virtual bool wait() = 0;
  virtual void cancelWait() = 0;

  virtual bool holdPosition() = 0;
  virtual void cancelHoldPosition() = 0;

  virtual bool hide() = 0;
  virtual void cancelHide() = 0;
};

} // namespace ai
