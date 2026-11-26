#pragma once
// Directed graph using adjacency lists, keyed by user ID.
// An edge A -> B means "A follows B".
// BFS: shortest connection paths / mutual followers / suggestions.
// DFS: full traversal demonstration and reachability checks.
#include <cstddef>
#include <vector>

#include "dsa/hash_table.hpp"
#include "dsa/hash_set.hpp"

namespace ig::dsa {

class FollowGraph {
 public:
  void addVertex(long long id);
  void removeVertex(long long id);
  bool hasVertex(long long id) const { return vertices_.contains(id); }

  // Returns false when the edge already existed (duplicate prevention).
  bool addEdge(long long from, long long to);
  bool removeEdge(long long from, long long to);
  bool hasEdge(long long from, long long to) const;
  std::vector<long long> neighbors(long long id) const;
  std::size_t vertexCount() const { return vertices_.size(); }

  // Breadth-first traversal up to maxDepth hops; includes `start`.
  std::vector<long long> bfs(long long start, std::size_t maxDepth = 2) const;
  // Depth-first traversal (iterative to avoid stack overflow).
  std::vector<long long> dfs(long long start) const;
  // Users followed by both a and b (intersection of adjacency sets).
  std::vector<long long> mutualFollows(long long a, long long b) const;
  // "People you may know": friends-of-friends not already followed.
  // BFS depth 2 visits at most V vertices and E adjacency entries, so
  // suggestions run in O(V + E) time with O(V) auxiliary space.
  std::vector<long long> suggest(long long user, std::size_t limit = 10) const;
  // Reverses all edges (followers of a user).
  std::vector<long long> followersOf(long long user) const;

  // Phase 2 semantic follow API. These wrappers keep the graph vocabulary
  // aligned with the backend service while reusing the adjacency-list core.
  bool addFollow(long long follower, long long following);
  bool removeFollow(long long follower, long long following);
  bool isFollowing(long long follower, long long following) const;
  std::vector<long long> getFollowers(long long user) const;
  std::vector<long long> getFollowing(long long user) const;
  std::vector<long long> suggestions(long long user, std::size_t limit = 10) const;

 private:
  HashSet vertices_;
  HashTable<long long, std::vector<long long>> adjacency_;
};

}  // namespace ig::dsa
