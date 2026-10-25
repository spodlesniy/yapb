// AiPB - read-only waypoint topology quality metrics.
#pragma once

namespace ai {

constexpr int kGraphQualityMaxNodes = 4096;
constexpr int kGraphQualityMaxLinks = 8;

struct GraphQualityReport {
  bool valid {};
  int nodes {};
  int edges {};
  int invalidLinks {};
  int selfLinks {};
  int duplicateLinks {};
  int oneWayLinks {};
  int isolatedNodes {};
  int weakComponents {};
  int largestWeakComponent {};
  int goals {};
  int rescues {};
  int terroristSources {};
  int ctSources {};
  int terroristReachableGoals {};
  int ctReachableGoals {};
  int ctReachableRescues {};
};

// The provider returns -1 for unused edge slots and a node index otherwise.
// Spawn sources are caller-supplied graph nodes; the function never infers
// spawn locations from tactical waypoint flags.
template <typename LinkAt, typename IsGoal, typename IsRescue>
GraphQualityReport measureGraphQuality(int nodeCount, const bool *terroristStarts, const bool *ctStarts,
    LinkAt linkAt, IsGoal isGoal, IsRescue isRescue) {
  GraphQualityReport result {};
  if (nodeCount < 0 || nodeCount > kGraphQualityMaxNodes) {
    return result;
  }
  result.valid = true;
  result.nodes = nodeCount;

  int parent[kGraphQualityMaxNodes] {};
  int sizes[kGraphQualityMaxNodes] {};
  int inDegree[kGraphQualityMaxNodes] {};
  int outDegree[kGraphQualityMaxNodes] {};
  for (int i = 0; i < nodeCount; ++i) {
    parent[i] = i;
    sizes[i] = 1;
    result.goals += isGoal(i) ? 1 : 0;
    result.rescues += isRescue(i) ? 1 : 0;
    result.terroristSources += terroristStarts != nullptr && terroristStarts[i] ? 1 : 0;
    result.ctSources += ctStarts != nullptr && ctStarts[i] ? 1 : 0;
  }

  auto root = [&](int node) {
    while (parent[node] != node) {
      parent[node] = parent[parent[node]];
      node = parent[node];
    }
    return node;
  };

  for (int i = 0; i < nodeCount; ++i) {
    for (int slot = 0; slot < kGraphQualityMaxLinks; ++slot) {
      const int target = linkAt(i, slot);
      if (target == -1) {
        continue;
      }
      if (target < 0 || target >= nodeCount) {
        ++result.invalidLinks;
        continue;
      }
      if (target == i) {
        ++result.selfLinks;
        continue;
      }
      bool repeated = false;
      for (int previous = 0; previous < slot; ++previous) {
        if (linkAt(i, previous) == target) {
          repeated = true;
          break;
        }
      }
      if (repeated) {
        ++result.duplicateLinks;
        continue;
      }
      ++result.edges;
      ++outDegree[i];
      ++inDegree[target];
      int a = root(i);
      int b = root(target);
      if (a != b) {
        if (sizes[a] < sizes[b]) {
          const int other = a;
          a = b;
          b = other;
        }
        parent[b] = a;
        sizes[a] += sizes[b];
      }
      bool reverse = false;
      for (int back = 0; back < kGraphQualityMaxLinks; ++back) {
        if (linkAt(target, back) == i) {
          reverse = true;
          break;
        }
      }
      result.oneWayLinks += reverse ? 0 : 1;
    }
  }

  for (int i = 0; i < nodeCount; ++i) {
    if (root(i) == i) {
      ++result.weakComponents;
      if (sizes[i] > result.largestWeakComponent) {
        result.largestWeakComponent = sizes[i];
      }
    }
    result.isolatedNodes += (inDegree[i] == 0 && outDegree[i] == 0) ? 1 : 0;
  }

  // Directed multi-source BFS measures topology only, not physical movement.
  auto flood = [&](const bool *starts, bool *seen) {
    if (starts == nullptr) {
      return;
    }
    int queue[kGraphQualityMaxNodes] {};
    int next = 0;
    int length = 0;
    for (int i = 0; i < nodeCount; ++i) {
      if (starts[i]) {
        seen[i] = true;
        queue[length++] = i;
      }
    }
    while (next < length) {
      const int node = queue[next++];
      for (int slot = 0; slot < kGraphQualityMaxLinks; ++slot) {
        const int target = linkAt(node, slot);
        if (target >= 0 && target < nodeCount && !seen[target]) {
          seen[target] = true;
          queue[length++] = target;
        }
      }
    }
  };
  bool seenT[kGraphQualityMaxNodes] {};
  bool seenCT[kGraphQualityMaxNodes] {};
  flood(terroristStarts, seenT);
  flood(ctStarts, seenCT);
  for (int i = 0; i < nodeCount; ++i) {
    if (isGoal(i)) {
      result.terroristReachableGoals += seenT[i] ? 1 : 0;
      result.ctReachableGoals += seenCT[i] ? 1 : 0;
    }
    if (isRescue(i)) {
      result.ctReachableRescues += seenCT[i] ? 1 : 0;
    }
  }
  return result;
}

} // namespace ai
