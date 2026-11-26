# Custom Data Structures — Design, Complexity and Integration

All structures live in `backend/include/dsa/` with implementations in
`backend/src/dsa/`. They are compiled into the `ig_dsa` library and verified by
`tests/test_dsa.cpp` (**105 assertions**, all passing).

The structures are not decorative: `AppContext` in
`backend/include/services/app_service.h` owns live instances, `rebuildIndexes()`
fills them from MySQL at startup, and service methods update them after
successful writes. MySQL remains the source of truth; the DSA layer is a
rebuildable application-level index/cache.

---

## 1. Summary table

| Structure | Insert | Search | Delete | Traversal | Extra operations |
|---|---:|---:|---:|---:|---|
| `HashTable<K,V>` | O(1) avg, O(n) worst | O(1) avg, O(n) worst | O(1) avg, O(n) worst | O(n + b) | `set`, `rehash`, load factor |
| `HashSet` | O(1) avg | O(1) avg | O(1) avg | O(n + b) | `entries`, `forEach` |
| `FollowGraph` | O(1) avg edge | O(1) avg edge | O(1) avg edge | O(V + E) | BFS/DFS O(V + E), mutual/suggest O(deg) |
| `MaxHeap` | O(log n) | peek O(1) | pop O(log n), by id O(n + log n) | O(n) array order | heapify O(n), `removeAt` O(log n) |
| `Trie` | O(L) | O(L) | O(L) | O(L + matches) | `startsWith` prefix autocomplete for usernames and hashtags |
| `IdQueue` | enqueue O(1) amortised | front O(1) | dequeue O(1) amortised | O(n) | circular buffer growth |
| `IdDeque` | push front/back O(1) amortised | `at` O(1) | pop front/back O(1) amortised | O(n) | `eraseValue` O(n) |
| `DynamicVector<T>` | push back O(1) amortised | `at` O(1) | `eraseAt` O(n) | O(n) | reserve, move semantics |

`L` = word length, `b` = bucket count, `V`/`E` = graph vertices/edges.

---

## 2. HashTable — separate chaining

**File:** `hash_table.hpp` / `hash_table.cpp`

- Bucket array of `Node*` chains with grow-on-load-factor (`maxLoad = 0.75`).
- `insert` rejects duplicates; `set` overwrites; `remove` unlinks a node;
  `search` returns the value through an optional out-parameter.
- `forEach` visits every key/value pair in bucket order.
- Explicit instantiations cover the pairs used by the service layer, e.g.
  `HashTable<std::string, long long>`,
  `HashTable<long long, std::vector<long long>>`, and the Phase 3 cache type
  `HashTable<std::string, bool>`.

**Used for:**
- `AppContext::usernameIndex` — username → user id.
- `AppContext::idIndex` — user id → username.
- `FollowGraph::adjacency_` — user id → outgoing follow IDs.
- `AppContext::likedPosts`, `savedPosts`, `unreadConversations` — collision-safe
  string keys produced by `interactionKey(first, second)`, e.g. `15:42`.

Why it matters: username resolution, follow-neighbour lookup, and "did this
viewer like/save this?" checks avoid repeated database scans during request
handling. Phase 8 made the like/save lookup explicit in `hydratePost()`:
`search(interactionKey(viewer, postId))` supplies `likedByViewer` and
`savedByViewer` from the cache populated at startup and after writes. Phase 6
also counts unread conversations by traversing keys with the viewer prefix
(`viewer:conversation`) after rebuilding the cache from MySQL; typing rows are
excluded. The authoritative row still lives in MySQL; these are rebuildable
caches.

---

## 3. HashSet — unique membership

**File:** `hash_set.hpp` / `hash_set.cpp`

- Separate-chaining set of `long long` with automatic growth.
- `insert` returns false for duplicates; `contains` and `remove` are average
  O(1); `entries`/`forEach` provide traversal.
- `FollowGraph::vertices_` uses it to enforce unique vertices.

**Used for:**
- BFS `visited` sets in the follow graph.
- `AppContext::activeStories` — O(1) membership check while pruning expired
  stories from the ordered story cache (Phase 5).

Why it matters: traversal needs unique vertex membership. Phase 3 viewer-scoped
membership uses a string-keyed `HashTable` instead (see §2) because composite
keys such as `user:post` avoid integer-sum collisions.

---

## 4. FollowGraph — directed adjacency lists

**File:** `graph.hpp` / `graph.cpp`

- Edge `A -> B` means "A follows B". Vertices are `HashSet`; adjacency lists are
  `HashTable<long long, std::vector<long long>>`.
- `addEdge`/`removeEdge` reject duplicates / missing edges; self-edges are rejected.
- Semantic wrappers: `addFollow`, `removeFollow`, `isFollowing`, `getFollowers`,
  `getFollowing`, and `suggestions`.
