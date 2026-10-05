//
// AiPB - AI action executor unit tests.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <cmath>
#include <limits>

#include <ai/ai_action_execution_context.h>
#include <ai/ai_bot_action_executor.h>

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
  int escapeFromBombCalls {};
  int cancelEscapeFromBombCalls {};
  int rescueHostageCalls {};
  int cancelRescueHostageCalls {};
  int lastNode { -1 };
  int lastAttackTarget { -1 };
  int lastAimTarget { -1 };
  int lastHuntTarget { -1 };
  bool followPlayerAvailable { true };
  bool huntTargetReached {};
  bool seekCoverReached {};
  bool escapeFromBombReached {};
  bool rescueHostageAvailable { true };
  int plantBombCalls {};
  int cancelPlantBombCalls {};
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

  bool huntTarget(int targetPlayer) override {
    ++huntTargetCalls;
    lastHuntTarget = targetPlayer;
    return true;
  }

  bool isHuntTargetReached(int targetPlayer) const override {
    return huntTargetReached && targetPlayer == lastHuntTarget;
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
    return true;
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
  observation.combat.lastEnemyEntity = targetPlayer;
  observation.playerCount = 1;
  observation.players[0].entityIndex = targetPlayer;
  observation.players[0].valid = true;
  observation.players[0].alive = true;
  observation.players[0].enemy = true;
  return observation;
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

AI_TEST(testBotActionExecutorCompletesRetreatWhenRuntimeStopsIt) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);
  auto action = ai::Action {};
  action.type = ai::ActionType::Retreat;
  auto result = executor.execute(action, aliveObservation());
  expect(result.type == ai::ActionResultType::Accepted, "retreat starts while available");
  context.retreatAvailable = false;
  result = executor.execute(action, aliveObservation());
  expect(result.type == ai::ActionResultType::Completed, "retreat completes when the runtime stops it");
  expect(context.cancelRetreatCalls == 1, "runtime stop releases direct retreat");
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
  expect(context.cancelHuntTargetCalls == 1, "completion releases the active hunt target");
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

AI_TEST(testBotActionExecutorCompletesEscapeFromBombWhenReached) {
  MockActionExecutionContext context {};
  ai::BotActionExecutor executor(context);

  auto observation = aliveObservation();
  observation.bot.objectiveFlags |= ai::ObjectiveFlag::BombPlanted;

  auto action = ai::Action {};
  action.type = ai::ActionType::EscapeFromBomb;

  auto result = executor.execute(action, observation);
  expect(result.type == ai::ActionResultType::Accepted, "escape starts before reaching safety");
  expect(context.escapeFromBombCalls == 1, "escape is delegated before completion");

  context.escapeFromBombReached = true;
  result = executor.execute(action, observation);

  expect(result.type == ai::ActionResultType::Completed, "reached safety completes escape");
  expect(context.cancelEscapeFromBombCalls == 1, "completion releases direct escape");
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

  observation.bot.objectiveFlags |= ai::ObjectiveFlag::BombPlanted;
  result = executor.execute(action, observation);

  expect(result.type == ai::ActionResultType::Completed, "planted bomb completes the action");
  expect(context.cancelPlantBombCalls == 1, "completion releases direct plant bomb");
}

AI_TEST(testBotActionExecutorCompletesPlantBombWhenC4IsLost) {
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

  expect(result.type == ai::ActionResultType::Completed, "losing C4 completes the active action");
  expect(context.cancelPlantBombCalls == 1, "C4 loss releases direct plant bomb");
}

AI_TEST(testBotActionExecutorCompletesPlantBombWhenLeavingBombZone) {
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

  expect(result.type == ai::ActionResultType::Completed, "leaving the bomb zone completes the active action");
  expect(context.cancelPlantBombCalls == 1, "leaving the zone releases direct plant bomb");
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
  action.type = ai::ActionType::Reload;

  expect(executor.execute(action, observation).type == ai::ActionResultType::Rejected,
      "unsupported reload action is rejected");
}

