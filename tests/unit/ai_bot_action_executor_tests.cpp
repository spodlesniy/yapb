//
// AiPB - AI action executor unit tests.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include <cmath>
#include <limits>

#include <ai/ai_action_execution_context.h>
#include <ai/ai_bot_action_executor.h>
#include <ai/ai_goal_navigation_policy.h>
#include <ai/ai_hunt_progress_guard.h>
#include <ai/ai_training_collector.h>

#include "ai_test.h"

using ai::test::expect;
using ai::test::expectNear;

namespace {

class MockActionExecutionContext final : public ai::ActionExecutionContext {
public:
  bool alive { true };
  bool navigationOverrideAllowed { true };
  bool targetReached {};
  int nearestNode { 7 };

  int moveToNodeCalls {};
  int moveToPositionCalls {};
  int attackTargetCalls {};
  int cancelAttackTargetCalls {};
  int aimAtTargetCalls {};
  int cancelAimAtTargetCalls {};
  int followPlayerCalls {};
  int cancelFollowPlayerCalls {};
  int throwGrenadeCalls {};
  int cancelThrowGrenadeCalls {};
  int throwFlashbangCalls {};
  int cancelThrowFlashbangCalls {};
  int throwSmokeCalls {};
  int cancelThrowSmokeCalls {};
  ai::Vec3 lastThrowPosition {};
  ai::Vec3 lastFlashbangPosition {};
  ai::Vec3 lastThrowSmokeTarget {};
  int lastFollowPlayer { -1 };
  int changeWeaponCalls {};
  int cancelChangeWeaponCalls {};
  ai::WeaponType lastChangeWeapon { ai::WeaponType::Unknown };
  bool changeWeaponAvailable { true };
  int huntTargetCalls {};
  int cancelHuntTargetCalls {};
  int seekCoverCalls {};
  int cancelSeekCoverCalls {};
  bool retreatAvailable { true };
  bool retreatReached {};
  int retreatCalls {};
  int cancelRetreatCalls {};
  bool exploreAvailable { true };
  bool exploreReached {};
  int exploreCalls {};
  int cancelExploreCalls {};
  bool protectObjectiveAvailable { true };
  bool protectObjectiveReached {};
  int protectObjectiveCalls {};
  int cancelProtectObjectiveCalls {};
  bool reloadAvailable { true };
  bool reloadCompleted {};
  int reloadCalls {};
  int cancelReloadCalls {};
  ai::WeaponType lastReloadWeapon { ai::WeaponType::Unknown };
  int escapeFromBombCalls {};
  int cancelEscapeFromBombCalls {};
  int rescueHostageCalls {};
  int cancelRescueHostageCalls {};
  int lastNode { -1 };
  int lastAttackTarget { -1 };
  int lastAimTarget { -1 };
  int lastHuntTarget { -1 };
  ai::Vec3 lastHuntPosition {};
  bool followPlayerAvailable { true };
  bool huntTargetReached {};
  bool huntTargetStalled {};
  int consumeHuntTargetMemoryCalls {};
  int lastConsumedHuntTarget { -1 };
  bool seekCoverReached {};
  bool escapeFromBombReached {};
  bool rescueHostageAvailable { true };
  int plantBombCalls {};
  int cancelPlantBombCalls {};
  bool plantBombAvailable { true };
  int defuseBombCalls {};
  int cancelDefuseBombCalls {};
  int pickupItemCalls {};
  int cancelPickupItemCalls {};
  int fireBreakableCalls {};
  int cancelFireBreakableCalls {};
  bool pickupItemAvailable {};
  bool fireBreakableAvailable {};
  int campCalls {};
  int cancelCampCalls {};
  int waitCalls {};
  int cancelWaitCalls {};
  bool holdPositionAvailable {};
  int holdPositionCalls {};
  int cancelHoldPositionCalls {};
  bool hideAvailable {};
  int hideCalls {};
  int cancelHideCalls {};
  bool campAvailable {};
  bool waitAvailable {};
  ai::Vec3 lastPosition {};

  bool isAlive() const override {
    return alive;
  }

  bool allowsNavigationOverride() const override {
    return navigationOverrideAllowed;
  }

  bool navigationNodeExists(int node) const override {
    return node >= 0 && node < 100;
  }

  int navigationNodeForPosition(const ai::Vec3 &) const override {
    return nearestNode;
  }

  bool isNavigationTargetReached(int node) const override {
    return targetReached && node == nearestNode;
  }

  void moveToNode(int node) override {
    ++moveToNodeCalls;
    lastNode = node;
  }

  void moveToPosition(const ai::Vec3 &position, int node) override {
    ++moveToPositionCalls;
    lastPosition = position;
    lastNode = node;
  }

  bool attackTarget(int targetPlayer) override {
    ++attackTargetCalls;
    lastAttackTarget = targetPlayer;
    return true;
  }

  void cancelAttackTarget(int targetPlayer) override {
    ++cancelAttackTargetCalls;
    lastAttackTarget = targetPlayer;
  }

  bool aimAtTarget(int targetPlayer) override {
    ++aimAtTargetCalls;
    lastAimTarget = targetPlayer;
    return true;
  }

  void cancelAimAtTarget(int targetPlayer) override {
    ++cancelAimAtTargetCalls;
    lastAimTarget = targetPlayer;
  }

  bool followPlayer(int targetPlayer) override { ++followPlayerCalls; lastFollowPlayer = targetPlayer; return followPlayerAvailable; }
  void cancelFollowPlayer(int targetPlayer) override { ++cancelFollowPlayerCalls; lastFollowPlayer = targetPlayer; }
  bool changeWeapon(ai::WeaponType weaponType) override { ++changeWeaponCalls; lastChangeWeapon = weaponType; return changeWeaponAvailable; }
  void cancelChangeWeapon() override { ++cancelChangeWeaponCalls; }
  bool throwGrenade(const ai::Vec3 &position) override { ++throwGrenadeCalls; lastThrowPosition = position; return true; }
  void cancelThrowGrenade() override { ++cancelThrowGrenadeCalls; }
  bool throwFlashbang(const ai::Vec3 &position) override { ++throwFlashbangCalls; lastFlashbangPosition = position; return true; }
  void cancelThrowFlashbang() override { ++cancelThrowFlashbangCalls; }
  bool throwSmoke(const ai::Vec3 &position) override { ++throwSmokeCalls; lastThrowSmokeTarget = position; return true; }
  void cancelThrowSmoke() override { ++cancelThrowSmokeCalls; }

  bool huntTarget(int targetPlayer, const ai::Vec3 &position) override {
    ++huntTargetCalls;
    lastHuntTarget = targetPlayer;
    lastHuntPosition = position;
    return true;
  }

  bool isHuntTargetReached(int targetPlayer) const override {
    return huntTargetReached && targetPlayer == lastHuntTarget;
  }

  bool isHuntTargetStalled(int targetPlayer) const override {
    return huntTargetStalled && targetPlayer == lastHuntTarget;
  }

  void consumeHuntTargetMemory(int targetPlayer) override {
    ++consumeHuntTargetMemoryCalls;
    lastConsumedHuntTarget = targetPlayer;
  }

  void cancelHuntTarget(int targetPlayer) override {
    ++cancelHuntTargetCalls;
    lastHuntTarget = targetPlayer;
  }

  bool seekCover() override {
    ++seekCoverCalls;
    return true;
  }

  bool isSeekCoverReached() const override {
    return seekCoverReached;
  }

  void cancelSeekCover() override {
    ++cancelSeekCoverCalls;
  }

  bool retreat() override {
    ++retreatCalls;
    return retreatAvailable;
  }

  bool isRetreatReached() const override {
    return retreatReached;
  }

  void cancelRetreat() override {
    ++cancelRetreatCalls;
  }

  bool explore() override {
    ++exploreCalls;
    return exploreAvailable;
  }

  bool isExploreReached() const override {
    return exploreReached;
  }

  void cancelExplore() override {
    ++cancelExploreCalls;
  }

  bool protectObjective() override {
    ++protectObjectiveCalls;
    return protectObjectiveAvailable;
  }

  bool isProtectObjectiveReached() const override {
    return protectObjectiveReached;
  }

  void cancelProtectObjective() override {
    ++cancelProtectObjectiveCalls;
  }

  bool reload(ai::WeaponType weaponType) override {
    ++reloadCalls;
    lastReloadWeapon = weaponType;
    return reloadAvailable;
  }

  bool isReloadCompleted() const override {
    return reloadCompleted;
  }

  void cancelReload() override {
    ++cancelReloadCalls;
  }

  bool escapeFromBomb() override {
    ++escapeFromBombCalls;
    return true;
  }

  bool isEscapeFromBombReached() const override {
    return escapeFromBombReached;
  }

  void cancelEscapeFromBomb() override {
    ++cancelEscapeFromBombCalls;
  }

  bool rescueHostage() override {
    ++rescueHostageCalls;
    return rescueHostageAvailable;
  }

  void cancelRescueHostage() override {
    ++cancelRescueHostageCalls;
  }

  bool plantBomb() override {
    ++plantBombCalls;
    return plantBombAvailable;
  }

  void cancelPlantBomb() override {
    ++cancelPlantBombCalls;
  }

  bool defuseBomb() override {
    ++defuseBombCalls;
    return true;
  }

  void cancelDefuseBomb() override {
    ++cancelDefuseBombCalls;
  }

