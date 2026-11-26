#include "dsa/graph.hpp"

#include <algorithm>
#include <queue>

namespace ig::dsa {

void FollowGraph::addVertex(long long id) {
  vertices_.insert(id);
  // Never overwrite an existing adjacency list: edges are accumulated.
  if (!adjacency_.contains(id)) {
    std::vector<long long> empty;
    adjacency_.set(id, empty);
  }
}

void FollowGraph::removeVertex(long long id) {
  vertices_.remove(id);
  adjacency_.remove(id);
  // Remove every incoming edge so no dangling references remain.
  vertices_.forEach([&](long long other) {
    std::vector<long long> list;
    if (adjacency_.search(other, &list)) {
      std::vector<long long> filtered;
      for (long long v : list) {
        if (v != id) filtered.push_back(v);
      }
      adjacency_.set(other, filtered);
    }
  });
}

bool FollowGraph::addEdge(long long from, long long to) {
  if (from == to) return false;  // a user can never follow themselves
  addVertex(from);
  addVertex(to);
  std::vector<long long> list;
  adjacency_.search(from, &list);
  if (std::find(list.begin(), list.end(), to) != list.end()) return false;
  list.push_back(to);
  adjacency_.set(from, list);
  return true;
}

bool FollowGraph::removeEdge(long long from, long long to) {
  std::vector<long long> list;
  if (!adjacency_.search(from, &list)) return false;
  auto it = std::find(list.begin(), list.end(), to);
  if (it == list.end()) return false;
  list.erase(it);
  adjacency_.set(from, list);
  return true;
}

bool FollowGraph::hasEdge(long long from, long long to) const {
  std::vector<long long> list;
  if (!adjacency_.search(from, &list)) return false;
  return std::find(list.begin(), list.end(), to) != list.end();
}

std::vector<long long> FollowGraph::neighbors(long long id) const {
  std::vector<long long> list;
  adjacency_.search(id, &list);
  return list;
}

std::vector<long long> FollowGraph::bfs(long long start,
                                        std::size_t maxDepth) const {
  std::vector<long long> order;
  if (!vertices_.contains(start)) return order;
  std::queue<std::pair<long long, std::size_t>> q;
  HashSet visited;
  visited.insert(start);
  q.push({start, 0});
  while (!q.empty()) {
    auto [node, depth] = q.front();
    q.pop();
    order.push_back(node);
    if (depth >= maxDepth) continue;
    for (long long next : neighbors(node)) {
      if (visited.insert(next)) q.push({next, depth + 1});
    }
  }
  return order;
}

std::vector<long long> FollowGraph::dfs(long long start) const {
  std::vector<long long> order;
  if (!vertices_.contains(start)) return order;
  HashSet visited;
  std::vector<std::pair<long long, std::size_t>> stack;
  stack.push_back({start, 0});
  while (!stack.empty()) {
    auto [node, nextIndex] = stack.back();
    stack.pop_back();
    if (visited.contains(node)) continue;  // already emitted
    visited.insert(node);
    order.push_back(node);
    auto ns = neighbors(node);
    if (nextIndex < ns.size()) {
      stack.push_back({node, nextIndex + 1});
      stack.push_back({ns[nextIndex], 0});
    }
  }
  return order;
}

std::vector<long long> FollowGraph::mutualFollows(long long a,
                                                  long long b) const {
  HashSet setA;
  for (long long v : neighbors(a)) setA.insert(v);
  std::vector<long long> result;
  for (long long v : neighbors(b)) {
    if (setA.contains(v)) result.push_back(v);
  }
  return result;
}

std::vector<long long> FollowGraph::suggest(long long user,
                                            std::size_t limit) const {
  // BFS depth 2 finds candidates followed by people the user follows.
  std::vector<long long> path = bfs(user, 2);
  HashSet seen;
  seen.insert(user);
  for (long long v : neighbors(user)) seen.insert(v);
  std::vector<long long> result;
  for (long long candidate : path) {
    if (candidate == user) continue;
    if (seen.insert(candidate) && result.size() < limit) {
      result.push_back(candidate);
    }
  }
  return result;
}

std::vector<long long> FollowGraph::followersOf(long long user) const {
  std::vector<long long> result;
  vertices_.forEach([&](long long v) {
    if (hasEdge(v, user)) result.push_back(v);
  });
  return result;
}

bool FollowGraph::addFollow(long long follower, long long following) {
  if (follower == following) return false;
  return addEdge(follower, following);
}

bool FollowGraph::removeFollow(long long follower, long long following) {
  if (follower == following) return false;
  return removeEdge(follower, following);
}

bool FollowGraph::isFollowing(long long follower, long long following) const {
  return hasEdge(follower, following);
}

std::vector<long long> FollowGraph::getFollowers(long long user) const {
  return followersOf(user);
}

std::vector<long long> FollowGraph::getFollowing(long long user) const {
  return neighbors(user);
}

// Two-hop BFS suggestions: start from `user`, walk only outgoing follow edges,
// then exclude the user, everyone already followed, and duplicates via HashSet.
// Blocked/private policy is applied by the service layer, which owns user data.
std::vector<long long> FollowGraph::suggestions(long long user,
                                                std::size_t limit) const {
  return suggest(user, limit);
}

}  // namespace ig::dsa
