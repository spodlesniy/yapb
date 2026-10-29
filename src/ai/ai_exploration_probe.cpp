#include <yapb.h>
#include <ai/ai_exploration_probe.h>
#include <ai/ai_menu.h>
#include <aipb/explorer/engine_bridge_scenario_pose.h>
#include <aipb/explorer/engine_bridge_math.h>
#include <aipb/explorer/engine_bridge_scenario_equipment_init.h>
#include <aipb/explorer/engine_bridge_scenario_loader.h>
#include <aipb/explorer/engine_bridge_scenario_run_entry.h>
#include <aipb/explorer/engine_bridge_scenario_run_gate.h>
#include <aipb/explorer/engine_bridge_scenario_action_scheduler.h>
#include <aipb/explorer/engine_bridge_scenario_movement.h>
#include <aipb/explorer/engine_bridge_scenario_lifecycle.h>
#include <aipb/explorer/engine_bridge_scenario_result.h>
#include <aipb/explorer/engine_bridge_scenario_trace.h>
#include <aipb/explorer/engine_bridge_queue.h>
#include <aipb/explorer/engine_bridge_trace.h>
#include <aipb/explorer/engine_bridge_trace_classification.h>
#include <aipb/explorer/engine_bridge_trace_io.h>
#include <aipb/explorer/engine_bridge_trace_observations.h>
#include <aipb/explorer/engine_bridge_trace_report.h>
#include <crlib/files.h>

#include <cstdio>
#include <cstring>

#include <cmath>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <utility>