  bool pickupItem() override {
    ++pickupItemCalls;
    return pickupItemAvailable;
  }

  void cancelPickupItem() override {
    ++cancelPickupItemCalls;
  }

  bool fireBreakable() override {
    ++fireBreakableCalls;
    return fireBreakableAvailable;
  }

  void cancelFireBreakable() override {
    ++cancelFireBreakableCalls;
  }

  bool camp() override {
    ++campCalls;
    return campAvailable;
  }

  void cancelCamp() override {
    ++cancelCampCalls;
  }

  bool wait() override {
    ++waitCalls;
    return waitAvailable;
  }

  void cancelWait() override {
    ++cancelWaitCalls;
  }

  bool holdPosition() override {
    ++holdPositionCalls;
    return holdPositionAvailable;
  }

  void cancelHoldPosition() override {
    ++cancelHoldPositionCalls;
  }

  bool hide() override {
    ++hideCalls;
    return hideAvailable;
  }

  void cancelHide() override {
    ++cancelHideCalls;
  }
};

ai::Observation aliveObservation() {
  ai::Observation observation {};
  observation.bot.alive = true;
  return observation;
}

AI_TEST(testBotActionExecutorExecutesFollowPlayer) {
  auto context = MockActionExecutionContext {};
  auto observation = aliveObservation();
  observation.bot.followTargetPlayer = 7;
  ai::Action action {};
  action.type = ai::ActionType::FollowPlayer;
  action.targetType = ai::TargetType::Player;
  action.targetPlayer = 7;
  const auto result = ai::BotActionExecutor { context }.execute(action, observation);
  expect(result.type == ai::ActionResultType::Accepted, "follow action is accepted");
  expect(context.followPlayerCalls == 1, "follow action invokes context");
}

AI_TEST(testBotActionExecutorCompletesFollowPlayerWhenTargetChanges) {
  auto context = MockActionExecutionContext {};
  auto observation = aliveObservation();
  observation.bot.followTargetPlayer = 7;
  ai::Action action {};
  action.type = ai::ActionType::FollowPlayer;
  action.targetType = ai::TargetType::Player;
  action.targetPlayer = 7;
  ai::BotActionExecutor executor { context };
  expect(executor.execute(action, observation).type == ai::ActionResultType::Accepted, "follow action starts");
  observation.bot.followTargetPlayer = 8;
  expect(executor.execute(action, observation).type == ai::ActionResultType::Completed, "follow action completes when target changes");
  expect(context.cancelFollowPlayerCalls == 1, "follow action cancels target");
}



} // namespace

AI_TEST(testBotActionExecutorDirectlyExecutesThrowGrenade) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  auto observation = aliveObservation();
  observation.bot.currentTask = ai::TaskType::ThrowExplosive;
  ai::Action action {};
  action.type = ai::ActionType::ThrowGrenade;
  action.targetType = ai::TargetType::Position;
  action.targetPosition = { 100.0f, 200.0f, 300.0f };
  action.grenadeType = ai::GrenadeType::HE;
  const auto result = executor.execute(action, observation);
  expect(result.type == ai::ActionResultType::Accepted, "grenade action is accepted");
  expect(context.throwGrenadeCalls == 1, "grenade action invokes context");
  expectNear(context.lastThrowPosition.x, 100.0f, 0.001f, "grenade target x is preserved");
}

AI_TEST(testBotActionExecutorCompletesThrowGrenadeWhenTaskEnds) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  auto observation = aliveObservation();
  observation.bot.currentTask = ai::TaskType::ThrowExplosive;
  ai::Action action {};
  action.type = ai::ActionType::ThrowGrenade;
  action.targetType = ai::TargetType::Position;
  action.targetPosition = { 100.0f, 200.0f, 300.0f };
  action.grenadeType = ai::GrenadeType::HE;
  expect(executor.execute(action, observation).type == ai::ActionResultType::Accepted, "grenade action starts");
  observation.bot.currentTask = ai::TaskType::Normal;
  expect(executor.execute(action, observation).type == ai::ActionResultType::Completed, "grenade action completes");
  expect(context.cancelThrowGrenadeCalls == 1, "grenade cancellation is delegated");
}

AI_TEST(testBotActionExecutorDirectlyExecutesThrowSmoke) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  auto observation = aliveObservation();
  observation.bot.currentTask = ai::TaskType::ThrowSmoke;
  ai::Action action {};
  action.type = ai::ActionType::ThrowSmoke;
  action.targetType = ai::TargetType::Position;
  action.targetPosition = { 100.0f, 200.0f, 300.0f };
  action.grenadeType = ai::GrenadeType::Smoke;

  const auto result = executor.execute(action, observation);

  expect(result.type == ai::ActionResultType::Accepted, "smoke action is accepted");
  expect(context.throwSmokeCalls == 1, "smoke action invokes context");
  expectNear(context.lastThrowSmokeTarget.x, 100.0f, 0.001f, "smoke target x is preserved");
}

AI_TEST(testBotActionExecutorCompletesThrowSmokeWhenTaskEnds) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  auto observation = aliveObservation();
  observation.bot.currentTask = ai::TaskType::ThrowSmoke;
  ai::Action action {};
  action.type = ai::ActionType::ThrowSmoke;
  action.targetType = ai::TargetType::Position;
  action.targetPosition = { 100.0f, 200.0f, 300.0f };
  action.grenadeType = ai::GrenadeType::Smoke;

  expect(executor.execute(action, observation).type == ai::ActionResultType::Accepted, "smoke action starts");
  observation.bot.currentTask = ai::TaskType::Normal;
  expect(executor.execute(action, observation).type == ai::ActionResultType::Completed, "smoke action completes");
  expect(context.cancelThrowSmokeCalls == 1, "smoke cancellation is delegated");
}

AI_TEST(testBotActionExecutorDirectlyExecutesThrowFlashbang) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  auto observation = aliveObservation();
  observation.bot.currentTask = ai::TaskType::ThrowFlashbang;
  ai::Action action {};
  action.type = ai::ActionType::ThrowFlashbang;
  action.targetType = ai::TargetType::Position;
  action.targetPosition = { 150.0f, 250.0f, 350.0f };
  action.grenadeType = ai::GrenadeType::Flashbang;
  const auto result = executor.execute(action, observation);
  expect(result.type == ai::ActionResultType::Accepted, "flashbang action is accepted");
  expect(context.throwFlashbangCalls == 1, "flashbang action invokes context");
  expectNear(context.lastFlashbangPosition.y, 250.0f, 0.001f, "flashbang target y is preserved");
}

AI_TEST(testBotActionExecutorCompletesThrowFlashbangWhenTaskEnds) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  auto observation = aliveObservation();
  observation.bot.currentTask = ai::TaskType::ThrowFlashbang;
  ai::Action action {};
  action.type = ai::ActionType::ThrowFlashbang;
  action.targetType = ai::TargetType::Position;
  action.targetPosition = { 150.0f, 250.0f, 350.0f };
  action.grenadeType = ai::GrenadeType::Flashbang;
  expect(executor.execute(action, observation).type == ai::ActionResultType::Accepted, "flashbang action starts");
  observation.bot.currentTask = ai::TaskType::Normal;
  expect(executor.execute(action, observation).type == ai::ActionResultType::Completed, "flashbang action completes");
  expect(context.cancelThrowFlashbangCalls == 1, "flashbang cancellation is delegated");
}

AI_TEST(testBotActionExecutorDirectlyExecutesChangeWeapon) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  auto observation = aliveObservation();
  observation.combat.weaponType = ai::WeaponType::Pistol;
  ai::Action action {};
  action.type = ai::ActionType::ChangeWeapon;
  action.weaponType = ai::WeaponType::Rifle;
  const auto result = executor.execute(action, observation);
  expect(result.type == ai::ActionResultType::Accepted, "change weapon action is accepted");
  expect(context.changeWeaponCalls == 1, "change weapon action invokes context");
  expect(context.lastChangeWeapon == ai::WeaponType::Rifle, "requested weapon type is preserved");
}

AI_TEST(testBotActionExecutorCompletesChangeWeaponWhenTargetIsEquipped) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  auto observation = aliveObservation();
  observation.combat.weaponType = ai::WeaponType::Pistol;
  ai::Action action {};
  action.type = ai::ActionType::ChangeWeapon;
  action.weaponType = ai::WeaponType::Rifle;
  expect(executor.execute(action, observation).type == ai::ActionResultType::Accepted, "change weapon action starts");
  observation.combat.weaponType = ai::WeaponType::Rifle;
  expect(executor.execute(action, observation).type == ai::ActionResultType::Completed, "change weapon action completes");
  expect(context.cancelChangeWeaponCalls == 1, "change weapon cancellation is delegated");
}

AI_TEST(testBotActionExecutorRejectsUnavailableChangeWeapon) {
  MockActionExecutionContext context {};
  context.changeWeaponAvailable = false;
  ai::BotActionExecutor executor(context);
  auto observation = aliveObservation();
  observation.combat.weaponType = ai::WeaponType::Pistol;
  ai::Action action {};
  action.type = ai::ActionType::ChangeWeapon;
  action.weaponType = ai::WeaponType::Rifle;
  expect(executor.execute(action, observation).type == ai::ActionResultType::Rejected, "unavailable weapon is rejected");
}

AI_TEST(testBotActionExecutorMovesToNodeThroughContext) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::MoveToNode;
  action.targetNode = 12;

  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Accepted, "valid node action is accepted");
  expect(context.moveToNodeCalls == 1, "node move is delegated once");
  expect(context.lastNode == 12, "delegated node is preserved");
}

