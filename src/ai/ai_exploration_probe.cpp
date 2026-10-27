#include <yapb.h>
#include <ai/ai_exploration_probe.h>
#include <aipb/explorer/engine_bridge_math.h>

#include <cmath>
#include <cstdint>

namespace ai::explore {
namespace {

struct Session {
   AipbExplorerBridgeCommand command {};
   int botIndex { -1 };
   aipb::explorer::bridge::ProbeBudget budget {};
   float lastTime {};
};

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
   state.tick = static_cast <uint64_t> (session.budget.executed);
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
}

} // namespace

bool active() {
   return session.botIndex >= 0;
}

bool begin(int botIndex, const AipbExplorerBridgeCommand &command, int limit, bool milliseconds) {
   const aipb::explorer::bridge::ProbeBudget budget { limit, milliseconds };
   if (active() || botIndex < 0 || !aipb::explorer::bridge::valid(command) || !budget.valid()) {
      return false;
   }
   auto *bot = bots.findBotByIndex(botIndex);

   if (bot == nullptr || !game.isAliveEntity(bot->ent()) || !std::isfinite(bot->pev->maxspeed)
      || bot->pev->maxspeed < 10.0f) {
      return false;
   }

   session = {};
   session.botIndex = botIndex;
   session.command = command;
   session.budget = budget;
   session.lastTime = game.time();
   game.print("AiPB explorer: started bot #%d for %d %s", botIndex, limit,
      milliseconds ? "simulated ms" : "frames");
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
      || bot.pev->maxspeed < 10.0f || session.budget.complete()) {
      stop("bot unavailable");
      return false;
   }

   const float now = game.time();
   const float elapsed = now - session.lastTime;
   session.lastTime = now;
   // Use real engine frame duration rather than inventing fixed GoldSrc physics parity.
   const int milliseconds = std::isfinite(elapsed) && elapsed > 0.0f
      ? cr::clamp(static_cast <int> (elapsed * 1000.0f + 0.5f), 1, 255) : 1;
   const uint8_t msec = static_cast <uint8_t> (milliseconds);

   float forwardSpeed {};
   float strafeSpeed {};
   aipb::explorer::bridge::translateAxes(session.command, bot.pev->maxspeed,
      forwardSpeed, strafeSpeed);

   auto angles = bot.pev->v_angle;
   angles.y = aipb::explorer::bridge::yaw(angles.y, session.command.yaw_delta);
   bot.pev->v_angle = angles;
   // Preserve YaPB's view-to-body angle convention when overriding AI movement.
   bot.pev->angles.x = -angles.x / 3.0f;
   bot.pev->angles.y = angles.y;
   bot.pev->angles.z = 0.0f;
   const uint16_t buttons = static_cast <uint16_t> (
      (session.command.jump ? IN_JUMP : 0) | (session.command.duck ? IN_DUCK : 0));
   bot.pev->button = buttons;
   bot.pev->impulse = 0;

   engfuncs.pfnRunPlayerMove(bot.ent(), angles, forwardSpeed, strafeSpeed, 0.0f, buttons, 0, msec);

   session.budget.consume(milliseconds);
   const auto after = observe(bot);
   if (session.budget.executed == 1 || session.budget.simulatedMs / 50 !=
      (session.budget.simulatedMs - milliseconds) / 50 || session.budget.complete()) {
      game.print("AiPB explorer: frame=%d, total_ms=%d, msec=%u, pos=(%.1f %.1f %.1f), vel=(%.1f %.1f %.1f), view_yaw=%.1f, body_yaw=%.1f, buttons=%u, oldbuttons=%d, ground=%u, duck=%u, duck_transition=%d, duck_time=%.0f, view_z=%.1f",
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
      stop(session.budget.safetyLimit() ? "safety frame limit" : "completed");
   }
   return true;
}

}