namespace ai::explore {
namespace {

struct Session {
   AipbExplorerBridgeCommand command {};
   int botIndex { -1 };
   aipb::explorer::bridge::ProbeBudget budget {};
   float lastTime {};
   bool replay {};
   int replayIndex {};
   bool waitingForKnife {};
   float knifeDeadline {};
};

using aipb::explorer::bridge::TraceBuffer;
using aipb::explorer::bridge::TraceSample;
using aipb::explorer::bridge::compareStates;
static TraceBuffer trace {};
static aipb::explorer::bridge::ProbeQueue queue {};
static char queueSavePath[512] {};
static aipb::explorer::bridge::TraceMetadata metadata {};

Session session {};
aipb::explorer::scenario::ActionScheduler scenarioScheduler {};
char scenarioName[64] {};
char scenarioMap[64] {};
float scenarioLastTime {};
float scenarioActionYaw {};
float scenarioActionPitch {};
int scenarioActionIndex { -1 };
uint32_t scenarioRoundId {};
int scenarioTeam {};
Bot *scenarioBot {};
aipb::explorer::scenario::ScenarioResult scenarioResult {};
aipb::explorer::scenario::ScenarioResultPose scenarioLastCommitted {};
bool scenarioResultActive {};
bool scenarioHaveLastCommitted {};
aipb::explorer::scenario::ScenarioTrace scenarioFrameTrace {};

aipb::explorer::scenario::ScenarioResultPose scenarioPose(const Bot &bot) {
   aipb::explorer::scenario::ScenarioResultPose pose {};
   pose.position[0] = bot.pev->origin.x;
   pose.position[1] = bot.pev->origin.y;
   pose.position[2] = bot.pev->origin.z;
   pose.velocity[0] = bot.pev->velocity.x;
   pose.velocity[1] = bot.pev->velocity.y;
   pose.velocity[2] = bot.pev->velocity.z;
   pose.yaw = bot.pev->v_angle.y;
   pose.pitch = bot.pev->v_angle.x;
   pose.onGround = !!(bot.pev->flags & FL_ONGROUND);
   pose.crouching = !!(bot.pev->flags & FL_DUCKING);
   return pose;
}

bool saveScenarioResult(const aipb::explorer::scenario::ScenarioResult &result) {
   namespace fs = std::filesystem;
   const auto explorer = strings.joinPath(bstor.getRunningPath(), folders.data, "explorer");
   const fs::path directory = fs::path(explorer.chars())
      / "scenarios" / "results" / result.name;
   const std::time_t epoch = std::time(nullptr);
   std::tm calendar {};
#if defined(_MSC_VER)
   if (localtime_s(&calendar, &epoch) != 0) {
      return false;
   }
#else
   if (!localtime_r(&epoch, &calendar)) {
      return false;
   }
#endif
   char prefix[32] {};
   if (std::strftime(prefix, sizeof(prefix), "%Y%m%d_%H%M%S", &calendar) == 0) {
      return false;
   }
   std::string saved {};
   return aipb::explorer::scenario::saveScenarioRunBundle(
      directory, prefix, result, scenarioFrameTrace, saved);
}

int64_t scenarioTimeMs() {
   const float now = game.time();
   return std::isfinite(now) && now >= 0.0f
      ? static_cast<int64_t>(static_cast<double>(now) * 1000.0 + 0.5) : -1;
}

void scenarioEnd(Bot *bot, const char *reason) {
   if (scenarioResultActive) {
      scenarioResultActive = false;
      scenarioResult.status = scenarioScheduler.status();
      scenarioResult.reason = reason;
      scenarioResult.frames = scenarioScheduler.frames();
      scenarioResult.commandMs = scenarioScheduler.totalMilliseconds();
      scenarioResult.endGameMs = scenarioTimeMs();
      if (bot && bot->pev) {
         scenarioResult.end = scenarioPose(*bot);
         scenarioResult.endSource = aipb::explorer::scenario::ScenarioEndSource::Live;
      }
      else if (scenarioHaveLastCommitted) {
         scenarioResult.end = scenarioLastCommitted;
         scenarioResult.endSource = aipb::explorer::scenario::ScenarioEndSource::LastCommitted;
      }
      else {
         scenarioResult.endSource = aipb::explorer::scenario::ScenarioEndSource::Missing;
      }
      if (!saveScenarioResult(scenarioResult)) {
         game.print("AiPB scenario: '%s' result could not be saved.", scenarioName);
      }
   }
   if (bot && bot->pev) {
      bot->pev->button = 0;
      bot->pev->oldbuttons = 0;
      bot->pev->impulse = 0;
   }
   game.print("AiPB scenario: '%s' %s, status=%s, frames=%d, command_ms=%d.",
      scenarioName, reason,
      aipb::explorer::scenario::actionScheduleStatusName(scenarioScheduler.status()),
      scenarioScheduler.frames(), scenarioScheduler.totalMilliseconds());
}


// This server-console-only command is registered from the existing AiPB bot
// frame hook, so legacy YaPB control and message handlers are not modified.
struct ScenarioRuntimeObservation {
   aipb::explorer::scenario::ScenarioStartObservation actual {};
   int primary {};
   int secondary {};
   int selected {};
};

// Read only live engine state. The same source is used by both equipment
// preparation and execution admission to prevent conflicting observations.
ScenarioRuntimeObservation readScenarioRuntimeObservation(
   Bot *bot, const aipb::explorer::scenario::Scenario &scenario
) {
   const auto &e = scenario.equipment;
   const auto weaponId = [](const std::string &alias) {
      return alias == "none" ? 0 : util.weaponAliasToId(alias.c_str());
   };

   const int primary = weaponId(e.primary.type);
   const int secondary = weaponId(e.secondary.type);
   const int selected = e.selected == "knife" ? Weapon::Knife
      : e.selected == "primary" ? primary
      : e.selected == "secondary" ? secondary
      : e.selected == "he" ? Weapon::Explosive
      : e.selected == "flash" ? Weapon::Flashbang
      : e.selected == "smoke" ? Weapon::Smoke : -1;

   aipb::explorer::scenario::ScenarioStartObservation actual {};
   actual.velocity[0] = bot->pev->velocity.x;
   actual.velocity[1] = bot->pev->velocity.y;
   actual.velocity[2] = bot->pev->velocity.z;
   actual.position[0] = bot->pev->origin.x;
   actual.position[1] = bot->pev->origin.y;
   actual.position[2] = bot->pev->origin.z;
   actual.yaw = bot->pev->v_angle.y;
   actual.pitch = bot->pev->v_angle.x;
   actual.onGround = !!(bot->pev->flags & FL_ONGROUND);
   actual.crouching = !!(bot->pev->flags & FL_DUCKING);
   actual.health = bot->pev->health;
   actual.armor = bot->pev->armorvalue;
   actual.maxSpeed = bot->pev->maxspeed;
   actual.money = bot->m_moneyAmount;
   actual.defuseKit = bot->m_hasDefuser;
   actual.selectedId = bot->m_currentWeapon;
   const auto weapons = static_cast<std::uint32_t>(bot->pev->weapons);
   actual.primaryId = aipb::explorer::scenario::observedScenarioWeaponId(
      weapons, static_cast<std::uint32_t>(kPrimaryWeaponMask));
   actual.secondaryId = aipb::explorer::scenario::observedScenarioWeaponId(
      weapons, static_cast<std::uint32_t>(kSecondaryWeaponMask));

   if (primary > 0 && actual.primaryId == primary) {
      actual.primaryClip = bot->m_ammoInClip[primary];
      actual.primaryReserve = bot->getAmmo(primary);
   }
   if (secondary > 0 && actual.secondaryId == secondary) {
      actual.secondaryClip = bot->m_ammoInClip[secondary];
      actual.secondaryReserve = bot->getAmmo(secondary);
   }

   actual.he = aipb::explorer::scenario::observedScenarioGrenadeCount(
      !!(weapons & cr::bit(Weapon::Explosive)), 0, 1);
   actual.flash = aipb::explorer::scenario::observedScenarioGrenadeCount(
      !!(weapons & cr::bit(Weapon::Flashbang)),
      bot->getAmmo(Weapon::Flashbang), 2);
   actual.smoke = aipb::explorer::scenario::observedScenarioGrenadeCount(
      !!(weapons & cr::bit(Weapon::Smoke)), 0, 1);
   // No trustworthy helmet observation is currently supplied by YaPB.

   return { actual, primary, secondary, selected };
}

bool runScenarioInternal(const char *name) {
   if (!name || !aipb::explorer::scenario::safeIdentifier(name)) {
      game.print("AiPB scenario: invalid name.");
      return false;
   }
   const auto directory = strings.joinPath(bstor.getRunningPath(), folders.data, "explorer");
   aipb::explorer::scenario::Scenario scenario {};
   std::string detail {};
   const auto loaded = aipb::explorer::scenario::loadScenario(directory.chars(), name, scenario, detail);
   if (loaded != aipb::explorer::scenario::LoadStatus::Ok) {
      game.print("AiPB scenario: '%s' load failed (%s): %s.", name,
         aipb::explorer::scenario::loadStatusName(loaded), detail.c_str());
      return false;
   }
   auto *bot = bots.findBotByIndex(scenario.botIndex);
   const int team = bot == nullptr ? 0 : bot->m_team == Team::CT ? 2 :
      bot->m_team == Team::Terrorist ? 1 : 0;
   const auto entry = aipb::explorer::scenario::checkScenarioRunEntry(scenario,
      game.getMapName(), bot != nullptr, bot && game.isAliveEntity(bot->ent()), team,
      active() || queue.running || scenarioScheduler.running());
   if (entry != aipb::explorer::scenario::RunEntryStatus::Validated || gameState.isRoundOver()) {
      game.print("AiPB scenario: '%s' rejected (%s).", name,
         gameState.isRoundOver() ? "RoundOver" :
         aipb::explorer::scenario::runEntryStatusName(entry));
      return false;
   }
   // Reject unsupported actions before changing any engine-owned state.
   for (const auto &action : scenario.actions) {
      if (action.attack || action.attack2 || action.use || action.select != "none") {
         game.print("AiPB scenario: '%s' rejected (UnsupportedAction).", name);
         return false;
      }
   }
   auto observation = readScenarioRuntimeObservation(bot, scenario);
   aipb::explorer::scenario::PoseInitPlan pose {};
   const auto poseStatus = aipb::explorer::scenario::planScenarioPose(
      scenario, observation.actual, false, pose);
   if (poseStatus != aipb::explorer::scenario::PoseInitStatus::Ready) {
      game.print("AiPB scenario: '%s' pose rejected (%s).", name,
         aipb::explorer::scenario::poseInitStatusName(poseStatus));
      return false;
   }
   const Vector target(pose.position[0], pose.position[1], pose.position[2]);
   const auto hull = pose.crouching ? head_hull : human_hull;
   TraceResult occupancy {}, support {};
   game.testHull(target, target, TraceIgnore::None, hull, bot->ent(), &occupancy);
   game.testHull(target, target - Vector(0.0f, 0.0f, 4.0f),
      TraceIgnore::None, hull, bot->ent(), &support);
   if (occupancy.fStartSolid || occupancy.fAllSolid
      || support.fStartSolid || support.fAllSolid
      || support.flFraction >= 1.0f || support.vecPlaneNormal.z < 0.7f) {
      game.print("AiPB scenario: '%s' blocked or unsupported pose.", name);
      return false;
   }
   const auto equipment = aipb::explorer::scenario::planScenarioEquipment(
      scenario, observation.actual, false, observation.primary,
      observation.secondary, observation.selected);
   if (!equipment.ready()) {
      game.print("AiPB scenario: '%s' equipment rejected (%s, field=%s).", name,
         aipb::explorer::scenario::equipmentInitStatusName(equipment.status),
         equipment.detail.c_str());
      return false;
   }
   // Plan and check every constraint before making any writes.
   const Vector oldOrigin = bot->pev->origin;
   const Vector oldVelocity = bot->pev->velocity;
   const Vector oldView = bot->pev->v_angle;
   const Vector oldAngles = bot->pev->angles;
   const float oldHealth = bot->pev->health;
   const float oldHealthValue = bot->m_healthValue;
   const float oldArmor = bot->pev->armorvalue;
   const int oldButtons = bot->pev->button;
   const int oldOldButtons = bot->pev->oldbuttons;
   const int oldImpulse = bot->pev->impulse;
   const int oldFlags = bot->pev->flags;
   const auto restore = [&] () {
      engfuncs.pfnSetOrigin(bot->ent(), oldOrigin);
      bot->pev->velocity = oldVelocity;
      bot->pev->v_angle = oldView;
      bot->pev->angles = oldAngles;
      bot->pev->health = oldHealth;
      bot->m_healthValue = oldHealthValue;
      bot->pev->armorvalue = oldArmor;
      bot->pev->button = oldButtons;
      bot->pev->oldbuttons = oldOldButtons;
      bot->pev->impulse = oldImpulse;
      bot->pev->flags = oldFlags;
   };
   engfuncs.pfnSetOrigin(bot->ent(), target);
   bot->pev->velocity = Vector(pose.velocity[0], pose.velocity[1], pose.velocity[2]);
   bot->pev->v_angle = Vector(pose.pitch, pose.yaw, 0.0f);
   bot->pev->angles = Vector(-pose.pitch / 3.0f, pose.yaw, 0.0f);
   bot->pev->button = 0;
   bot->pev->oldbuttons = 0;
   bot->pev->impulse = 0;
   bot->pev->flags |= FL_ONGROUND;
   if (equipment.plan.setHealth) {
      bot->pev->health = equipment.plan.health;
      bot->m_healthValue = equipment.plan.health;
   }
   if (equipment.plan.setArmor) {
      bot->pev->armorvalue = equipment.plan.armor;
   }
   observation = readScenarioRuntimeObservation(bot, scenario);
   const auto gate = aipb::explorer::scenario::checkScenarioRunStart(
      scenario, observation.actual, observation.primary,
      observation.secondary, observation.selected);
   const int64_t clock = scenarioTimeMs();
   if (!gate.ready() || clock < 0 || gameState.isRoundOver()
      || !scenarioScheduler.begin(scenario, gate, clock)) {
      restore();
      game.print("AiPB scenario: '%s' start preflight failed (mismatches=%d, unverified=%d).",
         name, gate.assessment.mismatches, gate.assessment.unverified);
      return false;
   }
   std::snprintf(scenarioName, sizeof(scenarioName), "%s", name);
   std::snprintf(scenarioMap, sizeof(scenarioMap), "%s", game.getMapName());
   scenarioLastTime = game.time();
   scenarioActionIndex = -1;
   scenarioRoundId = gameState.getTelemetryRoundId();
   scenarioTeam = team;
   scenarioBot = bot;
   scenarioResult = {};
   scenarioResult.name = name;
   scenarioResult.map = scenarioMap;
   scenarioResult.botIndex = scenario.botIndex;
   scenarioResult.team = team;
   scenarioResult.roundId = scenarioRoundId;
   scenarioResult.startGameMs = clock;
   scenarioResult.start = scenarioPose(*bot);
   scenarioHaveLastCommitted = false;
   scenarioFrameTrace.clear();
   scenarioResultActive = true;
   game.print("AiPB scenario: '%s' started; bot=%d actions=%d.", name,
      scenario.botIndex, static_cast<int>(scenario.actions.size()));
   return true;
}

void stopScenarioInternal() {
   if (!scenarioScheduler.running()) {
      game.print("AiPB scenario: idle; no action to stop.");
      return;
   }
   auto *bot = bots.findBotByIndex(scenarioScheduler.botIndex());
   scenarioScheduler.cancel();
   scenarioEnd(bot == scenarioBot ? bot : nullptr, "stopped");
}

void statusScenarioInternal(edict_t *client) {
   const auto message = strings.format("AiPB scenario: '%s' status=%s action=%d frames=%d command_ms=%d.",
      scenarioName,
      aipb::explorer::scenario::actionScheduleStatusName(scenarioScheduler.status()),
      static_cast<int>(scenarioScheduler.actionIndex()) + 1,
      scenarioScheduler.frames(), scenarioScheduler.totalMilliseconds());
   if (client) {
      game.clientPrint(client, "%s", message);
   }
   else {
      game.print("%s", message);
   }
}

AipbExplorerBridgeState observe(const Bot &bot) {
   const auto &v = *bot.pev;
   AipbExplorerBridgeState state {};
   state.x = v.origin.x;
   state.y = v.origin.y;
   state.z = v.origin.z;
   state.vx = v.velocity.x;
   state.vy = v.velocity.y;
   state.vz = v.velocity.z;
   state.yaw = aipb::explorer::bridge::yaw(v.v_angle.y, 0.0f);
   state.pitch = v.v_angle.x;
   state.tick = static_cast <uint64_t> (session.replay ? session.replayIndex : trace.count);
   state.on_ground = (v.flags & FL_ONGROUND) ? 1 : 0;
   state.crouching = (v.flags & FL_DUCKING) ? 1 : 0;
   return state;
}

void stop(const char *reason) {
   if (session.botIndex >= 0) {
      game.print("AiPB explorer: bot #%d, %d frames, %d ms simulated: %s", session.botIndex,
         session.budget.executed, session.budget.simulatedMs, reason);
   }
   session = {};
   session.botIndex = -1;
   if (queue.running) {
      queue.running = false;
      queueSavePath[0] = '\0';
   }
}

} // namespace

bool scenarioRunning() { return scenarioScheduler.running(); }
const char *currentScenarioName() { return scenarioName; }
bool runScenario(const char *name) { return runScenarioInternal(name); }
void stopScenario() { stopScenarioInternal(); }
void printScenarioStatus(edict_t *client) { statusScenarioInternal(client); }

bool active() {
   return session.botIndex >= 0 || scenarioScheduler.running();
}

bool begin(int botIndex, const AipbExplorerBridgeCommand &command, int limit, bool milliseconds, bool append) {
   const aipb::explorer::bridge::ProbeBudget budget { limit, milliseconds };
   if (active() || (queue.running && queueSavePath[0] != '\0') || botIndex < 0 || !aipb::explorer::bridge::valid(command) || !budget.valid()) {
      return false;
   }
   auto *bot = bots.findBotByIndex(botIndex);

   if (bot == nullptr || !game.isAliveEntity(bot->ent()) || !std::isfinite(bot->pev->maxspeed)
      || bot->pev->maxspeed < 10.0f) {
      return false;
   }

   if (append) {
      auto current = observe(*bot);
      if (!aipb::explorer::bridge::canAppendTrace(trace, botIndex, current)
         || std::strcmp(metadata.map, game.getMapName()) != 0) {
         game.print("AiPB explorer: append rejected; bot or endpoint state differs, or trace is full.");
         return false;
      }
   }
   else {
      // Preserve the visible body yaw before capturing the start of a new trace.
      const float previousViewYaw = bot->pev->v_angle.y;
      const float bodyYaw = bot->pev->angles.y;
      const float startYaw = aipb::explorer::bridge::initialViewYaw(bodyYaw, previousViewYaw);
      bot->pev->v_angle.y = startYaw;
      bot->pev->angles.y = startYaw;
      game.print("AiPB explorer: start yaw sync, body=%.1f, view=%.1f, selected=%.1f",
         bodyYaw, previousViewYaw, startYaw);
      trace.clear();
      trace.botIndex = botIndex;
      metadata = {};
      const auto map = game.getMapName();
      std::snprintf(metadata.map, sizeof(metadata.map), "%s", map);
      metadata.initial = observe(*bot);
      metadata.viewZ = bot->pev->view_ofs.z;
   }

   session = {};
   session.botIndex = botIndex;
   session.command = command;
   session.budget = budget;
   session.lastTime = game.time();
   if (append) {
      trace.completed = false;
   }
   game.print("AiPB explorer: %s bot #%d for %d %s", append ? "appending" : "started", botIndex, limit,
      milliseconds ? "simulated ms" : "frames");
   return true;
}

bool enqueue(int botIndex, const AipbExplorerBridgeCommand &command, int limit, bool milliseconds) {
   return !active() && queue.enqueue(botIndex, command, limit, milliseconds);
}

bool clearQueue() {
   if (active() || queue.running) {
      return false;
   }
   queue.clear();
   return true;
}

int queuedCommands() {
   return queue.count;
}

bool runQueue(const char *savePath) {
   if (active() || !queue.start() || !savePath || !*savePath
      || std::strlen(savePath) >= sizeof(queueSavePath)) {
      queue.running = false;
      return false;
   }
   auto *first = queue.current();
   if (!first || !begin(queue.botIndex, first->command, first->budget.requested,
      first->budget.milliseconds, false)) {
      queue.running = false;
      return false;
   }
   std::snprintf(queueSavePath, sizeof(queueSavePath), "%s", savePath);
   return true;
}

bool beginReplay(int botIndex) {
   if (active() || !trace.completed || trace.count == 0) {
      return false;
   }
   auto *bot = bots.findBotByIndex(botIndex);
   if (!bot || !game.isAliveEntity(bot->ent()) || !std::isfinite(bot->pev->maxspeed)
      || bot->pev->maxspeed < 10.0f) {
      return false;
   }
   if (std::strcmp(metadata.map, game.getMapName()) != 0) {
      return false;
   }
   const auto current = observe(*bot);
   const auto difference = compareStates(trace.samples[0].before, current);
   if (!difference.within(5.0, 30.0, 5.0f)) {
      game.print("AiPB explorer: replay start mismatch (position=%.2f, velocity=%.2f, yaw=%.2f)",
         difference.position, difference.velocity, difference.yawDegrees);
      return false;
   }
   aipb::explorer::bridge::ReplaySpeedRequirement requirement {};
   if (!aipb::explorer::bridge::replaySpeedRequirement(trace, requirement)) {
      game.print("AiPB explorer: replay rejected: invalid speed evidence.");
      return false;
   }
   game.print("AiPB explorer: replay speed preflight: required=%.1f, current_limit=%.1f, peak_observed_horizontal=%.1f, limiting_step=%d",
      requirement.maximumRequested, bot->pev->maxspeed,
      requirement.peakObservedHorizontal, requirement.requestedStep);
   const bool insufficient = bot->pev->maxspeed + 0.01f < requirement.maximumRequested;
   if (insufficient && !(bot->pev->weapons & cr::bit(Weapon::Knife))) {
      game.print("AiPB explorer: replay rejected: current weapon is too slow and knife is unavailable.");
      return false;
   }

   session = {};
   session.botIndex = botIndex;
   session.lastTime = game.time();
   if (insufficient) {
      session.waitingForKnife = true;
      session.knifeDeadline = game.time() + 1.0f;
      bot->issueCommand("weapon_knife");
      game.print("AiPB explorer: requested knife; waiting for speed limit >= %.1f before replay.",
         requirement.maximumRequested);
   }
   else {
      session.replay = true;
      game.print("AiPB explorer: replay started, %d steps", trace.count);
   }
   return true;
}

int recordedSteps() {
   return trace.count;
}

bool report() {
   if (active()) {
      return false;
   }
   aipb::explorer::bridge::TraceReport summary {};
   if (!aipb::explorer::bridge::summarizeTrace(trace, summary)) {
      return false;
   }
   game.print("AiPB explorer report: frames=%d, duration_ms=%d, path=%.2f, displacement=%.2f, avg_speed=%.2f, peak_velocity=%.2f",
      summary.frames, summary.simulatedMs, summary.traveledUnits, summary.displacementUnits,
      summary.averageSpeed, summary.peakVelocity);
   game.print("AiPB explorer report: airborne=%d, crouched=%d, jump_held=%d, neutral=%d",
      summary.airborneFrames, summary.crouchedFrames, summary.jumpHeldFrames,
      summary.neutralInputFrames);
   return true;
}

bool progress() {
   if (active()) {
      return false;
   }

   aipb::explorer::bridge::TraceProgressReport summary {};

   if (!aipb::explorer::bridge::summarizeTraceProgress(trace, summary)) {
      return false;
   }

   game.print("AiPB explorer progress: grounded_input=%d, windows=%d, low_progress=%d, first_step=%d, longest_ms=%d",
      summary.groundedInputFrames, summary.evaluatedWindows, summary.lowProgressWindows,
      summary.firstLowProgressStep, summary.longestLowProgressMs);
   game.print("AiPB explorer progress: requested_units=%.2f, observed_horizontal_units=%.2f (evaluated windows)",
      summary.requestedUnits, summary.observedUnits);
   return true;
}

bool classify() {
   if (active()) {
      return false;
   }

   aipb::explorer::bridge::TraceMovementClassification result {};

   if (!aipb::explorer::bridge::classifyTraceMovement(trace, result)) {
      return false;
   }

   game.print("AiPB explorer classify: result=%s, contact=unknown, windows=%d, progress=%d, low_travel=%d, direction_mismatch=%d",
      aipb::explorer::bridge::movementClassificationName(result.classification),
      result.evaluatedWindows, result.progressWindows, result.lowTravelWindows,
      result.directionMismatchWindows);
   game.print("AiPB explorer classify: first_blocked_step=%d, longest_low_streak=%d, requested=%.2f, observed=%.2f, projected=%.2f",
      result.firstBlockedStep, result.longestLowTravelStreak, result.requestedUnits,
      result.observedUnits, result.projectedUnits);
   game.print("AiPB explorer classify: last_window=%s, last_step=%d, state_start=%d, deflected_windows=%d, first_deflected_step=%d, longest_deflection_streak=%d, lateral=%.2f",
      aipb::explorer::bridge::movementWindowOutcomeName(result.lastWindow),
      result.lastWindowStep, result.classificationStartStep, result.deflectedWindows,
      result.firstDeflectedStep, result.longestDeflectionStreak, result.lateralUnits);
   return true;
}

bool observeToFile(const char *path) {
   if (active() || !aipb::explorer::bridge::validMetadata(metadata)
      || std::strcmp(metadata.map, game.getMapName()) != 0) {
      return false;
   }

   aipb::explorer::bridge::ObservationWriteResult summary {};
   if (!aipb::explorer::bridge::writeTraceObservations(trace, metadata.map, path, summary)) {
      return false;
   }

   game.print("AiPB explorer: observations saved: records=%d progress=%d blocked=%d deflected=%d",
      summary.records, summary.progress, summary.blockedCandidates,
      summary.deflectedCandidates);
   return true;
}

bool clear() {
   // Preserve the active recording or replay until it finishes or is stopped.
   if (active()) {
      return false;
   }
   trace.clear();
   metadata = {};
   return true;
}

bool save(const char *filePath) {
   if (active() || !trace.completed || trace.count == 0 || !filePath || !*filePath
      || !aipb::explorer::bridge::validMetadata(metadata)) {
      return false;
   }
   // Use exclusive creation so an existing named trace can never be overwritten.
   std::FILE *file = aipb::explorer::bridge::openTraceFile(filePath, "wx");
   if (!file) {
      return false;
   }
   char buffer[1024] {};
   bool ok = aipb::explorer::bridge::formatMetadata(buffer, sizeof(buffer), metadata)
      && std::fputs(buffer, file) >= 0
      && std::fputs(aipb::explorer::bridge::kTraceCsvColumns, file) >= 0;
   for (int n = 0; ok && n < trace.count; ++n) {
      ok = aipb::explorer::bridge::formatSample(buffer, sizeof(buffer), n, trace.samples[n])
         && std::fputs(buffer, file) >= 0;
   }
   if (std::fclose(file) != 0) {
      ok = false;
   }
   if (!ok) {
      std::remove(filePath);
   }
   return ok;
}

bool load(const char *filePath) {
   if (active() || !filePath || !*filePath) {
      return false;
   }
   std::FILE *file = aipb::explorer::bridge::openTraceFile(filePath, "rt");
   if (!file) {
      return false;
   }
   static TraceBuffer staged {};
   staged.clear();
   aipb::explorer::bridge::TraceMetadata loaded {};
   char line[1024] {};
   bool ok = std::fgets(line, sizeof(line), file)
      && aipb::explorer::bridge::parseMetadata(line, loaded)
      && std::fgets(line, sizeof(line), file)
      && std::strcmp(line, aipb::explorer::bridge::kTraceCsvColumns) == 0;
   if (ok) {
      while (std::fgets(line, sizeof(line), file)) {
         TraceSample sample {};
         if (!aipb::explorer::bridge::parseSample(line, staged.count, sample)
            || (staged.count == 0 && !aipb::explorer::bridge::compareStates(loaded.initial, sample.before).within(0.000001, 0.000001, 0.000001f))
            || !staged.append(sample)) {
            ok = false;
            break;
         }
      }
      if (std::ferror(file) || staged.count == 0) {
         ok = false;
      }
   }
   std::fclose(file);
   if (!ok) {
      // Do not retain a partial allocation after a failed import.
      staged.clear();
      return false;
   }
   staged.completed = true;
   // Transfer ownership of the validated trace without copying its samples.
   trace = std::move(staged);
   staged.clear();
   metadata = loaded;
   return true;
}

bool init(int botIndex) {
   if (active() || !trace.completed || !trace.count || !aipb::explorer::bridge::validMetadata(metadata)
      || std::strcmp(metadata.map, game.getMapName()) != 0) {
      return false;
   }
   auto *bot = bots.findBotByIndex(botIndex);
   if (!bot || !game.isAliveEntity(bot->ent()) || bot->pev->maxspeed < 10.0f) {
      return false;
   }
   const auto &s = metadata.initial;
   const Vector target(static_cast<float>(s.x), static_cast<float>(s.y), static_cast<float>(s.z));
   TraceResult hit {};
   game.testHull(target, target, TraceIgnore::None, s.crouching ? head_hull : human_hull,
      bot->ent(), &hit);
   if (hit.fStartSolid || hit.fAllSolid) {
      game.print("AiPB explorer: init destination obstructed.");
      return false;
   }
   engfuncs.pfnSetOrigin(bot->ent(), target);
   engfuncs.pfnSetSize(bot->ent(), s.crouching ? VEC_DUCK_HULL_MIN : VEC_HULL_MIN,
      s.crouching ? VEC_DUCK_HULL_MAX : VEC_HULL_MAX);
   bot->pev->velocity = Vector(static_cast<float>(s.vx), static_cast<float>(s.vy), static_cast<float>(s.vz));
   bot->pev->basevelocity.clear();
   bot->pev->v_angle = Vector(s.pitch, s.yaw, 0.0f);
   bot->pev->angles = Vector(-s.pitch / 3.0f, s.yaw, 0.0f);
   bot->pev->view_ofs.z = metadata.viewZ;
   bot->pev->button = 0;
   bot->pev->oldbuttons = 0;
   bot->pev->impulse = 0;
   bot->pev->bInDuck = 0;
   bot->pev->flDuckTime = 0;
   if (s.crouching) {
      bot->pev->flags |= FL_DUCKING;
   }
   else {
      bot->pev->flags &= ~FL_DUCKING;
   }
   if (s.on_ground) {
      bot->pev->flags |= FL_ONGROUND;
   }
   else {
      bot->pev->flags &= ~FL_ONGROUND;
   }
   game.print("AiPB explorer: initialized bot #%d at (%.1f %.1f %.1f), yaw %.1f",
      botIndex, s.x, s.y, s.z, s.yaw);
   return true;
}

void reset() {
   if (scenarioScheduler.running()) {
      auto *bot = bots.findBotByIndex(scenarioScheduler.botIndex());
      scenarioScheduler.cancel();
      scenarioEnd(bot == scenarioBot ? bot : nullptr, "cancelled");
   }
   stop("cancelled");
}

void cancelBot(int botIndex) {
   if (scenarioScheduler.running() && scenarioScheduler.botIndex() == botIndex) {
      scenarioScheduler.cancel();
      scenarioEnd(nullptr, "bot disconnected");
   }
   if (session.botIndex == botIndex) {
      stop("bot disconnected");
   }
}

bool frame(Bot &bot) {
   // The first bot frame is the earliest existing AiPB hook with a ready engine.
   // GoldSrc server commands persist across map changes.
   static bool aipbCommandRegistered {};
   if (!aipbCommandRegistered) {
      game.registerEngineCommand("aipb", [] () {
         ai::menu::handleServer();
      });
      aipbCommandRegistered = true;
   }

   if (scenarioScheduler.running() && scenarioScheduler.botIndex() == bot.index()) {
      const auto currentTeam = bot.m_team == Team::CT ? 2
         : bot.m_team == Team::Terrorist ? 1 : 0;
      const aipb::explorer::scenario::RunLifecycleObservation life {
         gameState.isRoundOver(), gameState.getTelemetryRoundId(), game.getMapName(),
         true, &bot == scenarioBot, game.isAliveEntity(bot.ent()), currentTeam,
         std::isfinite(bot.pev->maxspeed) && bot.pev->maxspeed >= 10.0f,
         session.botIndex >= 0 || queue.running
      };
      const auto failure = aipb::explorer::scenario::checkRunLifecycle(
         life, scenarioMap, scenarioRoundId, scenarioTeam);
      if (failure != aipb::explorer::scenario::RunLifecycleFailure::None) {
         scenarioScheduler.cancel();
         scenarioEnd(&bot == scenarioBot ? &bot : nullptr,
            aipb::explorer::scenario::runLifecycleFailureName(failure));
         return true;
      }
      const float now = game.time();
      const float elapsed = now - scenarioLastTime;
      scenarioLastTime = now;
      const int msec = std::isfinite(elapsed) && elapsed > 0.0f
         ? cr::clamp(static_cast<int>(elapsed * 1000.0f + 0.5f), 1, 255) : 1;
      const int64_t clock = scenarioTimeMs();
      const auto planned = scenarioScheduler.prepareFrame(clock, msec);
      if (!planned.ticket) {
         scenarioEnd(&bot, "scheduler stopped");
         return true;
      }
      if (scenarioActionIndex != static_cast<int>(planned.actionIndex)) {
         scenarioActionIndex = static_cast<int>(planned.actionIndex);
         scenarioActionYaw = bot.pev->v_angle.y;
         scenarioActionPitch = bot.pev->v_angle.x;
      }
      const auto movement = aipb::explorer::scenario::planScenarioFrameMovement(
         planned, scenarioScheduler.actionMilliseconds(), scenarioActionYaw,
         scenarioActionPitch, bot.pev->maxspeed);
      if (!movement.valid) {
         scenarioScheduler.cancel();
         scenarioEnd(&bot, "invalid movement plan");
         return true;
      }
      auto angles = bot.pev->v_angle;
      angles.y = movement.yaw;
      angles.x = movement.pitch;
      bot.pev->v_angle = angles;
      bot.pev->angles.x = -angles.x / 3.0f;
      bot.pev->angles.y = angles.y;
      bot.pev->angles.z = 0.0f;
      const uint16_t buttons = static_cast<uint16_t>(
         (movement.jump ? IN_JUMP : 0) | (movement.duck ? IN_DUCK : 0));
      bot.pev->button = buttons;
      bot.pev->impulse = 0;
      const auto beforeMove = scenarioPose(bot);
      const int oldButtons = bot.pev->oldbuttons;
      const float maxSpeed = bot.pev->maxspeed;
      engfuncs.pfnRunPlayerMove(bot.ent(), angles, movement.forwardSpeed,
         movement.strafeSpeed, 0.0f, buttons, 0, static_cast<uint8_t>(planned.milliseconds));
      const auto afterMove = scenarioPose(bot);
      const aipb::explorer::scenario::ScenarioTraceFrame observed {
         planned.frameIndex, planned.actionIndex, planned.milliseconds,
         movement.forwardSpeed, movement.strafeSpeed, maxSpeed,
         static_cast<unsigned>(buttons), oldButtons, movement.yaw, movement.pitch,
         beforeMove, afterMove
      };
      if (!scenarioFrameTrace.append(observed)) {
         scenarioScheduler.cancel();
         scenarioEnd(&bot, "trace invalid");
         return true;
      }
      scenarioLastCommitted = afterMove;
      scenarioHaveLastCommitted = true;
      if (!scenarioScheduler.commitFrame(planned, scenarioTimeMs())) {
         scenarioScheduler.cancel();
         scenarioEnd(&bot, "frame commit failed");
      }
      else if (!scenarioScheduler.running()) {
         scenarioEnd(&bot, "finished");
      }
      return true;
   }

   if (session.botIndex != bot.index()) {
      return false;
   }
   if (session.waitingForKnife) {
      if (!game.isAliveEntity(bot.ent()) || !std::isfinite(bot.pev->maxspeed)
         || game.time() >= session.knifeDeadline) {
         stop("knife fallback unavailable or timed out");
         return true;
      }
      aipb::explorer::bridge::ReplaySpeedRequirement requirement {};
      if (!aipb::explorer::bridge::replaySpeedRequirement(trace, requirement)) {
         stop("invalid replay speed evidence");
         return true;
      }
      if (bot.pev->maxspeed + 0.01f < requirement.maximumRequested) {
         return true;
      }
      const auto difference = compareStates(trace.samples[0].before, observe(bot));
      if (!difference.within(5.0, 30.0, 5.0f)) {
         stop("replay start changed during knife selection; use init and replay again");
         return true;
      }
      session.waitingForKnife = false;
      session.replay = true;
      session.replayIndex = 0;
      session.lastTime = game.time();
      game.print("AiPB explorer: knife fallback ready, limit=%.1f; replay started.",
         bot.pev->maxspeed);
      return true;
   }
   if (!game.isAliveEntity(bot.ent()) || !std::isfinite(bot.pev->maxspeed)
      || bot.pev->maxspeed < 10.0f || (session.replay ? session.replayIndex >= trace.count : session.budget.complete())) {
      stop("bot unavailable");
      return false;
   }

   const float now = game.time();
   const float elapsed = now - session.lastTime;
   session.lastTime = now;
   // Use real engine frame duration rather than inventing fixed GoldSrc physics parity.
   const int milliseconds = std::isfinite(elapsed) && elapsed > 0.0f
      ? cr::clamp(static_cast <int> (elapsed * 1000.0f + 0.5f), 1, 255) : 1;
   const bool replaying = session.replay;
   const TraceSample *recorded = replaying ? &trace.samples[session.replayIndex] : nullptr;
   if (replaying && bot.pev->maxspeed + 0.01f
      < aipb::explorer::bridge::requestedHorizontalSpeed(*recorded)) {
      game.print("AiPB explorer: replay speed limit dropped at step %d: required=%.1f, current=%.1f",
         session.replayIndex, aipb::explorer::bridge::requestedHorizontalSpeed(*recorded),
         bot.pev->maxspeed);
      stop("replay speed limit insufficient");
      return true;
   }
   const uint8_t msec = replaying ? recorded->msec : static_cast<uint8_t>(milliseconds);
   const auto before = observe(bot);
   if (replaying) {
      const auto diff = compareStates(recorded->before, before);
      if (!diff.within(5.0, 30.0, 5.0f)) {
         game.print("AiPB explorer: replay diverged before step %d, pos=%.2f vel=%.2f yaw=%.2f",
            session.replayIndex, diff.position, diff.velocity, diff.yawDegrees);
         stop("replay divergence");
         return true;
      }
   }
   const auto command = replaying ? recorded->command : session.command;

   float forwardSpeed {};
   float strafeSpeed {};
   aipb::explorer::bridge::translateAxes(command, replaying ? recorded->maxSpeed : bot.pev->maxspeed,
      forwardSpeed, strafeSpeed);

   auto angles = bot.pev->v_angle;
   angles.y = aipb::explorer::bridge::yaw(angles.y, command.yaw_delta);
   bot.pev->v_angle = angles;
   // Preserve YaPB's view-to-body angle convention when overriding AI movement.
   bot.pev->angles.x = -angles.x / 3.0f;
   bot.pev->angles.y = angles.y;
   bot.pev->angles.z = 0.0f;
   const uint16_t buttons = static_cast <uint16_t> (
      (command.jump ? IN_JUMP : 0) | (command.duck ? IN_DUCK : 0));
   bot.pev->button = buttons;
   bot.pev->impulse = 0;

   const int oldButtons = bot.pev->oldbuttons;
   const float commandedMaxSpeed = replaying ? recorded->maxSpeed : bot.pev->maxspeed;
   engfuncs.pfnRunPlayerMove(bot.ent(), angles, forwardSpeed, strafeSpeed, 0.0f, buttons, 0, msec);

   if (replaying) {
      ++session.replayIndex;
   }
   else {
      session.budget.consume(msec);
   }
   auto after = observe(bot);
   if (!replaying) {
      after.tick = before.tick + 1;
   }
   if (replaying) {
      const auto delta = compareStates(recorded->after, after);
      if (!delta.within(5.0, 30.0, 5.0f)) {
         game.print("AiPB explorer: replay diverged at step %d, pos=%.2f vel=%.2f yaw=%.2f stance=%d ground=%d",
            session.replayIndex, delta.position, delta.velocity, delta.yawDegrees,
            static_cast<int>(delta.stanceMismatch), static_cast<int>(delta.groundMismatch));
         stop("replay divergence");
         return true;
      }
      if (session.replayIndex == trace.count) {
         stop("replay matched");
      }
      return true;
   }
   const TraceSample sample { command, before, after, commandedMaxSpeed, msec, oldButtons };
   if (!trace.append(sample)) {
      stop("trace capacity or validation failure");
      trace.clear();
      return true;
   }
   if (session.budget.executed == 1 || session.budget.simulatedMs / 50 !=
      (session.budget.simulatedMs - milliseconds) / 50 || session.budget.complete()) {
      game.print("AiPB explorer: frame=%d, total_ms=%d, msec=%u, pos=(%.1f %.1f %.1f), vel=(%.1f %.1f %.1f), view_yaw=%.1f, body_yaw=%.1f, buttons=%u, oldbuttons=%d, ground=%u, duck=%u, duck_transition=%d, duck_time=%d, view_z=%.1f",
         session.budget.executed, session.budget.simulatedMs, static_cast <unsigned> (msec),
         after.x, after.y, after.z, after.vx, after.vy, after.vz, after.yaw,
         bot.pev->angles.y, static_cast <unsigned> (buttons), bot.pev->oldbuttons,
         static_cast <unsigned> (after.on_ground), static_cast <unsigned> (after.crouching),
         bot.pev->bInDuck, bot.pev->flDuckTime, bot.pev->view_ofs.z);
   }

   // Movement and collisions remain engine-owned; no hit claim is inferred from lack of progress.
   if (!std::isfinite(after.x) || !std::isfinite(after.y) || !std::isfinite(after.z)
      || !std::isfinite(after.vx) || !std::isfinite(after.vy) || !std::isfinite(after.vz)) {
      stop("nonfinite engine observation");
   }
   else if (session.budget.complete()) {
      if (queue.running && queue.current() && queue.advance()) {
         const auto *next = queue.current();
         session.command = next->command;
         session.budget = next->budget;
         session.lastTime = game.time();
         game.print("AiPB explorer: queue segment %d/%d, total recorded %d frames",
            queue.next + 1, queue.count, trace.count);
      }
      else {
         trace.completed = true;
         const bool queued = queue.running;
         char completedPath[sizeof(queueSavePath)] {};
         if (queued) {
            std::snprintf(completedPath, sizeof(completedPath), "%s", queueSavePath);
         }
         stop(session.budget.safetyLimit() ? "safety frame limit" : "completed");
         if (queued) {
            queue.clear();
            if (save(completedPath)) {
               game.print("AiPB explorer: queue saved %d frames to %s", trace.count, completedPath);
            }
            else {
               game.print("AiPB explorer: queue completed but save failed; trace retained.");
            }
         }
         else {
            game.print("AiPB explorer: recorded %d steps", trace.count);
         }
      }
   }
   return true;
}

}