AI_TEST(testBotActionExecutorRejectsUnknownNode) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::MoveToNode;
  action.targetNode = 1000;

  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Rejected, "unknown node is rejected");
  expect(context.moveToNodeCalls == 0, "unknown node is not delegated");
}

AI_TEST(testBotActionExecutorCompletesReachedNode) {
  MockActionExecutionContext context {};
  context.nearestNode = 12;
  context.targetReached = true;
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::MoveToNode;
  action.targetNode = 12;

  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Completed, "reached node completes the action");
  expect(context.moveToNodeCalls == 0, "reached node is not delegated");
}

AI_TEST(testBotActionExecutorInterruptsNavigationWithoutOwnership) {
  MockActionExecutionContext context {};
  context.navigationOverrideAllowed = false;
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::MoveToNode;
  action.targetNode = 12;

  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Interrupted, "navigation loses ownership when override is disallowed");
  expect(context.moveToNodeCalls == 0, "navigation is not delegated after ownership loss");
}

AI_TEST(testBotActionExecutorKeepsSemanticActionsOutsideGenericNavigationOwnership) {
  MockActionExecutionContext context {};
  context.navigationOverrideAllowed = false;
  ai::BotActionExecutor executor(context);

  ai::Action attack {};
  attack.type = ai::ActionType::AttackTarget;
  attack.targetType = ai::TargetType::Player;
  attack.targetPlayer = 9;
  expect(executor.isActionStillOwned(attack), "attack ownership does not depend on generic navigation state");

  ai::Action hunt {};
  hunt.type = ai::ActionType::HuntTarget;
  hunt.targetType = ai::TargetType::Player;
  hunt.targetPlayer = 9;
  expect(executor.isActionStillOwned(hunt), "hunt ownership is validated by the hunt execution context");

  ai::Action cover {};
  cover.type = ai::ActionType::SeekCover;
  expect(executor.isActionStillOwned(cover), "seek-cover ownership is validated by the cover execution context");

  ai::Action retreat {};
  retreat.type = ai::ActionType::Retreat;
  expect(executor.isActionStillOwned(retreat), "retreat ownership is validated by the retreat execution context");

  ai::Action protect {};
  protect.type = ai::ActionType::ProtectObjective;
  expect(executor.isActionStillOwned(protect), "objective protection owns its task-specific lifecycle");

  ai::Action escape {};
  escape.type = ai::ActionType::EscapeFromBomb;
  expect(executor.isActionStillOwned(escape), "bomb escape owns its task-specific lifecycle");
}

AI_TEST(testBotActionExecutorRejectsInactiveContext) {
  MockActionExecutionContext context {};
  context.alive = false;
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::MoveToNode;
  action.targetNode = 12;

  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Rejected, "inactive context rejects execution");
}

AI_TEST(testBotActionExecutorMovesToPositionThroughContext) {
  MockActionExecutionContext context {};
  context.nearestNode = 21;
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::MoveToPosition;
  action.targetPosition = { 10.0f, 20.0f, 30.0f };

  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Accepted, "valid position action is accepted");
  expect(context.moveToPositionCalls == 1, "position move is delegated once");
  expect(context.lastNode == 21, "position uses the nearest waypoint");
  expectNear(context.lastPosition.x, 10.0f, 0.001f, "x position is preserved");
  expectNear(context.lastPosition.y, 20.0f, 0.001f, "y position is preserved");
  expectNear(context.lastPosition.z, 30.0f, 0.001f, "z position is preserved");
}

AI_TEST(testBotActionExecutorRejectsNonFinitePosition) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::MoveToPosition;
  action.targetPosition.x = std::numeric_limits<float>::quiet_NaN();

  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Invalid, "non-finite position is invalid");
  expect(context.moveToPositionCalls == 0, "invalid position is not delegated");
}

ai::Observation attackObservation(int targetPlayer) {
  auto observation = aliveObservation();
  observation.combat.enemyEntity = targetPlayer;
  observation.combat.perceptionFlags = static_cast<uint32_t>(ai::PerceptionFlag::SeeingEnemy);
  observation.playerCount = 1;
  observation.players[0].entityIndex = targetPlayer;
  observation.players[0].valid = true;
  observation.players[0].alive = true;
  observation.players[0].enemy = true;
  observation.players[0].visible = true;
  return observation;
}

AI_TEST(testBotActionExecutorDirectlyExecutesAimAtTarget) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  ai::Action action {};
  action.type = ai::ActionType::AimAtTarget;
  action.targetType = ai::TargetType::Player;
  action.targetPlayer = 9;

  const auto result = executor.execute(action, attackObservation(9));

  expect(result.type == ai::ActionResultType::Accepted, "aim action is accepted");
  expect(context.aimAtTargetCalls == 1, "aim target is delegated");
  expect(context.lastAimTarget == 9, "aim target is preserved");
  expect(executor.suppressesLegacyTaskExecution(), "direct aim owns task execution");
}

AI_TEST(testBotActionExecutorCompletesAimWhenTargetChanges) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  ai::Action action {};
  action.type = ai::ActionType::AimAtTarget;
  action.targetType = ai::TargetType::Player;
  action.targetPlayer = 9;

  expect(executor.execute(action, attackObservation(9)).type == ai::ActionResultType::Accepted, "aim action starts");
  const auto result = executor.execute(action, attackObservation(10));

  expect(result.type == ai::ActionResultType::Completed, "changed target completes active aim");
  expect(context.cancelAimAtTargetCalls == 1, "completed aim releases target");
  expect(!executor.suppressesLegacyTaskExecution(), "completed aim releases ownership");
}

AI_TEST(testBotActionExecutorRejectsAimWithoutVisibleTarget) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  ai::Action action {};
  action.type = ai::ActionType::AimAtTarget;
  action.targetType = ai::TargetType::Player;
  action.targetPlayer = 9;

  auto observation = attackObservation(9);
  observation.players[0].visible = false;

  const auto result = executor.execute(action, observation);

  expect(result.type == ai::ActionResultType::Rejected, "aim without visible target is rejected");
  expect(context.aimAtTargetCalls == 0, "invalid aim target is not delegated");
}

AI_TEST(testBotActionExecutorDirectlyExecutesAttackTarget) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  ai::Action action {};
  action.type = ai::ActionType::AttackTarget;
  action.targetType = ai::TargetType::Player;
  action.targetPlayer = 9;
  const auto result = executor.execute(action, attackObservation(9));
  expect(result.type == ai::ActionResultType::Accepted, "visible enemy attack is accepted");
  expect(context.attackTargetCalls == 1, "attack target is delegated");
  expect(context.lastAttackTarget == 9, "attack target is preserved");
  expect(executor.suppressesLegacyTaskExecution(), "direct attack owns task execution");
}

AI_TEST(testBotActionExecutorCompletesAttackWhenTargetChanges) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  ai::Action action {};
  action.type = ai::ActionType::AttackTarget;
  action.targetType = ai::TargetType::Player;
  action.targetPlayer = 9;
  executor.execute(action, attackObservation(9));
  const auto result = executor.execute(action, attackObservation(10));
  expect(result.type == ai::ActionResultType::Completed, "changed target completes active attack");
  expect(context.cancelAttackTargetCalls == 1, "completed attack releases target");
  expect(!executor.suppressesLegacyTaskExecution(), "completed attack releases ownership");
}

ai::Observation huntObservation(int targetPlayer) {
  auto observation = aliveObservation();
  observation.bot.origin = { 100.0f, 200.0f, 300.0f };
  observation.combat.lastEnemyEntity = targetPlayer;
  observation.combat.lastEnemyRelativeOrigin = { 25.0f, -50.0f, 10.0f };
  observation.playerCount = 1;
  observation.players[0].entityIndex = targetPlayer;
  observation.players[0].valid = true;
  observation.players[0].alive = true;
  observation.players[0].enemy = true;
  return observation;
}

AI_TEST(testBotActionExecutorAcceptsTeacherLastEnemyHunt) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  auto observation = huntObservation(9);
  observation.bot.currentTask = ai::TaskType::Hunt;
  observation.bot.currentGoalNode = 20;
  const auto action = ai::GoalNavigationPolicy {}.decide(observation);

  expect(action.type == ai::ActionType::HuntTarget, "teacher produces a semantic hunt for the last enemy");
  expect(executor.execute(action, observation).type == ai::ActionResultType::Accepted,
         "executor accepts the teacher's remembered-enemy target without a current enemy");
  expect(context.huntTargetCalls == 1 && context.lastHuntTarget == 9, "last enemy is delegated to hunt execution");
  expect(context.lastHuntPosition.x == 125.0f && context.lastHuntPosition.y == 150.0f
             && context.lastHuntPosition.z == 310.0f,
         "hunt execution receives the remembered enemy position from observation");
}

AI_TEST(testBotActionExecutorDirectlyExecutesHuntTarget) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::HuntTarget;
  action.targetType = ai::TargetType::Player;
  action.targetPlayer = 9;

  const auto result = executor.execute(action, huntObservation(9));

  expect(result.type == ai::ActionResultType::Accepted, "remembered enemy hunt is accepted");
  expect(context.huntTargetCalls == 1, "hunt target is delegated");
  expect(context.lastHuntTarget == 9, "hunt target is preserved");
  expect(context.lastHuntPosition.x == 125.0f && context.lastHuntPosition.y == 150.0f
             && context.lastHuntPosition.z == 310.0f,
         "hunt uses remembered coordinates instead of a live player position");
  expect(!executor.suppressesLegacyTaskExecution(), "hunt keeps legacy task execution enabled");
}

