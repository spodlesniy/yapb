//
// AiPB - AI action to observed-task mapping unit tests.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <ai/ai_action_task_mapping.h>

using ai::test::expect;

AI_TEST(testObservedTaskActionMapping) {
  ai::Observation observation {};
  ai::Action action {};

  observation.bot.currentTask = ai::TaskType::Pause;
  action.type = ai::ActionType::Wait;
  expect(ai::actionMatchesObservedTask(action, observation), "wait maps to pause task");

  action.type = ai::ActionType::HoldPosition;
  expect(ai::actionMatchesObservedTask(action, observation), "hold position maps to pause task");

  observation.bot.currentTask = ai::TaskType::Hide;
  expect(ai::actionMatchesObservedTask(action, observation), "hold position maps to hide task");

  observation.bot.currentTask = ai::TaskType::Camp;
  action.type = ai::ActionType::Camp;
  expect(ai::actionMatchesObservedTask(action, observation), "camp action maps to camp task");

  observation.bot.currentTask = ai::TaskType::SeekCover;
  action.type = ai::ActionType::SeekCover;
  expect(ai::actionMatchesObservedTask(action, observation), "seek cover action maps to cover task");

  observation.bot.currentTask = ai::TaskType::PlantBomb;
  action.type = ai::ActionType::PlantBomb;
  expect(ai::actionMatchesObservedTask(action, observation), "plant action maps to plant task");

  observation.bot.currentTask = ai::TaskType::DefuseBomb;
  action.type = ai::ActionType::DefuseBomb;
  expect(ai::actionMatchesObservedTask(action, observation), "defuse action maps to defuse task");

  observation.bot.currentTask = ai::TaskType::PickupItem;
  action.type = ai::ActionType::PickupItem;
  expect(ai::actionMatchesObservedTask(action, observation), "pickup action maps to pickup task");

  observation.bot.currentTask = ai::TaskType::EscapeFromBomb;
  action.type = ai::ActionType::EscapeFromBomb;
  expect(ai::actionMatchesObservedTask(action, observation), "escape action maps to escape task");

  observation.bot.currentTask = ai::TaskType::ShootBreakable;
  action.type = ai::ActionType::Fire;
  expect(ai::actionMatchesObservedTask(action, observation), "fire action maps to breakable task");
}

AI_TEST(testObservedCombatActionMappingRequiresCurrentEnemy) {
  ai::Observation observation {};
  observation.bot.currentTask = ai::TaskType::Attack;
  observation.combat.enemyEntity = 7;

  ai::Action action {};
  action.type = ai::ActionType::AttackTarget;
  action.targetPlayer = 7;

  expect(ai::actionMatchesObservedTask(action, observation), "attack uses current observed enemy");

  action.targetPlayer = 8;
  expect(!ai::actionMatchesObservedTask(action, observation), "attack cannot claim another target");

  observation.bot.currentTask = ai::TaskType::Hunt;
  action.type = ai::ActionType::HuntTarget;
  action.targetPlayer = 7;
  expect(ai::actionMatchesObservedTask(action, observation), "hunt uses current observed enemy");
}

AI_TEST(testObservedTaskActionMappingRejectsUnrelatedActions) {
  ai::Observation observation {};
  observation.bot.currentTask = ai::TaskType::Attack;

  ai::Action action {};
  action.type = ai::ActionType::PlantBomb;

  expect(!ai::actionMatchesObservedTask(action, observation), "unrelated action is rejected");
}

AI_TEST(testObservedTaskActionIdentity) {
  ai::Action attackOne {};
  attackOne.type = ai::ActionType::AttackTarget;
  attackOne.targetPlayer = 7;

  ai::Action attackSame {};
  attackSame.type = ai::ActionType::AttackTarget;
  attackSame.targetPlayer = 7;

  ai::Action attackOther {};
  attackOther.type = ai::ActionType::AttackTarget;
  attackOther.targetPlayer = 8;

  expect(ai::sameObservedTaskAction(attackOne, attackSame), "same combat target identifies the same task action");
  expect(!ai::sameObservedTaskAction(attackOne, attackOther), "different combat target identifies a different task action");

  ai::Action campOne {};
  campOne.type = ai::ActionType::Camp;
  campOne.duration = 2.0f;

  ai::Action campSame {};
  campSame.type = ai::ActionType::Camp;
  campSame.duration = 5.0f;

  expect(ai::sameObservedTaskAction(campOne, campSame), "task action identity ignores runtime timing parameters");
}
