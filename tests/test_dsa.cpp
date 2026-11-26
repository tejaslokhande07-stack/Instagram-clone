// Unit tests for every custom DSA implementation.
// Build: cmake --build build --target ig_dsa_tests && ./build/ig_dsa_tests.exe
#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "dsa/deque.hpp"
#include "dsa/dynamic_vector.hpp"
#include "dsa/graph.hpp"
#include "dsa/hash_set.hpp"
#include "dsa/hash_table.hpp"
#include "dsa/max_heap.hpp"
#include "dsa/queue.hpp"
#include "dsa/trie.hpp"

using namespace ig::dsa;

static int checks = 0;
#define CHECK(expr)                                                       \
  do {                                                                    \
    ++checks;                                                             \
    if (!(expr)) {                                                        \
      std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " -> " << #expr \
                << '\n';                                                  \
      return 1;                                                           \
    }                                                                     \
  } while (0)

static int testHashTable() {
  HashTable<std::string, int> table(4, 0.75f);
  CHECK(table.insert("alice", 1));
  CHECK(table.insert("bob", 2));
  CHECK(!table.insert("alice", 99));  // duplicate rejected
  int value = 0;
  CHECK(table.search("bob", &value) && value == 2);
  CHECK(table.contains("alice"));
  CHECK(!table.contains("nobody"));
  CHECK(table.size() == 2);
  CHECK(table.bucketCount() >= 4);  // at least the initial bucket count
  CHECK(table.set("alice", 42) == false && table.search("alice", &value) && value == 42);
  int removed = 0;
  CHECK(table.remove("bob", &removed) && removed == 2);
  CHECK(!table.remove("bob"));
  CHECK(table.size() == 1);
  table.rehash(64);
  CHECK(table.search("alice", &value) && value == 42);
  std::size_t visited = 0;
  table.forEach([&](const std::string&, int) { ++visited; });
  CHECK(visited == 1);
  table.clear();
  CHECK(table.size() == 0 && !table.contains("alice"));
  return 0;
}

static int testHashSet() {
  HashSet set(2);
  CHECK(set.insert(10));
  CHECK(set.insert(20));
  CHECK(!set.insert(10));
  CHECK(set.contains(20));
  CHECK(set.size() == 2);
  CHECK(set.remove(10));
  CHECK(!set.contains(10));
  CHECK(!set.remove(10));
  CHECK(set.entries().size() == 1);
  return 0;
}

// Phase 3: viewer-scoped like/save caches use string pair keys so that
// (viewer 7, post 100) and (viewer 70, post 10) can never collide.
static int testCompositeKeyCache() {
  HashTable<std::string, bool> likes;
  CHECK(likes.set("7:100", true));
  CHECK(likes.set("7:10", true));
  CHECK(likes.set("70:10", true));
  CHECK(likes.size() == 3);
  CHECK(likes.contains("7:100"));
  CHECK(!likes.set("7:100", true));  // duplicate is not a new key
  CHECK(likes.size() == 3);
  CHECK(likes.remove("7:100"));
  CHECK(!likes.contains("7:100"));
  CHECK(likes.contains("7:10"));
  CHECK(likes.contains("70:10"));
  likes.clear();
  CHECK(likes.size() == 0);
  return 0;
}

static int testGraph() {
  FollowGraph g;
  g.addEdge(1, 2);
  g.addEdge(2, 3);
  g.addEdge(1, 3);
  g.addEdge(4, 3);
  g.addEdge(3, 5);
  CHECK(g.hasEdge(1, 2));
  CHECK(!g.hasEdge(2, 1));  // directed
  CHECK(!g.addEdge(1, 2));  // duplicate edge
  CHECK(g.neighbors(1).size() == 2);
  auto bfs = g.bfs(1, 2);
  CHECK(bfs.front() == 1);
  CHECK(std::find(bfs.begin(), bfs.end(), 5) != bfs.end());
  auto dfs = g.dfs(1);
  CHECK(dfs.size() == 4);  // 4 -> 3 is incoming, unreachable from 1
  auto mutual = g.mutualFollows(1, 4);  // both follow 3
  CHECK(mutual.size() == 1 && mutual[0] == 3);
  auto suggested = g.suggest(1, 5);
  CHECK(!suggested.empty());
  auto followers = g.followersOf(3);
  CHECK(followers.size() == 3);
  CHECK(g.removeEdge(1, 2));
  CHECK(!g.hasEdge(1, 2));
  g.removeVertex(3);
  CHECK(!g.hasEdge(2, 3));

  // Phase 2 semantic follow API and BFS suggestions on a fresh graph.
  FollowGraph sem;
  CHECK(sem.addFollow(10, 11));
  CHECK(sem.addFollow(10, 12));
  CHECK(sem.addFollow(11, 13));
  CHECK(sem.addFollow(12, 14));
  CHECK(!sem.addFollow(10, 12));       // duplicate edge rejected
  CHECK(!sem.addFollow(10, 10));       // self-follow rejected
  CHECK(sem.isFollowing(10, 11));
  CHECK(!sem.isFollowing(11, 10));     // directed edge
  CHECK(sem.getFollowing(10).size() == 2);
  CHECK(std::find(sem.getFollowers(13).begin(), sem.getFollowers(13).end(), 11) !=
        sem.getFollowers(13).end());
  auto suggestions = sem.suggestions(10, 10);
  CHECK(std::find(suggestions.begin(), suggestions.end(), 13) != suggestions.end());
  CHECK(std::find(suggestions.begin(), suggestions.end(), 14) != suggestions.end());
  CHECK(std::find(suggestions.begin(), suggestions.end(), 10) == suggestions.end());
  CHECK(std::find(suggestions.begin(), suggestions.end(), 11) == suggestions.end());
  CHECK(std::find(suggestions.begin(), suggestions.end(), 12) == suggestions.end());
  CHECK(std::count(suggestions.begin(), suggestions.end(), 13) == 1);  // no duplicates
  CHECK(sem.removeFollow(10, 11));
  CHECK(!sem.isFollowing(10, 11));
  return 0;
}