AI_TEST(testBotActionExecutorDirectlyExecutesSeekCover) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::SeekCover;

  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Accepted, "seek cover is accepted");
  expect(context.seekCoverCalls == 1, "seek cover is delegated");
  expect(!executor.suppressesLegacyTaskExecution(), "seek cover keeps legacy task execution enabled");
}

AI_TEST(testBotActionExecutorCompletesSeekCoverWhenReached) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::SeekCover;

  auto result = executor.execute(action, aliveObservation());
  expect(result.type == ai::ActionResultType::Accepted, "seek cover starts before completion");

  context.seekCoverReached = true;
  result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Completed, "reached cover completes the active action");
  expect(context.cancelSeekCoverCalls == 1, "completion releases the active cover action");
}

AI_TEST(testBotActionExecutorCancelsSeekCover) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::SeekCover;

  executor.execute(action, aliveObservation());
  executor.cancel();

  expect(context.cancelSeekCoverCalls == 1, "cancel releases direct seek cover");
}

AI_TEST(testBotActionExecutorDirectlyExecutesRetreat) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  auto action = ai::Action {};
  action.type = ai::ActionType::Retreat;
  const auto result = executor.execute(action, aliveObservation());
  expect(result.type == ai::ActionResultType::Accepted, "retreat is accepted");
  expect(context.retreatCalls == 1, "retreat is delegated");
  expect(!executor.suppressesLegacyTaskExecution(), "retreat keeps legacy task execution enabled");
}

AI_TEST(testBotActionExecutorCompletesRetreatWhenReached) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  auto action = ai::Action {};
  action.type = ai::ActionType::Retreat;
  auto result = executor.execute(action, aliveObservation());
  expect(result.type == ai::ActionResultType::Accepted, "retreat starts before reaching the retreat point");
  context.retreatReached = true;
  result = executor.execute(action, aliveObservation());
  expect(result.type == ai::ActionResultType::Completed, "reaching the retreat point completes the action");
  expect(context.cancelRetreatCalls == 1, "completion releases direct retreat");
}

AI_TEST(testBotActionExecutorInterruptsRetreatWhenRuntimeStopsIt) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  auto action = ai::Action {};
  action.type = ai::ActionType::Retreat;
  auto result = executor.execute(action, aliveObservation());
  expect(result.type == ai::ActionResultType::Accepted, "retreat starts while available");
  context.retreatAvailable = false;
  result = executor.execute(action, aliveObservation());
  expect(result.type == ai::ActionResultType::Interrupted, "retreat is interrupted when the runtime is preempted");
  expect(context.cancelRetreatCalls == 1, "runtime preemption releases direct retreat");
}

AI_TEST(testBotActionExecutorRejectsRetreatWhenRuntimeCannotStartIt) {
  MockActionExecutionContext context {};
  context.retreatAvailable = false;
  ai::BotActionExecutor executor(context);
  auto action = ai::Action {};
  action.type = ai::ActionType::Retreat;
  const auto result = executor.execute(action, aliveObservation());
  expect(result.type == ai::ActionResultType::Rejected, "retreat is rejected when runtime cannot start it");
  expect(context.retreatCalls == 1, "retreat availability is checked");
}

AI_TEST(testBotActionExecutorCancelsDirectRetreat) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  auto action = ai::Action {};
  action.type = ai::ActionType::Retreat;
  executor.execute(action, aliveObservation());
  executor.cancel();
  expect(context.cancelRetreatCalls == 1, "cancel releases direct retreat");
}

AI_TEST(testBotActionExecutorCompletesHuntWhenTargetPositionIsReached) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::HuntTarget;
  action.targetType = ai::TargetType::Player;
  action.targetPlayer = 9;

  auto result = executor.execute(action, huntObservation(9));
  expect(result.type == ai::ActionResultType::Accepted, "hunt starts before target is reached");
  expect(context.huntTargetCalls == 1, "hunt target is delegated before completion");

  context.huntTargetReached = true;
  result = executor.execute(action, huntObservation(9));

  expect(result.type == ai::ActionResultType::Completed, "reached hunt target completes the active action");
  expect(context.consumeHuntTargetMemoryCalls == 1 && context.lastConsumedHuntTarget == 9,
         "reaching the remembered position consumes that hunt memory");
  expect(context.cancelHuntTargetCalls == 1, "completion releases the active hunt target");
}


AI_TEST(testHuntProgressGuardRequiresMeaningfulProgress) {
  expect(ai::hasMeaningfulHuntProgress(-1.0f, 1200.0f),
         "first hunt sample initializes progress");
  expect(!ai::hasMeaningfulHuntProgress(1200.0f, 1150.0f),
         "small movement does not continuously refresh the hunt watchdog");
  expect(ai::hasMeaningfulHuntProgress(1200.0f, 1136.0f),
         "64 units of approach counts as meaningful hunt progress");
  expect(!ai::isHuntProgressStalled(7.99f, 0.0f),
         "hunt remains active inside the no-progress window");
  expect(ai::isHuntProgressStalled(8.0f, 0.0f),
         "hunt stalls after a full no-progress window");
  expect(!ai::hasNewerHuntEvidence(10.0f, 20.0f, 10.0f, 20.0f),
         "unchanged perception evidence remains consumable");
  expect(ai::hasNewerHuntEvidence(10.1f, 20.0f, 10.0f, 20.0f),
         "new visual evidence protects remembered enemy state");
  expect(ai::hasNewerHuntEvidence(10.0f, 20.1f, 10.0f, 20.0f),
         "new sound evidence protects remembered enemy state");
}

AI_TEST(testBotActionExecutorInterruptsStalledHuntTarget) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::HuntTarget;
  action.targetType = ai::TargetType::Player;
  action.targetPlayer = 9;

  auto result = executor.execute(action, huntObservation(9));
  expect(result.type == ai::ActionResultType::Accepted, "hunt starts before it stalls");

  context.huntTargetStalled = true;
  result = executor.execute(action, huntObservation(9));

  expect(result.type == ai::ActionResultType::Interrupted,
         "stalled remembered-enemy hunt is interrupted rather than completed");
  expect(context.consumeHuntTargetMemoryCalls == 1 && context.lastConsumedHuntTarget == 9,
         "stall interruption consumes the exhausted remembered position");
  expect(context.cancelHuntTargetCalls == 1, "stall interruption releases the active hunt target");
}

AI_TEST(testBotActionExecutorInterruptsActiveHuntWhenBombObjectiveStarts) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::HuntTarget;
  action.targetType = ai::TargetType::Player;
  action.targetPlayer = 9;

  auto observation = huntObservation(9);
  observation.bot.team = 1;

  auto result = executor.execute(action, observation);
  expect(result.type == ai::ActionResultType::Accepted, "hunt starts before the bomb objective is active");

  observation.bot.objectiveFlags |= ai::ObjectiveFlag::BombPlanted;
  result = executor.execute(action, observation);

  expect(result.type == ai::ActionResultType::Interrupted, "planted bomb interrupts an active remembered-enemy hunt");
  expect(context.consumeHuntTargetMemoryCalls == 0,
         "objective preemption preserves remembered enemy evidence for later decisions");
  expect(context.cancelHuntTargetCalls == 1, "bomb-objective interruption releases the active hunt target");
}

AI_TEST(testBotActionExecutorRejectsNewHuntDuringBombObjective) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::HuntTarget;
  action.targetType = ai::TargetType::Player;
  action.targetPlayer = 9;

  auto observation = huntObservation(9);
  observation.bot.team = 0;
  observation.bot.objectiveFlags |= ai::ObjectiveFlag::BombDropped;

  const auto result = executor.execute(action, observation);

  expect(result.type == ai::ActionResultType::Rejected, "dropped C4 rejects a new Terrorist remembered-enemy hunt");
  expect(context.huntTargetCalls == 0, "rejected bomb-objective hunt is not delegated");
}

AI_TEST(testBotActionExecutorCancelsDirectHunt) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::HuntTarget;
  action.targetType = ai::TargetType::Player;
  action.targetPlayer = 9;

  executor.execute(action, huntObservation(9));
  executor.cancel();

  expect(context.cancelHuntTargetCalls == 1, "cancel releases direct hunt");
  expect(context.consumeHuntTargetMemoryCalls == 0,
         "generic cancellation does not consume remembered enemy evidence");
  expect(context.lastHuntTarget == 9, "cancel releases active hunt target");
}

AI_TEST(testBotActionExecutorCancelReleasesDirectAttack) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  ai::Action action {};
  action.type = ai::ActionType::AttackTarget;
  action.targetType = ai::TargetType::Player;
  action.targetPlayer = 9;
  executor.execute(action, attackObservation(9));
  executor.cancel();
  expect(context.cancelAttackTargetCalls == 1, "cancel releases direct attack");
  expect(!executor.suppressesLegacyTaskExecution(), "cancel releases ownership");
}


AI_TEST(testBotActionExecutorDirectlyExecutesExplore) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  auto action = ai::Action {};
  action.type = ai::ActionType::Explore;

  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Accepted, "explore is accepted");
  expect(context.exploreCalls == 1, "explore is delegated");
  expect(!executor.suppressesLegacyTaskExecution(), "explore keeps legacy task execution enabled");
}