- `bfs(start, maxDepth)` — shortest-radius traversal, used for suggestions.
- `dfs(start)` — iterative depth-first traversal (no recursion stack risk).
- `mutualFollows(a,b)` — intersection of adjacency sets.
- `suggest(user, limit)` / `suggestions(user, limit)` — friends-of-friends not
  already followed. A 2-hop BFS visits at most `V` vertices and `E` adjacency
  entries, so suggestions are `O(V + E)` time with `O(V)` auxiliary space.
- `followersOf(user)` / `getFollowers(user)` — reverse traversal over all edges.

**Used for:** follow/unfollow endpoints, `/users/:id/follow-status`,
`/users/:id/suggestions`, `/users/:id/mutual`, follower/following list hydration.
MySQL remains the permanent source of truth; this graph is rebuilt on startup
and updated after successful writes.

Complexity: edge operations O(1) average; BFS/DFS/reverse O(V + E); mutual is
O(min(deg(a), deg(b))) with a hash set for membership.

---

## 5. MaxHeap — ranked feeds and reels

**File:** `max_heap.hpp` / `max_heap.cpp`

- Binary heap stored in `std::vector<HeapItem>` with `siftUp`/`siftDown`.
- `insert` O(log n), `peek` O(1), `pop` O(log n), `removeAt` O(log n),
  `removeId` O(n) search + O(log n) fix, `heapify` O(n).
- Ties are ordered by larger id first for deterministic ranking.
- `forEach` traverses array order; repeated `pop` yields sorted output.

**Used for:**
- `AppContext::feedHeap` — scores posts by recency, likes, comments and social
  proximity, then drains the ranked IDs for `/api/feed` pagination.
- `AppContext::trendingReels` — scores reels by views/likes/comments/recency for
  `/reels?mode=trending`.
- `AppContext::trendingHashtags` — scores active hashtags for
  `/api/hashtags/trending` (Phase 7).

Phase 8 made all three heaps genuine read-path structures: requests clear and
insert into the `AppContext` heap under `dsaMutex`, then drain it to produce the
response. They are also rebuilt from MySQL at startup, so the cache exists before
the first request.

Why it matters: ranking is performed by our own heap rather than relying on
`ORDER BY` alone; MySQL supplies candidate rows, C++ supplies the ordering.
Phase 3 filters blocked/private posts in the MySQL candidate query before the
heap is built, so the ranked feed and explore results cannot leak unavailable
posts.

**Phase 4 / Phase 8 feed complexity.** For `mode=popular|following`, each of `n`
privacy-filtered candidates is hydrated once, scored in O(1), inserted into
`ctx_.feedHeap` with `insert` O(log n), then drained with `pop` O(log n):
**O(n log n)** ranking after the indexed SQL scan. A hash map from post id to
hydrated candidate makes the final page assembly O(n) instead of O(n²). For `mode=recent`, the heap is
intentionally skipped: rows already arrive in `created_at DESC` order, so the
path is O(n) hydration plus SQL pagination. The UI exposes both ranked
("For you"/"Following") and chronological ("Recent") modes plus refresh,
pagination, and explicit error/retry states.

**Phase 5 / Phase 8 reel complexity.** `getReels(mode=trending)` hydrates at most
200 candidate rows, computes each score in O(1), inserts into `ctx_.trendingReels`
with O(log n), drains with O(log n) per pop, and assembles the page through an
id→reel hash map in O(n) instead of repeatedly scanning the vector. The
result is O(n log n) ranking plus O(n) hydration, with MySQL supplying the
block/privacy-filtered candidate set. `mode=latest` skips the heap and uses SQL
`created_at DESC` order. The `ctx_.trendingReels` heap is rebuilt at startup and
repopulated on each trending request; Phase 8 changed it from a write-only mirror
into the structure that actually supplies ranked output.

**Phase 7 / Phase 8 hashtag complexity.** `trendingHashtags()` hydrates active tags
with their total usage and posts from the last seven days, computes each score in
O(1), inserts every id into `ctx_.trendingHashtags` in O(log n), and drains the top
12 with O(log n) per pop: **O(n log n)** ranking. Phase 8 removed the old
local-heap-then-copy behavior; the `AppContext` heap itself is now drained for the
response and rebuilt from `hashtags.usage_count` at startup, so it is recoverable
after a restart even though the score is recalculated per request.

---

## 6. Trie — username and hashtag autocomplete

**File:** `trie.hpp` / `trie.cpp`

- Array of 38 child indices per node (lowercase letters, digits, `.`, `_`).
- `insert` O(L), `search` O(L), `remove` O(L), `startsWith(prefix, limit)`
  O(L + matches) via bounded DFS `collect`.
