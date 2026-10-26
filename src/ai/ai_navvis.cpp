#include <yapb.h>
#include <ai/ai_navvis.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <utility>
#include <string>
#include <vector>

namespace ai::navvis {
namespace {
constexpr int kMaxPlayers = 32;
constexpr unsigned kMaxPoints = 100000;
constexpr unsigned kMaxEdges = 150000;
constexpr int kNodesPerFrame = 48;
constexpr int kEdgesPerFrame = 24;
struct Setting {
   bool enabled {};
   unsigned mask { 0x7f };
   float nextDraw {};
   float nextHud {};
   unsigned lastNearest {};
};
Setting settings[kMaxPlayers + 1] {};
Data data {};
bool loaded {};
std::string currentMap;
float drawRadius = 450.0f;

bool validDigest(const std::string &s) {
   if (s.size() != 64) {
      return false;
   }
   for (const char ch : s) {
      if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'))) {
         return false;
      }
   }
   return true;
}

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
   return index >= 1 && index <= kMaxPlayers &&
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

bool load(const char *filename, const char *expectedMap, Data &out) {
   std::ifstream input(filename, std::ios::binary);
   std::string marker, version, map, digest;
   unsigned count = 0, edges = 0;

   if (!(input >> marker >> version >> map >> digest) ||
       marker != "AIPBNAVVIS" || version != "1" || map != expectedMap ||
       !validDigest(digest) || !(input >> marker >> count >> edges) ||
       marker != "COUNT" || count > kMaxPoints || edges > kMaxEdges) {
      return false;
   }
   Data candidate;
   candidate.map = map;
   candidate.sha256 = digest;
   candidate.points.reserve(count);
   candidate.edges.reserve(edges);

   for (unsigned i = 0; i < count; ++i) {
      Point p;
      int standing = 0, crouching = 0, isolated = 0;
      if (!(input >> marker >> p.id >> p.x >> p.y >> p.z >> p.source >> standing >> crouching >> isolated) ||
          marker != "V" || !p.id || p.source < 0 || p.source > 3 ||
          (standing != 0 && standing != 1) || (crouching != 0 && crouching != 1) || (isolated != 0 && isolated != 1) ||
          !std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) ||
          (!standing && !crouching)) {
         return false;
      }
      p.standing = standing != 0;
      p.crouching = crouching != 0;
      p.isolated = isolated != 0;
      candidate.points.push_back(p);
   }
   std::sort(candidate.points.begin(), candidate.points.end(),
      [](const Point &a, const Point &b) { return a.id < b.id; });
   for (size_t i = 1; i < candidate.points.size(); ++i) {
      if (candidate.points[i - 1].id == candidate.points[i].id) {
         return false;
      }
   }
   const auto exists = [&candidate](unsigned id) {
      const auto it = std::lower_bound(candidate.points.begin(), candidate.points.end(), id,
         [](const Point &p, unsigned value) { return p.id < value; });
      return it != candidate.points.end() && it->id == id;
   };

   for (unsigned i = 0; i < edges; ++i) {
      Edge edge;
      if (!(input >> marker >> edge.from >> edge.to >> edge.type) ||
          marker != "E" || edge.type < 0 || edge.type > 2 ||
          !exists(edge.from) || !exists(edge.to) || edge.from == edge.to) {
         return false;
      }
      candidate.edges.push_back(edge);
   }
   if (!(input >> marker) || marker != "END" || (input >> marker)) {
      return false;
   }
   out = std::move(candidate);
   return true;
}

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
   const float squaredRadius = drawRadius * drawRadius;
   const Point *nearest = nullptr;
   float nearestDistance = squaredRadius;
   int drawn = 0;

   for (const auto &point : data.points) {
      if (!allowed(setting, point)) {
         continue;
      }
      const Vector pos = coords(point);
      const float distance = (pos - view).lengthSq();
      if (distance > squaredRadius) {
         continue;
      }
      if (distance < nearestDistance) {
         nearestDistance = distance;
         nearest = &point;
      }
      if (++drawn > kNodesPerFrame) {
         continue;
      }
      const Color color = point.isolated ? Color { 240, 70, 70 } :
                          point.source == 3 ? Color { 230, 160, 55 } :
                          point.source == 2 ? Color { 50, 210, 80 } :
                          point.source == 1 ? Color { 165, 120, 255 } : Color { 55, 175, 235 };
      game.drawLine(editor, pos + Vector(0, 0, 2), pos + Vector(0, 0, 26),
         4, 0, color, 220, 0, 3);
   }

   if (!nearest) {
      return;
   }
   if (setting.lastNearest != nearest->id || game.time() >= setting.nextHud) {
      game.centerPrint(editor, strings.format("AiPB node %u | Z %.1f | %s",
         nearest->id, nearest->z, nearest->standing ? "standing" : "crouching"));
      setting.lastNearest = nearest->id;
      setting.nextHud = game.time() + 1.0f;
   }
   int edgeCount = 0;
   for (const auto &edge : data.edges) {
      if (edge.from != nearest->id || (setting.mask & (1u << (edge.type + 4))) == 0) {
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
      if (++edgeCount == kEdgesPerFrame) {
         break;
      }
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

void reset() {
   currentMap.clear();
   loaded = false;
   data = {};
   for (auto &setting : settings) {
      setting = {};
   }
}
} // namespace ai::navvis