AI_TEST(testBotActionExecutorCompletesExploreWhenReached) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  auto action = ai::Action {};
  action.type = ai::ActionType::Explore;

  auto result = executor.execute(action, aliveObservation());
  expect(result.type == ai::ActionResultType::Accepted, "explore starts before reaching the waypoint");

  context.exploreReached = true;
  result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Completed, "reaching the exploration waypoint completes the action");
  expect(context.cancelExploreCalls == 1, "completion releases direct explore");
}

AI_TEST(testBotActionExecutorCompletesExploreWhenRuntimeStopsIt) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  auto action = ai::Action {};
  action.type = ai::ActionType::Explore;

  auto result = executor.execute(action, aliveObservation());
  expect(result.type == ai::ActionResultType::Accepted, "explore starts while available");

  context.exploreAvailable = false;
  result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Completed, "explore completes when the runtime stops it");
  expect(context.cancelExploreCalls == 1, "runtime stop releases direct explore");
}

AI_TEST(testBotActionExecutorRejectsExploreWhenRuntimeCannotStartIt) {
  MockActionExecutionContext context {};
  context.exploreAvailable = false;
  ai::BotActionExecutor executor(context);
  auto action = ai::Action {};
  action.type = ai::ActionType::Explore;

  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Rejected, "explore is rejected when runtime cannot start it");
  expect(context.exploreCalls == 1, "explore availability is checked");
}

AI_TEST(testBotActionExecutorCancelsDirectExplore) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  auto action = ai::Action {};
  action.type = ai::ActionType::Explore;

  executor.execute(action, aliveObservation());
  executor.cancel();

  expect(context.cancelExploreCalls == 1, "cancel releases direct explore");
}

AI_TEST(testBotActionExecutorDirectlyExecutesProtectObjective) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  auto action = ai::Action {};
  action.type = ai::ActionType::ProtectObjective;

  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Accepted, "protect objective is accepted");
  expect(context.protectObjectiveCalls == 1, "protect objective is delegated");
  expect(!executor.suppressesLegacyTaskExecution(), "protect objective keeps legacy task execution enabled");
}

AI_TEST(testBotActionExecutorCompletesProtectObjectiveWhenReached) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  auto action = ai::Action {};
  action.type = ai::ActionType::ProtectObjective;

  auto result = executor.execute(action, aliveObservation());
  expect(result.type == ai::ActionResultType::Accepted, "protect objective starts while the objective is active");

  context.protectObjectiveReached = true;
  result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Completed, "completed objective ends protection");
  expect(context.cancelProtectObjectiveCalls == 1, "completion releases direct objective protection");
}

AI_TEST(testBotActionExecutorInterruptsProtectObjectiveWhenRuntimeIsPreempted) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  auto action = ai::Action {};
  action.type = ai::ActionType::ProtectObjective;

  expect(executor.execute(action, aliveObservation()).type == ai::ActionResultType::Accepted,
         "protect objective starts before tactical preemption");

  context.protectObjectiveAvailable = false;
  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Interrupted,
         "runtime preemption interrupts objective protection instead of completing it");
  expect(context.cancelProtectObjectiveCalls == 1, "preemption releases direct objective protection");
}


AI_TEST(testBotActionExecutorRejectsProtectObjectiveWhenRuntimeCannotStartIt) {
  MockActionExecutionContext context {};
  context.protectObjectiveAvailable = false;
  ai::BotActionExecutor executor(context);
  auto action = ai::Action {};
  action.type = ai::ActionType::ProtectObjective;

  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Rejected, "protect objective is rejected when unavailable");
  expect(context.protectObjectiveCalls == 1, "objective protection availability is checked");
}

AI_TEST(testBotActionExecutorCancelsDirectProtectObjective) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  auto action = ai::Action {};
  action.type = ai::ActionType::ProtectObjective;

  executor.execute(action, aliveObservation());
  executor.cancel();

  expect(context.cancelProtectObjectiveCalls == 1, "cancel releases direct objective protection");
}

AI_TEST(testBotActionExecutorDirectlyExecutesReload) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  auto action = ai::Action {};
  action.type = ai::ActionType::Reload;

  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Accepted, "reload is accepted");
  expect(context.reloadCalls == 1, "reload is delegated");
  expect(context.lastReloadWeapon == ai::WeaponType::Unknown, "reload defaults to automatic weapon selection");
}

AI_TEST(testBotActionExecutorCompletesReloadWhenRuntimeFinishes) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  auto action = ai::Action {};
  action.type = ai::ActionType::Reload;

  expect(executor.execute(action, aliveObservation()).type == ai::ActionResultType::Accepted, "reload starts");
  context.reloadCompleted = true;
  expect(executor.execute(action, aliveObservation()).type == ai::ActionResultType::Completed, "reload completes");
  expect(context.cancelReloadCalls == 1, "reload completion clears runtime state");
}

AI_TEST(testBotActionExecutorRejectsReloadWhenRuntimeCannotStartIt) {
  MockActionExecutionContext context {};
  context.reloadAvailable = false;
  ai::BotActionExecutor executor(context);
  auto action = ai::Action {};
  action.type = ai::ActionType::Reload;

  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Rejected, "reload is rejected when unavailable");
  expect(context.reloadCalls == 1, "reload availability is checked");
}

AI_TEST(testBotActionExecutorFailsReloadWhenActiveRuntimeStopsIt) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  auto action = ai::Action {};
  action.type = ai::ActionType::Reload;

  expect(executor.execute(action, aliveObservation()).type == ai::ActionResultType::Accepted, "reload starts");
  context.reloadAvailable = false;
  expect(executor.execute(action, aliveObservation()).type == ai::ActionResultType::Failed, "active reload reports a runtime failure");
  expect(context.cancelReloadCalls == 1, "failed reload is cancelled");
}

AI_TEST(testBotActionExecutorCancelsDirectReload) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  auto action = ai::Action {};
  action.type = ai::ActionType::Reload;

  executor.execute(action, aliveObservation());
  executor.cancel();

  expect(context.cancelReloadCalls == 1, "cancel releases direct reload");
}

AI_TEST(testBotActionExecutorDirectlyExecutesEscapeFromBomb) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto observation = aliveObservation();
  observation.bot.objectiveFlags |= ai::ObjectiveFlag::BombPlanted;

  auto action = ai::Action {};
  action.type = ai::ActionType::EscapeFromBomb;

  const auto result = executor.execute(action, observation);

  expect(result.type == ai::ActionResultType::Accepted, "planted bomb escape is accepted");
  expect(context.escapeFromBombCalls == 1, "escape from bomb is delegated");
  expect(!executor.suppressesLegacyTaskExecution(), "escape keeps legacy task execution enabled");
}

AI_TEST(testBotActionExecutorKeepsEscapeFromBombActiveWhenReached) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto observation = aliveObservation();
  observation.bot.objectiveFlags |= ai::ObjectiveFlag::BombPlanted;

  auto action = ai::Action {};
  action.type = ai::ActionType::EscapeFromBomb;

  auto result = executor.execute(action, observation);
  expect(result.type == ai::ActionResultType::Accepted, "escape starts before reaching safety");
  expect(context.escapeFromBombCalls == 1, "escape is delegated before reaching safety");

  context.escapeFromBombReached = true;
  result = executor.execute(action, observation);

  expect(result.type == ai::ActionResultType::Accepted, "reached safety keeps escape active while bomb remains planted");
  expect(context.escapeFromBombCalls == 2, "escape context maintains the safe state after arrival");
  expect(context.cancelEscapeFromBombCalls == 0, "reaching safety does not terminate escape lifecycle");
}

AI_TEST(testBotActionExecutorCompletesEscapeFromBombWhenBombEnds) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto observation = aliveObservation();
  observation.bot.objectiveFlags |= ai::ObjectiveFlag::BombPlanted;

  auto action = ai::Action {};
  action.type = ai::ActionType::EscapeFromBomb;

  auto result = executor.execute(action, observation);
  expect(result.type == ai::ActionResultType::Accepted, "escape starts while bomb is planted");

  observation.bot.objectiveFlags &= ~ai::ObjectiveFlag::BombPlanted;
  result = executor.execute(action, observation);

  expect(result.type == ai::ActionResultType::Completed, "bomb ending completes escape");
  expect(context.cancelEscapeFromBombCalls == 1, "bomb ending releases direct escape");
}

AI_TEST(testBotActionExecutorRejectsEscapeFromBombWithoutBomb) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::EscapeFromBomb;

  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Rejected, "escape without planted bomb is rejected");
  expect(context.escapeFromBombCalls == 0, "escape is not delegated without a bomb");
}

AI_TEST(testBotActionExecutorCancelsDirectEscapeFromBomb) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto observation = aliveObservation();
  observation.bot.objectiveFlags |= ai::ObjectiveFlag::BombPlanted;

  auto action = ai::Action {};
  action.type = ai::ActionType::EscapeFromBomb;

  executor.execute(action, observation);
  executor.cancel();

  expect(context.cancelEscapeFromBombCalls == 1, "cancel releases direct escape");
}

AI_TEST(testBotActionExecutorDirectlyExecutesRescueHostage) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  auto observation = aliveObservation();
  observation.bot.hasHostage = true;
  ai::Action action {};
  action.type = ai::ActionType::RescueHostage;

  const auto result = executor.execute(action, observation);

  expect(result.type == ai::ActionResultType::Accepted, "rescue action is accepted");
  expect(context.rescueHostageCalls == 1, "rescue action invokes context");
}

