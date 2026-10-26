#include <yapb.h>
#include <ai/ai_navvis.h>
#include <ai/ai_navvis_select.h>

#include <algorithm>
#include <cmath>

namespace ai::navvis {
namespace {
constexpr int kMaxPlayers = 32;
constexpr int kNodesPerFrame = 48;
constexpr int kEdgesPerFrame = 24;
struct Setting {
   bool enabled {};
   unsigned mask { 0x7f };
   float nextDraw {};
   float nextHud {};
   unsigned lastNearest {};
};
Setting settings[kMaxPlayers] {};
Data data {};
bool loaded {};
std::string currentMap;
float drawRadius = 450.0f;

const Point *findPoint(unsigned id) {
   const auto it = std::lower_bound(data.points.begin(), data.points.end(), id,
      [](const Point &p, unsigned key) { return p.id < key; });
   return it != data.points.end() && it->id == id ? &*it : nullptr;
}

Vector coords(const Point &p) {
   return Vector(p.x, p.y, p.z);
}

bool allowed(const Setting &setting, const Point &p) {
   return (setting.mask & (1u << p.source)) != 0;
}

bool authorized(edict_t *ent) {
   if (!game.isPlayerEntity(ent) || game.isFakeClientEntity(ent)) {
      return false;
   }
   const int index = game.indexOfPlayer(ent);
   return validPlayerIndex(index, kMaxPlayers) &&
      (ent == game.getLocalEntity() || (util.getClient(index).flags & ClientFlags::Admin));
}

void ensureMap() {
   const std::string mapName = game.getMapName();
   if (currentMap == mapName) {
      return;
   }
   currentMap = mapName;
   loaded = false;
   data = {};
   for (auto &setting : settings) {
      setting = {};
   }
   const String directory = strings.joinPath(bstor.getRunningPath(), "data", "navvis");
   const String filename = strings.joinPath(directory, strings.format("%s.nviz", mapName.c_str()));
   loaded = load(filename.chars(), mapName.c_str(), data);
}
} // namespace

void frame(edict_t *editor) {
   if (!authorized(editor)) {
      return;
   }
   ensureMap();
   if (!loaded) {
      return;
   }
   const int index = game.indexOfPlayer(editor);
   auto &setting = settings[index];

   if (!setting.enabled || game.time() < setting.nextDraw) {
      return;
   }
   setting.nextDraw = game.time() + 0.2f;

   const Vector view = editor->v.origin;
   NearbyPoint <Point> nearby[kNodesPerFrame] {};
   const std::size_t count = selectNearby(data.points, setting.mask,
      view.x, view.y, view.z, drawRadius, nearby);

   if (count == 0) {
      return;
   }

   for (std::size_t i = 0; i < count; ++i) {
      const Point &point = *nearby[i].point;
      const Vector pos = coords(point);
      const Color color = point.isolated ? Color { 240, 70, 70 } :
                          point.source == 3 ? Color { 230, 160, 55 } :
                          point.source == 2 ? Color { 50, 210, 80 } :
                          point.source == 1 ? Color { 165, 120, 255 } : Color { 55, 175, 235 };
      game.drawLine(editor, pos + Vector(0, 0, 2), pos + Vector(0, 0, 26),
         4, 0, color, 220, 0, 3);
   }

   const Point *nearest = nearby[0].point;
   int edgeCount = 0;
   unsigned outgoing = 0;

   for (const auto &edge : data.edges) {
      if (edge.from != nearest->id) {
         continue;
      }
      ++outgoing;

      if (edgeCount >= kEdgesPerFrame || (setting.mask & (1u << (edge.type + 4))) == 0) {
         continue;
      }
      const auto *target = findPoint(edge.to);

      if (!target || !allowed(setting, *target)) {
         continue;
      }
      const Color color = edge.type == 0 ? Color { 80, 230, 110 } :
                          edge.type == 1 ? Color { 245, 225, 60 } : Color { 240, 65, 205 };
      game.drawLine(editor, coords(*nearest) + Vector(0, 0, 12),
         coords(*target) + Vector(0, 0, 12), 4, 0, color, 220, 0, 3,
         DrawLine::Arrow);
      ++edgeCount;
   }

   if (setting.lastNearest != nearest->id || game.time() >= setting.nextHud) {
      const char *sources[] = { "world", "spawn", "brush", "conditional" };
      const char *stance = nearest->standing ? (nearest->crouching ? "stand/duck" : "stand") : "duck";
      game.centerPrint(editor, strings.format("AiPB %u (%.0f %.0f %.0f) %s %s | out %u",
         nearest->id, nearest->x, nearest->y, nearest->z, sources[nearest->source], stance, outgoing));
      setting.lastNearest = nearest->id;
      setting.nextHud = game.time() + 1.0f;
   }
}

void toggle(edict_t *editor, int option) {
   if (!authorized(editor) || option < 0 || option > 7) {
      return;
   }
   ensureMap();
   auto &setting = settings[game.indexOfPlayer(editor)];
   if (option == 0) {
      setting.enabled = !setting.enabled;
      if (setting.enabled && !loaded) {
         game.clientPrint(editor, "AiPB navigation viewer: no valid .nviz file for this map.");
      }
   }
   else {
      setting.mask ^= 1u << (option - 1);
   }
}

void radius(float value) {
   if (std::isfinite(value) && value >= 64.0f && value <= 2048.0f) {
      drawRadius = value;
   }
}

float radius() {
   return drawRadius;
}

bool canView(edict_t *editor) {
   return authorized(editor);
}

void resetPlayer(edict_t *editor) {
   if (!editor) {
      return;
   }
   const int index = game.indexOfPlayer(editor);

   if (validPlayerIndex(index, kMaxPlayers)) {
      settings[index] = {};
   }
}

void reset() {
   currentMap.clear();
   loaded = false;
   data = {};
   for (auto &setting : settings) {
      setting = {};
   }
}
} // namespace ai::navvis
