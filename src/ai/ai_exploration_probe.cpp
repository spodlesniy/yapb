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
   int remaining {};
   int requested {};
   int executed {};
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
   state.tick = static_cast <uint64_t> (session.executed);
   state.on_ground = (v.flags & FL_ONGROUND) ? 1 : 0;
   state.crouching = (v.flags & FL_DUCKING) ? 1 : 0;
   return state;
}

void stop(const char *reason) {
   if (session.botIndex >= 0) {
      game.print("AiPB explorer: bot #%d, %d/%d frames: %s", session.botIndex,
         session.executed, session.requested, reason);
   }
   session = {};
   session.botIndex = -1;
}

} // namespace

bool active() {
   return session.botIndex >= 0;
}

bool begin(int botIndex, const AipbExplorerBridgeCommand &command, int frameCount) {
   if (active() || botIndex < 0 || !aipb::explorer::bridge::valid(command)
      || frameCount < 1 || frameCount > 128) {
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
   session.remaining = frameCount;
   session.requested = frameCount;
   session.lastTime = game.time();
   game.print("AiPB explorer: started bot #%d for %d frames", botIndex, frameCount);
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
      || bot.pev->maxspeed < 10.0f || session.remaining <= 0) {
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

   const auto before = observe(bot);
   float forwardSpeed {};
   float strafeSpeed {};
   aipb::explorer::bridge::translateAxes(session.command, bot.pev->maxspeed,
      forwardSpeed, strafeSpeed);

   auto angles = bot.pev->v_angle;
   angles.y = aipb::explorer::bridge::yaw(angles.y, session.command.yaw_delta);
   bot.pev->v_angle = angles;
   const uint16_t buttons = static_cast <uint16_t> (
      (session.command.jump ? IN_JUMP : 0) | (session.command.duck ? IN_DUCK : 0));
   bot.pev->button = buttons;
   bot.pev->impulse = 0;

   engfuncs.pfnRunPlayerMove(bot.ent(), angles, forwardSpeed, strafeSpeed, 0.0f, buttons, 0, msec);

   ++session.executed;
   --session.remaining;
   const auto after = observe(bot);
   game.print("AiPB explorer: frame %d/%d, msec=%u, pos=(%.1f %.1f %.1f), vel=(%.1f %.1f %.1f), yaw=%.1f, ground=%u, duck=%u",
      session.executed, session.requested, static_cast <unsigned> (msec),
      after.x, after.y, after.z, after.vx, after.vy, after.vz, after.yaw,
      static_cast <unsigned> (after.on_ground), static_cast <unsigned> (after.crouching));

   // Movement and collisions remain engine-owned; no hit claim is inferred from lack of progress.
   if (!std::isfinite(after.x) || !std::isfinite(after.y) || !std::isfinite(after.z)
      || !std::isfinite(after.vx) || !std::isfinite(after.vy) || !std::isfinite(after.vz)) {
      stop("nonfinite engine observation");
   }
   else if (session.remaining == 0) {
      stop("completed");
   }
   (void)before;
   return true;
}

}