AI_TEST(testBotActionExecutorCompletesRescueHostageWhenHostageIsRescued) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  auto observation = aliveObservation();
  observation.bot.hasHostage = true;
  ai::Action action {};
  action.type = ai::ActionType::RescueHostage;

  expect(executor.execute(action, observation).type == ai::ActionResultType::Accepted, "rescue action starts");
  observation.bot.hasHostage = false;
  expect(executor.execute(action, observation).type == ai::ActionResultType::Completed, "rescue action completes after hostage state clears");
  expect(context.cancelRescueHostageCalls == 1, "rescue cancellation is delegated");
}

AI_TEST(testBotActionExecutorRejectsUnavailableRescue) {
  MockActionExecutionContext context {};
  context.rescueHostageAvailable = false;
  ai::BotActionExecutor executor(context);
  auto observation = aliveObservation();
  observation.bot.hasHostage = true;
  ai::Action action {};
  action.type = ai::ActionType::RescueHostage;

  expect(executor.execute(action, observation).type == ai::ActionResultType::Rejected, "unavailable rescue is rejected");
}

AI_TEST(testBotActionExecutorDirectlyExecutesPlantBomb) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto observation = aliveObservation();
  observation.bot.objectiveFlags = ai::ObjectiveFlag::BombCarrier | ai::ObjectiveFlag::InBombZone;

  auto action = ai::Action {};
  action.type = ai::ActionType::PlantBomb;

  const auto result = executor.execute(action, observation);

  expect(result.type == ai::ActionResultType::Accepted, "plant bomb is accepted while carrying C4 in the bomb zone");
  expect(context.plantBombCalls == 1, "plant bomb is delegated");
  expect(!executor.suppressesLegacyTaskExecution(), "plant bomb keeps legacy task execution enabled");
}

AI_TEST(testBotActionExecutorCompletesPlantBombWhenBombIsPlanted) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto observation = aliveObservation();
  observation.bot.objectiveFlags = ai::ObjectiveFlag::BombCarrier | ai::ObjectiveFlag::InBombZone;

  auto action = ai::Action {};
  action.type = ai::ActionType::PlantBomb;

  auto result = executor.execute(action, observation);
  expect(result.type == ai::ActionResultType::Accepted, "plant bomb starts before completion");

  observation.bot.objectiveFlags = ai::ObjectiveFlag::BombPlanted;
  context.plantBombAvailable = false;
  result = executor.execute(action, observation);

  expect(result.type == ai::ActionResultType::Completed, "planted bomb completes the action");
  expect(context.plantBombCalls == 1, "observed completion does not attempt to plant again");
  expect(context.cancelPlantBombCalls == 1, "completion releases direct plant bomb");
}

AI_TEST(testBotActionExecutorInterruptsPlantBombForVisibleEnemy) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto observation = aliveObservation();
  observation.bot.objectiveFlags = ai::ObjectiveFlag::BombCarrier | ai::ObjectiveFlag::InBombZone;

  auto plant = ai::Action {};
  plant.type = ai::ActionType::PlantBomb;

  auto result = executor.execute(plant, observation);
  expect(result.type == ai::ActionResultType::Accepted, "plant starts before an enemy is visible");
  expect(context.plantBombCalls == 1, "initial plant is delegated once");

  observation = attackObservation(9);
  observation.bot.objectiveFlags = ai::ObjectiveFlag::BombCarrier | ai::ObjectiveFlag::InBombZone;
  result = executor.execute(plant, observation);

  expect(result.type == ai::ActionResultType::Interrupted, "visible enemy interrupts the active plant");
  expect(context.plantBombCalls == 1, "visible-enemy preemption does not restart the legacy plant task");
  expect(context.cancelPlantBombCalls == 1, "visible-enemy preemption releases plant ownership");

  ai::Action attack {};
  attack.type = ai::ActionType::AttackTarget;
  attack.targetType = ai::TargetType::Player;
  attack.targetPlayer = 9;

  result = executor.execute(attack, observation);
  expect(result.type == ai::ActionResultType::Accepted, "combat can re-enter after plant interruption");
  expect(context.attackTargetCalls == 1, "visible enemy is delegated to combat after plant interruption");
}

AI_TEST(testBotActionExecutorRejectsPlantBombForVisibleEnemy) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto observation = attackObservation(9);
  observation.bot.objectiveFlags = ai::ObjectiveFlag::BombCarrier | ai::ObjectiveFlag::InBombZone;

  ai::Action action {};
  action.type = ai::ActionType::PlantBomb;

  const auto result = executor.execute(action, observation);

  expect(result.type == ai::ActionResultType::Rejected, "new plant is rejected while a visible enemy is present");
  expect(context.plantBombCalls == 0, "rejected visible-enemy plant never restarts the legacy plant task");
  expect(context.cancelPlantBombCalls == 0, "rejected visible-enemy plant never acquires ownership");
}

AI_TEST(testBotActionExecutorInterruptsPlantBombWhenC4IsLost) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto observation = aliveObservation();
  observation.bot.objectiveFlags = ai::ObjectiveFlag::BombCarrier | ai::ObjectiveFlag::InBombZone;

  auto action = ai::Action {};
  action.type = ai::ActionType::PlantBomb;

  auto result = executor.execute(action, observation);
  expect(result.type == ai::ActionResultType::Accepted, "plant bomb starts with carried C4");

  observation.bot.objectiveFlags = ai::ObjectiveFlag::InBombZone;
  result = executor.execute(action, observation);

  expect(result.type == ai::ActionResultType::Interrupted, "losing C4 before planting interrupts the active action");
  expect(context.cancelPlantBombCalls == 1, "C4 loss releases direct plant bomb");
}

AI_TEST(testBotActionExecutorInterruptsPlantBombWhenLeavingBombZone) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto observation = aliveObservation();
  observation.bot.objectiveFlags = ai::ObjectiveFlag::BombCarrier | ai::ObjectiveFlag::InBombZone;

  auto action = ai::Action {};
  action.type = ai::ActionType::PlantBomb;

  auto result = executor.execute(action, observation);
  expect(result.type == ai::ActionResultType::Accepted, "plant bomb starts inside the bomb zone");

  observation.bot.objectiveFlags = ai::ObjectiveFlag::BombCarrier;
  result = executor.execute(action, observation);

  expect(result.type == ai::ActionResultType::Interrupted, "leaving the bomb zone before planting interrupts the active action");
  expect(context.cancelPlantBombCalls == 1, "leaving the zone releases direct plant bomb");
}

AI_TEST(testBotActionExecutorInterruptsPreemptedPlantBomb) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  auto observation = aliveObservation();
  observation.bot.objectiveFlags = ai::ObjectiveFlag::BombCarrier | ai::ObjectiveFlag::InBombZone;
  observation.bot.currentTask = ai::TaskType::PlantBomb;
  ai::Action action {};
  action.type = ai::ActionType::PlantBomb;

  expect(executor.execute(action, observation).type == ai::ActionResultType::Accepted, "plant starts before preemption");
  observation.bot.currentTask = ai::TaskType::Attack;
  context.plantBombAvailable = false;
  expect(executor.execute(action, observation).type == ai::ActionResultType::Interrupted,
         "task preemption without a planted bomb interrupts the active plant");
  expect(context.plantBombCalls == 2, "active plant checks execution availability");
  expect(context.cancelPlantBombCalls == 1, "preemption releases plant ownership");
  expect(executor.execute(action, observation).type == ai::ActionResultType::Rejected,
         "a new unavailable plant is rejected after interrupted ownership is cleared");
  executor.cancel();
  expect(context.cancelPlantBombCalls == 1, "interrupted plant is released only once");
}

AI_TEST(testBotActionExecutorRejectsInitiallyUnavailablePlantBomb) {
  MockActionExecutionContext context {};
  context.plantBombAvailable = false;
  ai::BotActionExecutor executor(context);
  auto observation = aliveObservation();
  observation.bot.objectiveFlags = ai::ObjectiveFlag::BombCarrier | ai::ObjectiveFlag::InBombZone;
  ai::Action action {};
  action.type = ai::ActionType::PlantBomb;

  expect(executor.execute(action, observation).type == ai::ActionResultType::Rejected,
         "initial execution refusal rejects the plant request");
  expect(context.plantBombCalls == 1, "initial request checks execution availability");
  expect(context.cancelPlantBombCalls == 0, "rejected request never acquires plant ownership");
}

AI_TEST(testBotActionExecutorRejectsPlantBombWhenAlreadyPlanted) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  auto observation = aliveObservation();
  observation.bot.objectiveFlags = ai::ObjectiveFlag::BombPlanted;
  ai::Action action {};
  action.type = ai::ActionType::PlantBomb;

  expect(executor.execute(action, observation).type == ai::ActionResultType::Rejected,
         "an already planted bomb does not complete a new plant request");
  expect(context.plantBombCalls == 0, "already planted bomb is not delegated");
  expect(context.cancelPlantBombCalls == 0, "already planted bomb acquires no plant ownership");
}

