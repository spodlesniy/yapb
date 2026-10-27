#include <aipb/map/navigation/connectivity.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

namespace aipb::map::navigation {
namespace {

using Neighbors = std::vector<std::vector<std::size_t>>;

struct DirectedGraph {
  Neighbors forward;
  Neighbors reverse;
};

struct Components {
  std::vector<std::size_t> group;
  std::vector<std::size_t> sizes;
  std::size_t largest {};
};

// Iterative Kosaraju avoids call-stack growth on large or linear maps.
Components findComponents(const DirectedGraph &graph) {
  const auto count = graph.forward.size();
  std::vector<std::uint8_t> visited(count, 0);
  std::vector<std::size_t> order;
  order.reserve(count);
  std::vector<std::pair<std::size_t, std::size_t>> frames;

  for (std::size_t root = 0; root < count; ++root) {
    if (visited[root]) {
      continue;
    }
    frames.emplace_back(root, 0);
    visited[root] = 1;

    while (!frames.empty()) {
      auto &[node, offset] = frames.back();

      if (offset < graph.forward[node].size()) {
        const auto next = graph.forward[node][offset++];

        if (!visited[next]) {
          visited[next] = 1;
          frames.emplace_back(next, 0);
        }

        continue;
      }
      order.push_back(node);
      frames.pop_back();
    }
  }

  Components result;
  result.group.assign(count, count);
  std::vector<std::size_t> pending;

  for (auto it = order.rbegin(); it != order.rend(); ++it) {
    if (result.group[*it] != count) {
      continue;
    }
    const auto component = result.sizes.size();
    std::size_t size = 0;
    result.group[*it] = component;
    pending.push_back(*it);

    while (!pending.empty()) {
      const auto node = pending.back();
      pending.pop_back();
      ++size;

      for (const auto next : graph.reverse[node]) {
        if (result.group[next] == count) {
          result.group[next] = component;
          pending.push_back(next);
        }
      }
    }
    result.sizes.push_back(size);
    result.largest = std::max(result.largest, size);
  }

  return result;
}

std::size_t reachableCount(
  const Neighbors &graph,
  const std::vector<std::size_t> &anchors
) {
  std::vector<std::uint8_t> visited(graph.size(), 0);
  std::vector<std::size_t> pending;
  pending.reserve(graph.size());
  std::size_t count = 0;

  for (const auto anchor : anchors) {
    if (!visited[anchor]) {
      visited[anchor] = 1;
      pending.push_back(anchor);
    }
  }

  while (!pending.empty()) {
    const auto node = pending.back();
    pending.pop_back();
    ++count;

    for (const auto next : graph[node]) {
      if (!visited[next]) {
        visited[next] = 1;
        pending.push_back(next);
      }
    }
  }

  return count;
}

} // namespace

void analyzeDirectedConnectivity(
  const CandidateReport &candidates,
  ConnectivityReport &report
) {
  const auto count = candidates.candidates.size();
  std::map<std::size_t, std::size_t> indices;

  for (std::size_t i = 0; i < count; ++i) {
    if (candidates.candidates[i].id == 0 ||
        !indices.emplace(candidates.candidates[i].id, i).second) {
      throw std::invalid_argument("duplicate or invalid graph candidate ID");
    }
  }

  DirectedGraph all {
    .forward = Neighbors(count),
    .reverse = Neighbors(count)
  };
  DirectedGraph unconditional {
    .forward = Neighbors(count),
    .reverse = Neighbors(count)
  };

  for (const auto &edge : report.edges) {
    const auto from = indices.find(edge.from);
    const auto to = indices.find(edge.to);

    if (from == indices.end() || to == indices.end()) {
      throw std::invalid_argument("graph edge references unknown candidate");
    }
    all.forward[from->second].push_back(to->second);
    all.reverse[to->second].push_back(from->second);

    if (!edge.conditionalSupport) {
      unconditional.forward[from->second].push_back(to->second);
      unconditional.reverse[to->second].push_back(from->second);
    }
  }

  const auto components = findComponents(all);
  const auto fixedComponents = findComponents(unconditional);
  report.strongComponents = components.sizes.size();
  report.largestStrongComponent = components.largest;
  report.unconditionalStrongComponents = fixedComponents.sizes.size();
  report.oneWaySinkComponents = 0;
  report.oneWaySinkCandidates = 0;
  std::vector<std::uint8_t> incoming(components.sizes.size(), 0);
  std::vector<std::uint8_t> outgoing(components.sizes.size(), 0);

  for (std::size_t from = 0; from < count; ++from) {
    for (const auto to : all.forward[from]) {
      const auto a = components.group[from];
      const auto b = components.group[to];

      if (a != b) {
        outgoing[a] = 1;
        incoming[b] = 1;
      }
    }
  }

  for (std::size_t i = 0; i < components.sizes.size(); ++i) {
    // Isolated components are not one-way sinks without an incoming path.
    if (incoming[i] && !outgoing[i]) {
      ++report.oneWaySinkComponents;
      report.oneWaySinkCandidates += components.sizes[i];
    }
  }

  std::vector<std::size_t> anchors;

  for (const auto &spawn : candidates.spawns) {
    if (!spawn.coveredCandidateId) {
      continue;
    }
    const auto it = indices.find(*spawn.coveredCandidateId);

    if (it == indices.end()) {
      throw std::invalid_argument("spawn anchor references unknown candidate");
    }
    anchors.push_back(it->second);
  }
  std::sort(anchors.begin(), anchors.end());
  anchors.erase(std::unique(anchors.begin(), anchors.end()), anchors.end());
  report.spawnAnchors = anchors.size();
  report.candidatesReachableFromSpawns = reachableCount(all.forward, anchors);
  report.candidatesReachableUnconditionally = reachableCount(unconditional.forward, anchors);
  report.candidatesUnreachableFromSpawns = count - report.candidatesReachableFromSpawns;
}

} // namespace aipb::map::navigation
