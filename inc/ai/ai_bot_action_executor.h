//
// AiPB - AI action executor.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_action_executor.h>

namespace ai {

class ActionExecutionContext;

// Bridges high-level AI intents into runtime capabilities. Engine-specific integration is supplied by
// ActionExecutionContext implementations; actions without direct execution semantics remain under YaPB
// task-stack control.
class BotActionExecutor final : public ActionExecutor {
private:
  ActionExecutionContext *m_context {};
  Action m_observedTaskAction {};
  bool m_observedTaskActive {};
  Action m_directAttackAction {};
  bool m_directAttackTargetActive {};
  Action m_directAimAction {};
  bool m_directAimTargetActive {};
  Action m_directFollowPlayerAction {};
  bool m_directFollowPlayerActive {};
  Action m_directChangeWeaponAction {};
  bool m_directChangeWeaponActive {};
  Action m_directThrowGrenadeAction {};
  bool m_directThrowGrenadeActive {};
  Action m_directThrowFlashbangAction {};
  bool m_directThrowFlashbangActive {};
  Action m_directHuntAction {};
  bool m_directHuntTargetActive {};
  bool m_directSeekCoverActive {};
  bool m_directEscapeFromBombActive {};
  bool m_directRescueHostageActive {};
  bool m_directPlantBombActive {};
  bool m_directDefuseBombActive {};
  bool m_directPickupItemActive {};
  bool m_directFireBreakableActive {};
  bool m_directCampActive {};
  bool m_directWaitActive {};
  bool m_directHoldPositionActive {};
  bool m_directHideActive {};

private:
  ActionResult executeMoveToNode(const Action &action);
  ActionResult executeMoveToPosition(const Action &action);
  ActionResult executeAttackTarget(const Action &action, const Observation &observation);
  ActionResult executeAimAtTarget(const Action &action, const Observation &observation);
  ActionResult executeFollowPlayer(const Action &action, const Observation &observation);
  ActionResult executeChangeWeapon(const Action &action, const Observation &observation);
  ActionResult executeThrowGrenade(const Action &action, const Observation &observation);
  ActionResult executeThrowFlashbang(const Action &action, const Observation &observation);
  ActionResult executeHuntTarget(const Action &action, const Observation &observation);
  ActionResult executeSeekCover(const Action &action);
  ActionResult executeEscapeFromBomb(const Action &action, const Observation &observation);
  ActionResult executeRescueHostage(const Action &action, const Observation &observation);
  ActionResult executePlantBomb(const Action &action, const Observation &observation);
  ActionResult executeDefuseBomb(const Action &action, const Observation &observation);
  ActionResult executePickupItem(const Action &action);
  ActionResult executeFireBreakable(const Action &action);
  ActionResult executeCamp(const Action &action);
  ActionResult executeWait(const Action &action);
  ActionResult executeHoldPosition(const Action &action);
  ActionResult executeHide(const Action &action);
  ActionResult executeObservedTaskAction(const Action &action, const Observation &observation);

public:
  explicit BotActionExecutor(ActionExecutionContext &context);

  ActionResult execute(const Action &action, const Observation &observation) override;
  void cancel() override;

  bool isActionStillOwned(const Action &action) const;
  bool suppressesLegacyTaskExecution() const;
};

} // namespace ai