AI_TEST(testTrainingCollectorRecordsPreemptedPlantWithoutSuccessReward) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  ai::ActionRuntime runtime { executor };
  ai::GoalNavigationPolicy policy {};
  runtime.setMode(ai::ControlMode::Training);
  runtime.setPolicy(&policy);
  ai::TrainingBuffer buffer {};
  ai::TrainingRecorder recorder { buffer };
  ai::ActionOutcomeRewardProvider rewards {};
  ai::TrainingCollector collector { recorder, rewards };
  auto observation = aliveObservation();
  observation.gameTime = 10.0f;
  observation.bot.currentTask = ai::TaskType::PlantBomb;
  observation.bot.objectiveFlags = ai::ObjectiveFlag::BombCarrier | ai::ObjectiveFlag::InBombZone;

  expect(collector.step(runtime, observation).type == ai::ActionResultType::Accepted, "teacher plant starts recording");
  expect(recorder.hasPendingAction() && buffer.empty(), "active plant has no terminal sample yet");
  observation.gameTime = 11.0f;
  observation.bot.currentTask = ai::TaskType::Attack;
  context.plantBombAvailable = false;
  expect(collector.step(runtime, observation).type == ai::ActionResultType::Interrupted, "preempted plant is interrupted");
  expect(!runtime.isActive() && !recorder.hasPendingAction(), "interruption finishes runtime and recording lifecycles");
  expect(buffer.size() == 1, "preempted plant records exactly one transition");
  const auto &transition = buffer.at(0);
  expect(transition.action.type == ai::ActionType::PlantBomb, "recorded action remains the original plant");
  expect(transition.result.type == ai::ActionResultType::Interrupted, "recorded plant result is interrupted");
  expect(!(transition.nextObservation.bot.objectiveFlags & ai::ObjectiveFlag::BombPlanted), "recorded bomb is not planted");
  expectNear(transition.reward, 0.0f, 0.001f, "preempted plant receives the existing neutral interruption reward");
  expectNear(transition.result.elapsedTime, 1.0f, 0.001f, "recorded plant retains its actual elapsed time");
  collector.step(runtime, observation, false);
  expect(buffer.size() == 1, "finished plant is not recorded twice");
  expect(context.cancelPlantBombCalls == 1, "recorded preemption releases plant ownership once");
}

AI_TEST(testBotActionExecutorRejectsPlantBombWithoutC4) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::PlantBomb;

  auto observation = aliveObservation();
  observation.bot.objectiveFlags = ai::ObjectiveFlag::InBombZone;

  const auto result = executor.execute(action, observation);

  expect(result.type == ai::ActionResultType::Rejected, "plant bomb without C4 is rejected");
  expect(context.plantBombCalls == 0, "plant bomb is not delegated without C4");
}

AI_TEST(testBotActionExecutorRejectsPlantBombOutsideBombZone) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::PlantBomb;

  auto observation = aliveObservation();
  observation.bot.objectiveFlags = ai::ObjectiveFlag::BombCarrier;

  const auto result = executor.execute(action, observation);

  expect(result.type == ai::ActionResultType::Rejected, "plant bomb outside the bomb zone is rejected");
  expect(context.plantBombCalls == 0, "plant bomb is not delegated outside the bomb zone");
}

AI_TEST(testBotActionExecutorCancelsDirectPlantBomb) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto observation = aliveObservation();
  observation.bot.objectiveFlags = ai::ObjectiveFlag::BombCarrier | ai::ObjectiveFlag::InBombZone;

  auto action = ai::Action {};
  action.type = ai::ActionType::PlantBomb;

  executor.execute(action, observation);
  executor.cancel();

  expect(context.cancelPlantBombCalls == 1, "cancel releases direct plant bomb");
}

AI_TEST(testBotActionExecutorDirectlyExecutesDefuseBomb) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto observation = aliveObservation();
  observation.bot.objectiveFlags = ai::ObjectiveFlag::BombPlanted;

  auto action = ai::Action {};
  action.type = ai::ActionType::DefuseBomb;

  const auto result = executor.execute(action, observation);

  expect(result.type == ai::ActionResultType::Accepted, "defuse bomb is accepted while bomb is planted");
  expect(context.defuseBombCalls == 1, "defuse bomb is delegated");
  expect(!executor.suppressesLegacyTaskExecution(), "defuse bomb keeps legacy task execution enabled");
}

AI_TEST(testBotActionExecutorInterruptsActiveDefuseForVisibleEnemyWithTimeToFight) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto observation = aliveObservation();
  observation.bot.objectiveFlags = ai::ObjectiveFlag::BombPlanted;
  observation.bot.hasDefuser = true;
  observation.bombTimeRemaining = 19.2f;

  ai::Action action {};
  action.type = ai::ActionType::DefuseBomb;

  auto result = executor.execute(action, observation);
  expect(result.type == ai::ActionResultType::Accepted, "defuse starts before the enemy is visible");
  expect(context.defuseBombCalls == 1, "initial defuse is delegated once");

  observation = attackObservation(10);
  observation.bot.objectiveFlags = ai::ObjectiveFlag::BombPlanted;
  observation.bot.hasDefuser = true;
  observation.bombTimeRemaining = 15.76f;

  result = executor.execute(action, observation);

  expect(result.type == ai::ActionResultType::Interrupted,
         "visible enemy interrupts active defuse while enough bomb time remains");
  expect(context.defuseBombCalls == 1,
         "combat preemption happens before the legacy defuse task can continue");
  expect(context.cancelDefuseBombCalls == 1,
         "combat preemption releases direct defuse ownership");

  ai::Action attack {};
  attack.type = ai::ActionType::AttackTarget;
  attack.targetType = ai::TargetType::Player;
  attack.targetPlayer = 10;

  result = executor.execute(attack, observation);
  expect(result.type == ai::ActionResultType::Accepted,
         "combat can re-enter immediately after defuse interruption");
  expect(context.attackTargetCalls == 1,
         "visible enemy is delegated to combat after defuse interruption");
}

AI_TEST(testBotActionExecutorKeepsUrgentKitDefuseUnderVisibleEnemy) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto observation = aliveObservation();
  observation.bot.objectiveFlags = ai::ObjectiveFlag::BombPlanted;
  observation.bot.hasDefuser = true;
  observation.bombTimeRemaining = 12.0f;

  ai::Action action {};
  action.type = ai::ActionType::DefuseBomb;

  auto result = executor.execute(action, observation);
  expect(result.type == ai::ActionResultType::Accepted, "kit defuse starts before urgent combat contact");

  observation = attackObservation(10);
  observation.bot.objectiveFlags = ai::ObjectiveFlag::BombPlanted;
  observation.bot.hasDefuser = true;
  observation.bombTimeRemaining = 8.5f;

  result = executor.execute(action, observation);

  expect(result.type == ai::ActionResultType::Accepted,
         "kit defuse remains active inside the nine-second safety window");
  expect(context.defuseBombCalls == 2,
         "urgent kit defuse continues through the legacy task");
  expect(context.cancelDefuseBombCalls == 0,
         "urgent kit defuse is not canceled for visible combat");
}

AI_TEST(testBotActionExecutorKeepsUrgentNoKitDefuseUnderVisibleEnemy) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto observation = aliveObservation();
  observation.bot.objectiveFlags = ai::ObjectiveFlag::BombPlanted;
  observation.bot.hasDefuser = false;
  observation.bombTimeRemaining = 18.0f;

  ai::Action action {};
  action.type = ai::ActionType::DefuseBomb;

  auto result = executor.execute(action, observation);
  expect(result.type == ai::ActionResultType::Accepted, "no-kit defuse starts before urgent combat contact");

  observation = attackObservation(10);
  observation.bot.objectiveFlags = ai::ObjectiveFlag::BombPlanted;
  observation.bot.hasDefuser = false;
  observation.bombTimeRemaining = 13.5f;

  result = executor.execute(action, observation);

  expect(result.type == ai::ActionResultType::Accepted,
         "no-kit defuse remains active inside the fourteen-second safety window");
  expect(context.defuseBombCalls == 2,
         "urgent no-kit defuse continues through the legacy task");
  expect(context.cancelDefuseBombCalls == 0,
         "urgent no-kit defuse is not canceled for visible combat");
}

AI_TEST(testBotActionExecutorCompletesDefuseBombWhenBombIsGone) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto observation = aliveObservation();
  observation.bot.objectiveFlags = ai::ObjectiveFlag::BombPlanted;

  auto action = ai::Action {};
  action.type = ai::ActionType::DefuseBomb;

  auto result = executor.execute(action, observation);
  expect(result.type == ai::ActionResultType::Accepted, "defuse bomb starts while bomb is planted");

  observation.bot.objectiveFlags = 0;
  result = executor.execute(action, observation);

  expect(result.type == ai::ActionResultType::Completed, "defused bomb completes the action");
  expect(context.cancelDefuseBombCalls == 1, "completion releases direct defuse bomb");
}

AI_TEST(testBotActionExecutorRejectsDefuseBombWithoutBomb) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::DefuseBomb;

  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Rejected, "defuse bomb without planted bomb is rejected");
  expect(context.defuseBombCalls == 0, "defuse bomb is not delegated without a bomb");
}

AI_TEST(testBotActionExecutorCancelsDirectDefuseBomb) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto observation = aliveObservation();
  observation.bot.objectiveFlags = ai::ObjectiveFlag::BombPlanted;

  auto action = ai::Action {};
  action.type = ai::ActionType::DefuseBomb;

  executor.execute(action, observation);
  executor.cancel();

  expect(context.cancelDefuseBombCalls == 1, "cancel releases direct defuse bomb");
}

