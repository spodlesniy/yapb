#include <yapb.h>
#include <ai/ai_exploration_probe.h>
#include <aipb/explorer/engine_bridge_math.h>
#include <aipb/explorer/engine_bridge_queue.h>
#include <aipb/explorer/engine_bridge_trace.h>
#include <aipb/explorer/engine_bridge_trace_io.h>
#include <aipb/explorer/engine_bridge_trace_report.h>
#include <crlib/files.h>

#include <cstdio>
#include <cstring>

#include <cmath>
#include <cstdint>
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
};

using aipb::explorer::bridge::TraceBuffer;
using aipb::explorer::bridge::TraceSample;
using aipb::explorer::bridge::compareStates;
static TraceBuffer trace {};
static aipb::explorer::bridge::ProbeQueue queue {};
static char queueSavePath[512] {};
static aipb::explorer::bridge::TraceMetadata metadata {};

Session session {};

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

bool active() {
   return session.botIndex >= 0;
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
   if (!bot || !game.isAliveEntity(bot->ent()) || bot->pev->maxspeed < 10.0f) {
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
   session = {};
   session.botIndex = botIndex;
   session.replay = true;
   session.lastTime = game.time();
   game.print("AiPB explorer: replay started, %d steps", trace.count);
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
   stop("cancelled");
}

void cancelBot(int botIndex) {
   if (session.botIndex == botIndex) {
      stop("bot disconnected");
   }
}

bool frame(Bot &bot) {
   if (session.botIndex != bot.index()) {
      return false;
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
