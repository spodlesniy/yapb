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
  Vec3 m_huntTargetOrigin {};
  float m_huntBestDistance { -1.0f };
  float m_huntLastProgressTime { -1.0f };
  float m_huntSeenEvidenceTime { -1.0f };
  float m_huntNoiseEndTime { -1.0f };
  bool m_huntNavigationTaskCreated {};
  bool m_seekCoverActive {};
  int m_seekCoverNode { -1 };
  bool m_seekCoverNavigationTaskCreated {};
  bool m_retreatActive {};
  int m_retreatNode { -1 };
  bool m_retreatNavigationTaskCreated {};
  bool m_retreatHideTaskCreated {};
  bool m_exploreActive {};
  int m_exploreNode { -1 };
  bool m_exploreNavigationTaskCreated {};
  bool m_protectObjectiveActive {};
  int m_protectObjectiveNode { -1 };
  bool m_protectObjectiveNavigationTaskCreated {};
  bool m_reloadActive {};
  int m_reloadStateIssued { 0 };
  bool m_escapeFromBombActive {};
  int m_escapeFromBombNode { -1 };
  bool m_escapeFromBombNavigationTaskCreated {};
  bool m_escapeFromBombHoldTaskCreated {};
  bool m_rescueHostageActive {};
  int m_rescueHostageNode { -1 };
  bool m_rescueHostageNavigationTaskCreated {};

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
  bool aimAtTarget(int targetPlayer) override;
  void cancelAimAtTarget(int targetPlayer) override;

  bool followPlayer(int targetPlayer) override;
  void cancelFollowPlayer(int targetPlayer) override;
  bool changeWeapon(WeaponType weaponType) override;
  void cancelChangeWeapon() override;

  bool throwGrenade(const Vec3 &position) override;
  void cancelThrowGrenade() override;
  bool throwFlashbang(const Vec3 &position) override;
  void cancelThrowFlashbang() override;
  bool throwSmoke(const Vec3 &position) override;
  void cancelThrowSmoke() override;

  bool huntTarget(int targetPlayer, const Vec3 &position) override;
  bool isHuntTargetReached(int targetPlayer) const override;
  bool isHuntTargetStalled(int targetPlayer) const override;
  void consumeHuntTargetMemory(int targetPlayer) override;
  void cancelHuntTarget(int targetPlayer) override;

  bool seekCover() override;
  bool isSeekCoverReached() const override;
  void cancelSeekCover() override;

  bool retreat() override;
  bool isRetreatReached() const override;
  void cancelRetreat() override;

  bool explore() override;
  bool isExploreReached() const override;
  void cancelExplore() override;

  bool protectObjective() override;
  bool isProtectObjectiveReached() const override;
  void cancelProtectObjective() override;

  bool reload(WeaponType weaponType) override;
  bool isReloadCompleted() const override;
  void cancelReload() override;

  bool escapeFromBomb() override;
  bool isEscapeFromBombReached() const override;
  void cancelEscapeFromBomb() override;

  bool rescueHostage() override;
  void cancelRescueHostage() override;

  bool plantBomb() override;
  void cancelPlantBomb() override;

  bool defuseBomb() override;
  void cancelDefuseBomb() override;

  bool pickupItem() override;
  void cancelPickupItem() override;

  bool fireBreakable() override;
  void cancelFireBreakable() override;

  bool camp() override;
  void cancelCamp() override;

  bool wait() override;
  void cancelWait() override;

  bool holdPosition() override;
  void cancelHoldPosition() override;

  bool hide() override;
  void cancelHide() override;
};

} // namespace ai
