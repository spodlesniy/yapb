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
};

} // namespace ai