static int testMaxHeap() {
  MaxHeap heap;
  heap.insert(1, 10.0);
  heap.insert(2, 30.0);
  heap.insert(3, 20.0);
  CHECK(heap.peek().id == 2);
  CHECK(heap.pop().id == 2);
  CHECK(heap.pop().id == 3);
  CHECK(heap.pop().id == 1);
  CHECK(heap.empty());
  std::vector<HeapItem> raw{{1, 5}, {2, 9}, {3, 1}, {4, 7}};
  MaxHeap built(raw);
  CHECK(built.peek().id == 2);
  CHECK(built.removeId(2));
  CHECK(built.peek().id == 4);
  CHECK(built.removeAt(0));
  CHECK(built.size() == 2);
  built.heapify();
  std::size_t seen = 0;
  built.forEach([&](const HeapItem&) { ++seen; });
  CHECK(seen == 2);
  return 0;
}

static int testTrie() {
  Trie trie;
  trie.insert("alice");
  trie.insert("alicia");
  trie.insert("bob");
  trie.insert("alice");  // duplicate
  CHECK(trie.size() == 3);
  CHECK(trie.search("alice"));
  CHECK(!trie.search("ali"));
  auto matches = trie.startsWith("ali");
  CHECK(matches.size() == 2);
  CHECK(std::find(matches.begin(), matches.end(), "alice") != matches.end());
  CHECK(trie.startsWith("zz").empty());
  CHECK(trie.startsWith("ALI").size() == 2);
  CHECK(trie.remove("alicia"));
  CHECK(!trie.search("alicia"));
  return 0;
}

static int testQueueAndDeque() {
  IdQueue q(2);
  q.enqueue(1);
  q.enqueue(2);
  q.enqueue(3);  // forces wrap/grow
  CHECK(q.front() == 1 && q.size() == 3);
  CHECK(q.dequeue() == 1);
  CHECK(q.dequeue() == 2);
  CHECK(q.dequeue() == 3);
  CHECK(q.empty());

  IdDeque d(2);
  d.pushBack(2);
  d.pushFront(1);
  d.pushBack(3);
  CHECK(d.front() == 1 && d.back() == 3 && d.size() == 3);
  CHECK(d.at(1) == 2);
  CHECK(d.popFront() == 1);
  CHECK(d.popBack() == 3);
  CHECK(d.popFront() == 2);
  CHECK(d.empty());
  return 0;
}

static int testDynamicVector() {
  DynamicVector<long long> v;
  v.pushBack(1);
  v.pushBack(2);
  v.pushBack(3);
  CHECK(v.size() == 3 && v.capacity() >= 3);
  CHECK(v.at(2) == 3);
  CHECK(v.eraseValue(2));
  CHECK(v.size() == 2 && v.at(1) == 3);
  CHECK(v.eraseAt(0));
  CHECK(v.popBack() == 3 && v.empty());
  DynamicVector<std::string> names{"a", "b"};
  CHECK(names.size() == 2 && names.at(1) == "b");
  return 0;
}

int main() {
  int result = 0;
  result |= testHashTable();
  result |= testHashSet();
  result |= testCompositeKeyCache();
  result |= testGraph();
  result |= testMaxHeap();
  result |= testTrie();
  result |= testQueueAndDeque();
  result |= testDynamicVector();
  if (result == 0) {
    std::cout << "All DSA tests passed (" << checks << " assertions)\n";
  }
  return result;
}
