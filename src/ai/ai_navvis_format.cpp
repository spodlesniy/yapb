#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

#include <ai/ai_navvis_format.h>

namespace ai::navvis {
namespace {
constexpr unsigned kMaxPoints = 100000;
constexpr unsigned kMaxEdges = 150000;
constexpr std::size_t kMaxLine = 256;

char *nextToken (char *&cursor) {
   while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') {
      ++cursor;
   }
   if (*cursor == '\0') {
      return nullptr;
   }
   char *token = cursor;

   while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t' && *cursor != '\r' && *cursor != '\n') {
      ++cursor;
   }
   if (*cursor != '\0') {
      *cursor++ = '\0';
   }
   return token;
}

bool readTokens (std::FILE *file, char (&line)[kMaxLine], char **tokens, std::size_t count) {
   if (!std::fgets(line, static_cast<int>(sizeof(line)), file)) {
      return false;
   }
   if (!std::strchr(line, '\n') && !std::feof(file)) {
      return false;
   }
   char *cursor = line;

   for (std::size_t i = 0; i < count; ++i) {
      tokens[i] = nextToken(cursor);
      if (!tokens[i]) {
         return false;
      }
   }
   return nextToken(cursor) == nullptr;
}

bool readUnsigned (const char *text, unsigned &result) {
   if (!text || !*text) {
      return false;
   }
   for (const char *digit = text; *digit; ++digit) {
      if (*digit < '0' || *digit > '9') {
         return false;
      }
   }
   errno = 0;
   char *end = nullptr;
   const unsigned long parsed = std::strtoul(text, &end, 10);

   if (errno == ERANGE || *end != '\0' || parsed > UINT_MAX) {
      return false;
   }
   result = static_cast<unsigned>(parsed);
   return true;
}

bool readFloat (const char *text, float &result) {
   if (!text || !*text) {
      return false;
   }
   errno = 0;
   char *end = nullptr;
   const float parsed = std::strtof(text, &end);

   if (errno == ERANGE || end == text || *end != '\0' || !std::isfinite(parsed)) {
      return false;
   }
   result = parsed;
   return true;
}

bool validDigest (const char *digest) {
   if (std::strlen(digest) != 64) {
      return false;
   }
   for (const char *ch = digest; *ch; ++ch) {
      if (!(*ch >= '0' && *ch <= '9') && !(*ch >= 'a' && *ch <= 'f')) {
         return false;
      }
   }
   return true;
}

bool loadRecords (std::FILE *file, const char *expectedMap, Data &result) {
   char line[kMaxLine] {};
   char *header[4] {};

   if (!readTokens(file, line, header, 4) || std::strcmp(header[0], "AIPBNAVVIS") != 0 ||
       std::strcmp(header[1], "1") != 0 || std::strcmp(header[2], expectedMap) != 0 || !validDigest(header[3])) {
      return false;
   }
   result.map = header[2];
   result.sha256 = header[3];

   char *counts[3] {};
   unsigned points = 0, edges = 0;

   if (!readTokens(file, line, counts, 3) || std::strcmp(counts[0], "COUNT") != 0 ||
       !readUnsigned(counts[1], points) || !readUnsigned(counts[2], edges) ||
       points > kMaxPoints || edges > kMaxEdges) {
      return false;
   }
   result.points.reserve(points);
   result.edges.reserve(edges);

   for (unsigned i = 0; i < points; ++i) {
      char *tokens[9] {};
      unsigned id = 0, source = 0, standing = 0, crouching = 0, isolated = 0;
      Point point;

      if (!readTokens(file, line, tokens, 9) || std::strcmp(tokens[0], "V") != 0 ||
          !readUnsigned(tokens[1], id) || !id ||
          !readFloat(tokens[2], point.x) || !readFloat(tokens[3], point.y) || !readFloat(tokens[4], point.z) ||
          !readUnsigned(tokens[5], source) || source > 3 ||
          !readUnsigned(tokens[6], standing) || standing > 1 ||
          !readUnsigned(tokens[7], crouching) || crouching > 1 ||
          !readUnsigned(tokens[8], isolated) || isolated > 1 || (!standing && !crouching)) {
         return false;
      }
      point.id = id;
      point.source = static_cast<int>(source);
      point.standing = standing != 0;
      point.crouching = crouching != 0;
      point.isolated = isolated != 0;
      result.points.push_back(point);
   }
   std::sort(result.points.begin(), result.points.end(), [](const Point &a, const Point &b) {
      return a.id < b.id;
   });
   for (std::size_t i = 1; i < result.points.size(); ++i) {
      if (result.points[i - 1].id == result.points[i].id) {
         return false;
      }
   }
   const auto exists = [&result](unsigned id) {
      const auto it = std::lower_bound(result.points.begin(), result.points.end(), id,
         [](const Point &p, unsigned key) { return p.id < key; });
      return it != result.points.end() && it->id == id;
   };

   for (unsigned i = 0; i < edges; ++i) {
      char *tokens[4] {};
      unsigned from = 0, to = 0, type = 0;

      if (!readTokens(file, line, tokens, 4) || std::strcmp(tokens[0], "E") != 0 ||
          !readUnsigned(tokens[1], from) || !readUnsigned(tokens[2], to) ||
          !readUnsigned(tokens[3], type) || type > 3 || from == to || !exists(from) || !exists(to)) {
         return false;
      }
      result.edges.push_back({ from, to, static_cast<int>(type) });
   }
   char *end[1] {};

   if (!readTokens(file, line, end, 1) || std::strcmp(end[0], "END") != 0) {
      return false;
   }
   int trailing = 0;
   do {
      trailing = std::fgetc(file);
   } while (trailing == ' ' || trailing == '\t' || trailing == '\r' || trailing == '\n');
   return trailing == EOF && !std::ferror(file);
}
} // namespace

bool load (const char *filename, const char *expectedMap, Data &out) {
   if (!filename || !expectedMap || !*expectedMap) {
      return false;
   }
   std::FILE *file = std::fopen(filename, "rb");

   if (!file) {
      return false;
   }
   Data candidate;
   const bool success = loadRecords(file, expectedMap, candidate);
   const int closeResult = std::fclose(file);

   if (!success || closeResult != 0) {
      return false;
   }
   out = std::move(candidate);
   return true;
}
} // namespace ai::navvis