- Invalid characters are ignored, matching the username validation rules.

**Used for:**
- `AppContext::usernameTrie` — `/users/search?q=...` prefix matching and the
  frontend search page's people results.
- `AppContext::hashtagTrie` — Phase 7 hashtag suggestions in
  `/api/search?q=...`. New tags are inserted by `syncHashtags()` after a
  successful post write; tags whose usage drops to zero are removed by
  `replaceHashtags()` on edit/delete. Both tries are rebuilt from MySQL at
  startup, and `startsWith(query, 20)` returns bounded candidates that MySQL
  hydrates into the final top 10.

The Trie is the prefix-index step: O(L) to reach the query prefix and O(L +
matches) to collect candidates. SQL then verifies live usage and visibility, so
stale candidates cannot become API results.

---

## 7. Queue — notification events

**File:** `queue.hpp` / `queue.cpp`

- Circular buffer with growable capacity; `enqueue`, `dequeue`, `front` are O(1)
  amortised; `forEach` visits oldest-to-newest.
- `notify()` in `AppService` persists a `notifications` row and then enqueues
  its id in FIFO order.
- `rebuildNotificationQueue()` (Phase 6) clears the queue and enqueues the newest
  100 unread notification ids from MySQL, while `rebuildUnreadConversations()`
  refreshes `AppContext::unreadConversations` from unread, non-typing messages.
  Both rebuilds run at startup and after read transitions.

Why it matters: the queue is no longer write-only. `GET /api/notifications/unread`
and `GET /api/messages/unread` traverse these in-memory structures to answer
count queries in O(n) without scanning all rows per request; MySQL remains the
source of truth and the caches are rebuilt after restarts or external changes.

---

## 8. Deque — recent activity

**File:** `deque.hpp` / `deque.cpp`

- Circular buffer supporting `pushFront`, `pushBack`, `popFront`, `popBack`,
  `front`, `back`, `at`, `eraseValue`, `forEach`.
- Both-end operations are O(1) amortised; random access O(1); `eraseValue` O(n).

**Used for:**
- `AppContext::recentActivity` — touched post/reel IDs (reel views push to the
  front).
- `AppContext::storyOrder` — active story IDs ordered oldest → newest. Story
  creation appends to the back; `pruneExpiredStories()` removes expired IDs with
  `eraseValue` (O(n)) after an O(1) `HashSet` membership check. `getStories()`
  snapshots the order with `forEach` and hydrates rows from MySQL, so a restart
  rebuilds the same ordering from `created_at ASC, id ASC`.

---

## 9. DynamicVector — generic dynamic array

**File:** `dynamic_vector.hpp` / `dynamic_vector.cpp`

- Raw storage with capacity doubling, `pushBack` O(1) amortised, `at` O(1),
  `eraseAt`/`eraseValue` O(n), and full copy/move semantics.
- Explicit instantiations for `long long` and `std::string`.

**Used for:** service-layer collections of IDs, comments and media before they
are converted to JSON, plus adjacency-list storage inside `FollowGraph`. In
Phase 5, `getStories()` snapshots active story IDs into a `DynamicVector<long long>`
before hydrating them in chronological order without allocating a second
`std::vector`.

---

## 10. Testing

`tests/test_dsa.cpp` checks every structure for:

- insertion and duplicate rejection;
- exact search / membership;
- deletion and post-delete state;
- traversal order and visited counts;
- edge cases (empty structures, growth, rehash, tie-breaking);
- graph BFS/DFS depth, mutual follows and suggestions;
- heap heapify/pop ordering;
- Trie prefix limits and removal;
- Phase 3 composite-key cache set/remove/rebuild behavior
  (`testCompositeKeyCache`).

Run:

```powershell
$env:PATH = "C:\ig-mingw-real\bin;I:\build;$env:PATH"
I:\build\ig_dsa_tests.exe
```

Expected output:

```
All DSA tests passed (105 assertions)
```

The API suite (`tests/test_api.py`, 57 requests) exercises the same structures
through real endpoints: follow/suggest, feed ranking, search autocomplete,
likes/saves and notification flow. The focused Phase 3 harness additionally
verifies string-keyed like/save caches across a backend restart; see
`docs/PHASE3_REPORT.md`. The Phase 7 harness verifies the hashtag Trie and
trending `MaxHeap` after a backend restart, including tag removal on edit and
normalized `#tag` search/explore behavior. The Phase 8 harness verifies all of
those caches on the read path: 66/66 preparation checks, 14/14 verification
checks, 15/15 restart-persistence checks, and 26/26 cleanup checks, including
feed/hashtag/reel heaps, HashTable-backed viewer flags, notification and unread
conversation caches, private reel visibility, and post-delete media cleanup.