AI_TEST(testBotActionExecutorDirectlyExecutesPickupItem) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  context.pickupItemAvailable = true;

  auto action = ai::Action {};
  action.type = ai::ActionType::PickupItem;

  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Accepted, "available pickup item is accepted");
  expect(context.pickupItemCalls == 1, "pickup item is delegated");
  expect(!executor.suppressesLegacyTaskExecution(), "pickup item keeps legacy task execution enabled");
}

AI_TEST(testBotActionExecutorCompletesPickupItemWhenTargetDisappears) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::PickupItem;

  context.pickupItemAvailable = true;
  auto result = executor.execute(action, aliveObservation());
  expect(result.type == ai::ActionResultType::Accepted, "pickup starts while target is available");

  context.pickupItemAvailable = false;
  result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Completed, "pickup completes when target is no longer available");
  expect(context.cancelPickupItemCalls == 1, "completion releases direct pickup");
}

AI_TEST(testBotActionExecutorRejectsPickupItemWithoutTarget) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::PickupItem;

  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Rejected, "pickup without an available target is rejected");
  expect(context.pickupItemCalls == 1, "pickup availability is checked");
}

AI_TEST(testBotActionExecutorCancelsDirectPickupItem) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::PickupItem;

  context.pickupItemAvailable = true;
  executor.execute(action, aliveObservation());
  executor.cancel();

  expect(context.cancelPickupItemCalls == 1, "cancel releases direct pickup");
}

AI_TEST(testBotActionExecutorDirectlyExecutesFireBreakable) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  context.fireBreakableAvailable = true;

  auto action = ai::Action {};
  action.type = ai::ActionType::Fire;

  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Accepted, "available breakable fire is accepted");
  expect(context.fireBreakableCalls == 1, "fire breakable is delegated");
  expect(!executor.suppressesLegacyTaskExecution(), "fire breakable keeps legacy task execution enabled");
}

AI_TEST(testBotActionExecutorCompletesFireBreakableWhenTargetDisappears) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::Fire;

  context.fireBreakableAvailable = true;
  auto result = executor.execute(action, aliveObservation());
  expect(result.type == ai::ActionResultType::Accepted, "fire breakable starts while target is available");

  context.fireBreakableAvailable = false;
  result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Completed, "fire breakable completes when target disappears");
  expect(context.cancelFireBreakableCalls == 1, "completion releases direct fire breakable");
}

AI_TEST(testBotActionExecutorRejectsFireWithoutBreakable) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::Fire;

  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Rejected, "fire without breakable target is rejected");
  expect(context.fireBreakableCalls == 1, "breakable availability is checked");
}

AI_TEST(testBotActionExecutorCancelsDirectFireBreakable) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::Fire;

  context.fireBreakableAvailable = true;
  executor.execute(action, aliveObservation());
  executor.cancel();

  expect(context.cancelFireBreakableCalls == 1, "cancel releases direct fire breakable");
}

AI_TEST(testBotActionExecutorDirectlyExecutesCamp) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  context.campAvailable = true;

  auto action = ai::Action {};
  action.type = ai::ActionType::Camp;

  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Accepted, "camp is accepted when the runtime can start it");
  expect(context.campCalls == 1, "camp is delegated");
  expect(!executor.suppressesLegacyTaskExecution(), "camp keeps legacy task execution enabled");
}

AI_TEST(testBotActionExecutorCompletesCampWhenCampStops) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::Camp;

  context.campAvailable = true;
  auto result = executor.execute(action, aliveObservation());
  expect(result.type == ai::ActionResultType::Accepted, "camp starts while available");

  context.campAvailable = false;
  result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Completed, "camp completes when the runtime stops it");
  expect(context.cancelCampCalls == 1, "completion releases direct camp");
}

AI_TEST(testBotActionExecutorRejectsCampWhenRuntimeCannotStartIt) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::Camp;

  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Rejected, "camp is rejected when runtime cannot start it");
}

AI_TEST(testBotActionExecutorCancelsDirectCamp) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::Camp;

  context.campAvailable = true;
  executor.execute(action, aliveObservation());
  executor.cancel();

  expect(context.cancelCampCalls == 1, "cancel releases direct camp");
}

AI_TEST(testBotActionExecutorDirectlyExecutesWait) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  context.waitAvailable = true;

  auto action = ai::Action {};
  action.type = ai::ActionType::Wait;

  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Accepted, "wait is accepted when the runtime can start it");
  expect(context.waitCalls == 1, "wait is delegated");
  expect(!executor.suppressesLegacyTaskExecution(), "wait keeps legacy task execution enabled");
}

AI_TEST(testBotActionExecutorCompletesWaitWhenPauseStops) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::Wait;

  context.waitAvailable = true;
  auto result = executor.execute(action, aliveObservation());
  expect(result.type == ai::ActionResultType::Accepted, "wait starts while available");

  context.waitAvailable = false;
  result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Completed, "wait completes when the runtime stops it");
  expect(context.cancelWaitCalls == 1, "completion releases direct wait");
}

AI_TEST(testBotActionExecutorRejectsWaitWhenRuntimeCannotStartIt) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::Wait;

  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Rejected, "wait is rejected when runtime cannot start it");
}

AI_TEST(testBotActionExecutorCancelsDirectWait) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::Wait;

  context.waitAvailable = true;
  executor.execute(action, aliveObservation());
  executor.cancel();

  expect(context.cancelWaitCalls == 1, "cancel releases direct wait");
}

AI_TEST(testBotActionExecutorDirectlyExecutesHoldPosition) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  context.holdPositionAvailable = true;
  auto action = ai::Action {};
  action.type = ai::ActionType::HoldPosition;

  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Accepted, "hold position is accepted when the runtime can start it");
  expect(context.holdPositionCalls == 1, "hold position is delegated");
  expect(!executor.suppressesLegacyTaskExecution(), "hold position keeps legacy task execution enabled");
}

AI_TEST(testBotActionExecutorCompletesHoldPositionWhenRuntimeStopsIt) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  context.holdPositionAvailable = true;
  auto action = ai::Action {};
  action.type = ai::ActionType::HoldPosition;

  auto result = executor.execute(action, aliveObservation());
  expect(result.type == ai::ActionResultType::Accepted, "hold position starts while available");

  context.holdPositionAvailable = false;
  result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Completed, "hold position completes when the runtime stops it");
  expect(context.cancelHoldPositionCalls == 1, "completion releases direct hold position");
}

AI_TEST(testBotActionExecutorRejectsHoldPositionWhenRuntimeCannotStartIt) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto action = ai::Action {};
  action.type = ai::ActionType::HoldPosition;

  const auto result = executor.execute(action, aliveObservation());

  expect(result.type == ai::ActionResultType::Rejected, "hold position is rejected when the runtime cannot start it");
}

AI_TEST(testBotActionExecutorCancelsDirectHoldPosition) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  context.holdPositionAvailable = true;
  auto action = ai::Action {};
  action.type = ai::ActionType::HoldPosition;

  executor.execute(action, aliveObservation());
  executor.cancel();

  expect(context.cancelHoldPositionCalls == 1, "cancel releases direct hold position");
}

AI_TEST(testBotActionExecutorDirectlyExecutesHide) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  context.hideAvailable = true;
  auto observation = aliveObservation();
  observation.bot.currentTask = ai::TaskType::Normal;

  auto action = ai::Action {};
  action.type = ai::ActionType::Hide;

  const auto result = executor.execute(action, observation);

  expect(result.type == ai::ActionResultType::Accepted, "hide is accepted when the runtime can start it");
  expect(context.hideCalls == 1, "hide is delegated");
  expect(!executor.suppressesLegacyTaskExecution(), "hide keeps legacy task execution enabled");
}

AI_TEST(testBotActionExecutorCompletesHideWhenRuntimeStopsIt) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  context.hideAvailable = true;
  auto observation = aliveObservation();
  observation.bot.currentTask = ai::TaskType::Normal;

  auto action = ai::Action {};
  action.type = ai::ActionType::Hide;

  auto result = executor.execute(action, observation);
  expect(result.type == ai::ActionResultType::Accepted, "hide starts while available");

  context.hideAvailable = false;
  result = executor.execute(action, observation);

  expect(result.type == ai::ActionResultType::Completed, "hide completes when the runtime stops it");
  expect(context.cancelHideCalls == 1, "completion releases direct hide");
}

AI_TEST(testBotActionExecutorRejectsHideWhenRuntimeCannotStartIt) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto observation = aliveObservation();
  observation.bot.currentTask = ai::TaskType::Normal;

  auto action = ai::Action {};
  action.type = ai::ActionType::Hide;

  const auto result = executor.execute(action, observation);

  expect(result.type == ai::ActionResultType::Rejected, "hide is rejected when the runtime cannot start it");
}

AI_TEST(testBotActionExecutorCancelsDirectHide) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  context.hideAvailable = true;
  auto observation = aliveObservation();
  observation.bot.currentTask = ai::TaskType::Normal;

  auto action = ai::Action {};
  action.type = ai::ActionType::Hide;

  executor.execute(action, observation);
  executor.cancel();

  expect(context.cancelHideCalls == 1, "cancel releases direct hide");
}


AI_TEST(testBotActionExecutorRejectsUnsupportedAction) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor { context };
  auto observation = aliveObservation();

  ai::Action action {};
  action.type = ai::ActionType::None;

  expect(executor.execute(action, observation).type == ai::ActionResultType::Rejected,
      "none action is rejected as non-executable");
}

