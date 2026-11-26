# Development Progress

## Phase 1 — Completed

React frontend, Crow backend, MySQL connectivity, registration/login/logout,
Argon2id password hashing, profile read/update, custom Hash Table user lookup,
frontend/backend API wiring, and focused Phase 1 tests were verified.

## Phase 2 — Follow/Unfollow System with C++ Graph DSA — Completed

### Implemented

- [x] Inspected existing frontend, backend, schema, DSA implementations, routes and components.
- [x] Extended `FollowGraph` with semantic APIs: `addFollow`, `removeFollow`,
      `isFollowing`, `getFollowers`, `getFollowing`, and BFS `suggestions`.
- [x] Prevented self-follow in the graph API, backend API, and MySQL schema.
- [x] Kept the unique `(follower_id, following_id)` key; MySQL remains the source of truth.
- [x] Synchronized graph writes with MySQL and avoided duplicate follow notifications.
- [x] Added `GET /api/users/:id/follow-status` and `GET /api/users/:id/suggestions`.
- [x] Kept all follow/unfollow/followers/following endpoints authenticated and validated.
- [x] Updated profile UI: Follow/Unfollow/Cancel-request states, immediate counts,
      followers/following lists, and no Follow button on the current user's profile.
- [x] Added loading/error/retry states to suggested accounts and the user-list modal.
- [x] Added C++ unit tests for semantic graph operations and BFS suggestions.
- [x] Ran focused Phase 2 API tests including restart persistence and BFS exclusions.
- [x] Verified the UI in the browser with no console errors or failed requests.

### Test results

- DSA tests: 93 assertions passed (was 75 before Phase 2 additions).
- Focused Phase 2 API checks before restart: 24/24 passed.
- Focused Phase 2 API checks after restart: 11/11 passed.
- Cleanup check: 1/1 passed.
- Frontend production build: passed.
- Browser smoke test: follow → refresh persistence → followers list → unfollow passed.
- Temporary users removed; demo user count restored to 5.

### Notes

MySQL remains the permanent source of truth. `FollowGraph` is an in-memory
adjacency-list index rebuilt from `followers` on startup and updated after
successful database writes. BFS suggestion depth is 2; time complexity is
`O(V + E)` where `V` is users and `E` is follow edges. Private accounts are
supported through the existing follow-request flow, so they may be suggested.

## Phase 3 — Posts, Likes and Comments — Completed

### Implemented

- [x] Hardened post create/update/delete validation: 1–10 media items, caption
      ≤ 5000 chars, location ≤ 150 chars, author-only edits/deletes, and media
      rollback when no valid upload can be saved.
- [x] Kept MySQL authoritative for `posts`, `post_media`, `likes`, `comments`,
      `saved_posts`, `hashtags`, `post_hashtags`, and like/comment notifications.
- [x] Added `Database::executeCount()` so duplicate like/save writes are detected
      from affected-row counts instead of the auto-increment ID.
- [x] Replaced numeric-sum interaction cache keys with collision-safe string keys
      (`viewer:post`, `viewer:conversation`) in a string-keyed `HashTable`.
- [x] Made like/unlike and save/unsave idempotent; duplicate likes no longer emit
      duplicate notifications, and all mutation responses return fresh counts.
- [x] Added post/visibility validation to comments; comment bodies are trimmed,
      limited to 1000 chars, and list/add/delete responses include `commentCount`.
- [x] Applied privacy/block filtering to feed, explore, saved posts, and all
      post/comment/like/save reads; private posts require an accepted follow.
- [x] Deduplicated hashtags per caption so `usage_count` counts posts rather than
      repeated occurrences; edits resync hashtags and deletes clear usage.
- [x] Rebuilt like/save/unread caches from MySQL on startup and updated them only
      after successful writes; restart persistence was verified.
- [x] Fixed a latent follow-request bug (`SELECT id` on a table with no `id`
      column) that made every accept/reject request return 404.
- [x] Added C++ DSA tests for composite-key cache behavior.
- [x] Updated `PostCard` with like/save busy guards, server-authoritative count
      reconciliation, comment loading/error/retry states, and comment length cap.
- [x] Updated `CreatePostModal` with file/type/count validation, caption counter,
      object-URL cleanup, and post-create limits.
- [x] Made direct post detail navigate home after deleting the displayed post.
- [x] Ran focused API verification (prepare, restart, verify, cleanup), a browser
      smoke test, the C++ DSA suite, and a production frontend build.

### Test results

- DSA tests: 105 assertions passed (was 93 after Phase 2).
- Focused Phase 3 API checks before restart: 60/60 passed.
- Focused Phase 3 API checks after restart: 16/16 passed.
- Cleanup check: 5/5 passed.
- Frontend production build: passed (`1608` modules transformed).
- Browser smoke: create → like/unlike → comment error/retry/add → explore →
  `/post/:id` → delete passed; no console errors or failed requests.
  (Only pre-existing React Router v7 future-flag warnings remain.)
- Demo state restored: `users=5`, `temp_users=0`, `posts=4`, `comments=5`,
  `likes=9`, `saved=3`, `notifications=6`; smoke upload and hashtag removed.

### Notes

MySQL remains the permanent source of truth. The DSA layer is a rebuildable cache:
string-keyed `HashTable` membership for likes/saves/unread conversations,
`IdDeque` for recent posts, `IdQueue` for notifications, and `MaxHeap` for feed
ranking. Candidate rows are filtered for privacy in MySQL before C++ ranking.
See `docs/API.md`, `docs/DSA.md`, and `docs/PHASE3_REPORT.md` for details.

### Phase 3 completion re-check before Phase 4

- Backend health returned `instagram-clone-cpp:ok`.
- Login as `alice` succeeded; read-only feed, post, comments, explore, saved,
  and trending-hashtag requests all returned `200` with hydrated data.
- DSA suite re-run: `All DSA tests passed (105 assertions)`.
- Demo DB baseline unchanged: `users=5`, `posts=4`, `comments=5`, `likes=9`,
  `saved=3`, `notifications=6`, no temporary Phase 3 users/tags/uploads.
- Conclusion: Phase 3 is complete and working; Phase 4 may proceed.

## Phase 4 — Home Feed and Feed Ranking — Completed

### Modified files

- `backend/src/services/posts.cpp`
- `frontend/src/pages/Home.tsx`
- `docs/API.md`
- `docs/DSA.md`
- `PROGRESS.md`

### Completed functionality

- [x] Kept `mode=following` limited to the viewer and followed accounts.
- [x] Kept `mode=popular` ranked by the custom `MaxHeap` using recency, likes,
      comments, follow relationship, and recent-activity boosts.
- [x] Added `mode=recent` / `mode=chronological`, which returns posts in
      `created_at DESC` order and bypasses ranking by design.
- [x] Feed responses now include `page`, `mode`, `order`, and `hasMore`.
- [x] Replaced the ranked page-assembly linear search with an id→candidate hash
      map (O(n) instead of O(n²) after heap ordering).
- [x] Added a Recent tab, explicit feed error banner with Try again, and a
      Refresh feed button in the Home UI.
- [x] Preserved the existing Load more pagination, loading spinner, and toast.

### DSA use

The custom `MaxHeap` remains the actual ranking engine for `popular` and
`following` feeds: n inserts and n pops cost O(n log n). The chronological mode
uses the SQL ordering (O(n) hydration). Complexity is documented in
`docs/DSA.md`.

### Test results

- Backend rebuild: passed after stopping the running server.
- Frontend production build: passed (`1608` modules transformed).
- Focused Phase 4 API preparation: 8/8 passed.
- Focused Phase 4 API verification: 18/18 passed, covering chronological vs
  ranked order, engagement-first ranking, followed-account filtering,
  pagination with no duplicates, auth, and unknown-mode fallback.
- Cleanup: 4/4 passed; demo baseline restored.
- Browser smoke: Recent → For you → Following → Refresh all issued the expected
  `/api/feed` requests; simulated failure showed the error banner, and Try again
  recovered. No console errors or failed requests.
- Demo state restored: `users=5`, `posts=4`, `comments=5`, `likes=9`,
  `saved=3`, `notifications=6`.

### Remaining issues

- No functional Phase 4 blockers are known.
- Ranking weights remain constants in `posts.cpp` for easy tuning; exposing them
  as configuration is a future enhancement.
- Media-file garbage collection on post delete remains a Phase 3 carry-over.

## Phase 5 — Stories and Reels — Completed

### Modified files

- `backend/include/services/app_service.h`
- `backend/src/services/app_context.cpp`
- `backend/src/services/stories_reels.cpp`
- `frontend/src/components/Stories.tsx`
- `frontend/src/pages/Reels.tsx`
- `docs/API.md`
- `docs/DSA.md`
- `PROGRESS.md`

### Completed functionality

- [x] Audited the existing story/reel routes, service logic, DB tables, and UI
      before making changes.
- [x] Added `IdDeque storyOrder` + `HashSet activeStories` to `AppContext`; active
      story IDs are rebuilt from MySQL at startup, appended on create, and pruned
      when `expires_at` passes.
- [x] Used `DynamicVector<long long>` to snapshot story order before hydration,
      so story ordering now comes from the custom DSA layer instead of relying
      only on an unordered map iteration.
- [x] Fixed story views, reel views, and reel likes to use `Database::executeCount()`
      for affected-row detection. First views/likes increment and notify exactly
      once; repeat views/likes no longer create duplicate notifications or counts.
- [x] Kept story visibility limited to followed accounts, excluding blocked
      relationships; a own-story viewer sees their stories only under `mine`.
- [x] Kept reel trending ranking in the custom `MaxHeap` and replaced the ranked
      assembly's repeated linear scan with an id→reel hash map (O(n) page build).
- [x] Added a Saved tab to the Reels UI backed by `GET /api/reels/saved`; unsaving
      from that tab removes the reel immediately.
- [x] Wired story viewing in the UI to `POST /api/stories/<id>/view` in addition
      to the local viewed-state update.
- [x] Updated API and DSA docs with Phase 5 behavior and complexity.

### DSA use

Stories use `HashSet` for O(1) active-story membership, `IdDeque` for chronological
ordering/pruning, and `DynamicVector` for an ordered snapshot. Trending reels use
the custom `MaxHeap` with O(n log n) ranking and an O(n) hash-map page assembly.
All caches are rebuilt from MySQL on startup. Details are in `docs/DSA.md`.

### Test results

- Backend rebuild: passed.
- C++ DSA suite: `All DSA tests passed (105 assertions)`.
- Frontend production build: passed (`1608` modules transformed).
- Focused Phase 5 API preparation: 5/5 passed.
- Focused Phase 5 API verification: 37/37 passed, covering story visibility and
  chronological order, idempotent views, expiry pruning, latest vs max-heap
  trending order, reel view/like/comment/save persistence, saved-reels listing,
  auth, and invalid-media validation.
- Cleanup: 10/10 passed; all 9 tracked tables and uploaded files restored.
- Restart persistence snapshot: identical story/reel state before and after a
  backend restart (stories, own stories, latest reels, trending reels).
- Browser smoke: story viewer opened and issued `POST /api/stories/3/view`; Reels
  Latest → Trending → Saved issued the expected API requests; no console errors.
- Demo state restored: `users=5`, `stories=3`, `story_views=3`, `reels=2`,
  `reel_views=7`, `reel_likes=5`, `reel_comments=3`, `reel_saves=2`,
  `notifications=6`.

### Remaining issues

- The demo `demo-reel.mp4` media request was marked failed in the browser network
  log after navigating away from Reels; the file exists and is served correctly,
  and this appears to be a canceled media load rather than a functional error.
- `notificationQueue` is still write-only; notifications are read from MySQL.
  This is a Phase 6 candidate, not a Phase 5 blocker. **Resolved in Phase 6:**
  the queue is rebuilt and traversed for unread counts.
- Story/reel media files are not garbage-collected if rows are deleted directly
  in MySQL; normal API flows create and clean up their own files.

## Phase 6 — Direct Messaging and Notifications — Completed

### Modified files

- `backend/include/services/app_service.h`
- `backend/src/services/messages_notifications.cpp`
- `backend/src/services/app_context.cpp`
- `backend/src/controllers/routes.cpp`
- `frontend/src/components/Layout.tsx`
- `frontend/src/pages/Messages.tsx`
- `frontend/src/pages/Notifications.tsx`
- `docs/API.md`
- `docs/DSA.md`
- `PROGRESS.md`

### Completed functionality

- [x] Audited the existing message routes, notification routes, schema tables,
      DSA caches, and frontend Messages/Notifications/Layout components first.
- [x] Added `GET /api/messages/unread`, backed by a rebuilt
      `unreadConversations` cache that counts distinct conversations with unread
      non-typing messages.
- [x] Made `notificationQueue` a real read-path cache: it rebuilds from the
      newest 100 unread MySQL rows at startup and after single/read-all reads,
      and `GET /api/notifications/unread` traverses it for the viewer's ids.
- [x] Fixed typing indicators: `typing=false` now deletes the row, new typing
      replaces previous rows from the same sender, sending a message clears the
      sender's typing row, and typing rows are excluded from unread counts,
      message history, and conversation previews.
- [x] Fixed media-only conversation previews to show `[Photo]` instead of an
      empty body.
- [x] Hardened notification read endpoints with affected-row detection: reading
      another user's notification returns 404 instead of silently succeeding.
- [x] Kept participant and block checks enforced on conversation creation,
      history, and messaging.
- [x] Added unread message badges and notification-badge refresh events to the
      shared layout; Messages now polls conversations and emits send/read events,
      and Notifications emits read events.
- [x] Updated API and DSA docs with Phase 6 behavior and complexity.

### DSA use

`IdQueue` now serves the unread notification count as a bounded hot cache; it is
rebuilt from MySQL and traversed with `forEach`. `HashTable<std::string, bool>`
`unreadConversations` is rebuilt from unread, non-typing messages and traversed
by viewer prefix for `GET /api/messages/unread`. MySQL remains authoritative and
both caches recover automatically after a backend restart. Details are in
`docs/DSA.md`.

### Test results

- Backend rebuild: passed.
- C++ DSA suite: `All DSA tests passed (105 assertions)`.
- Frontend production build: passed (`1608` modules transformed).
- Focused Phase 6 API preparation: 7/7 passed (3 temporary users, conversation,
  text and image messages, unread cache seed).
- Focused Phase 6 API verification: 27/27 passed, covering unread counts,
  conversation previews, notification ownership/read transitions, typing
  lifecycle, participant checks, block enforcement, and auth.
- Restart message setup: 1/1 passed.
- Restart persistence checks: 5/5 passed, covering unread conversation rebuild,
  message persistence, notification queue rebuild, and read-all cache clearing.
- Cleanup: 5/5 passed; all 4 tracked tables and temporary uploads restored.
- Browser smoke: Messages showed bob/charlie previews and unread badges; opening
  bob's conversation issued `/api/messages/1/read` and `/api/messages/unread`,
  and the sidebar badge fell from 2 to 1. No console errors.
- Demo state restored: `users=5`, `conversations=3`, `messages=8`,
  `notifications=6`; alice's unread message ids (4, 7) and notification ids
  (1, 2, 3) were restored after the browser smoke.

### Remaining issues

- No functional Phase 6 blockers are known.
- Live delivery still uses polling (8s conversations, 15s sidebar counts) rather
  than WebSockets; this was the existing design and remains acceptable for the
  demo.
- Abandoned typing rows have no TTL sweeper if a client closes mid-typing; a
  future cleanup job could remove rows older than a few minutes.
- The frontend still has no block/unblock button; backend block enforcement is
  verified, and the UI control remains a Phase 7/8 candidate. **Resolved in Phase 8:**
  Profile now has a Block action and Settings retains Unblock.

## Phase 7 — Search, Hashtags and Discovery — Completed

### Modified files

- `backend/include/services/app_service.h`
- `backend/src/services/app_context.cpp`
- `backend/src/services/helpers.cpp`
- `backend/src/services/messages_notifications.cpp`
- `backend/src/services/posts.cpp`
- `backend/src/services/users.cpp`
- `frontend/src/pages/SearchPage.tsx`
- `docs/API.md`
- `docs/DSA.md`
- `PROGRESS.md`

### Completed functionality

- [x] Audited the existing search, explore, hashtag, trending, and upload
      routes/services/tables and the Search/Explore UI before making changes.
- [x] Added `AppContext::hashtagTrie` and `AppContext::trendingHashtags`; both
      are cleared and rebuilt from `hashtags` at backend startup.
- [x] Connected `syncHashtags()` and `replaceHashtags()` to the hashtag Trie:
      new tags are inserted, and tags whose usage drops to zero are removed from
      both MySQL and the Trie after post edits/deletes.
- [x] Normalized `/api/search?q=` and `/api/explore?tag=` by trimming, removing
      one leading `#`, and lowercasing; empty search queries no longer write a
      `searches` row.
- [x] Hashtag suggestions now come from Trie prefix candidates hydrated by
      MySQL, with only active tags (`usage_count > 0`) returned and ordered by
      usage then tag name.
- [x] Search post results now carry accurate viewer `likedByViewer` and
      `savedByViewer` flags and exclude blocked users and private authors
      without an accepted follow.
- [x] Removed search recording from `searchUsers()` so people lookup does not
      create a second `searches` row; `recordSearch()` skips consecutive
      duplicate normalized queries.
- [x] `GET /api/search/recent` deduplicates with `GROUP BY query`, newest first.
- [x] `GET /api/hashtags/trending` uses the custom `MaxHeap` with total usage
      plus a seven-day recency score and returns `recentCount`/`score`.
- [x] `SearchPage.tsx` renders trending hashtags with an empty state and
      error/retry handling, and strips a leading `#` before submitting.
- [x] Updated API and DSA docs with Phase 7 behavior and complexity.

### DSA use

`hashtagTrie` provides O(L) prefix lookup plus O(L + matches) bounded candidate
collection; MySQL hydrates the live top 10. `trendingHashtags` is a `MaxHeap`
ranking whose n inserts and top-12 drains cost O(n log n); it is rebuilt from
MySQL at startup and recalculated per request. Both caches mutate only after
successful hashtag writes. Details are in `docs/DSA.md`.

### Test results

- Backend rebuild: passed.
- C++ DSA suite: `All DSA tests passed (105 assertions)`.
- Frontend production build: passed (`1608` modules transformed).
- Focused Phase 7 API preparation: 6/6 passed (temporary users, hashtag post,
  like, and save).
- Focused Phase 7 API verification: 30/30 passed, covering normalized `#tag`
  search/explore, Trie suggestions, people results, hydrated viewer flags,
  single-write and deduped recent searches, trending score ordering, blocked-user
  filtering, caption-edit hashtag removal, and auth.
- Backend-restart persistence: 4/4 passed, proving the hashtag Trie, trending
  heap, search results, and explore results rebuild correctly.
- Cleanup: 8/8 passed after the smoke test and final verification; all tracked
  tables and temporary uploads were restored.
- Browser smoke: Search page showed recent searches and trending hashtags;
  submitting `#p7_...alpha` issued `GET /api/search?q=p7_...alpha` (200) and
  rendered the hashtag and matching post. No console errors.
- Demo state restored: `users=5`, `posts=4`, `post_media=4`, `hashtags=11`,
  `post_hashtags=9`, `searches=12`, `notifications=6`.

### Remaining issues

- No functional Phase 7 blockers are known.
- Trending scores are recalculated per request rather than maintained
  continuously; this keeps MySQL authoritative and the cost is O(n log n).
- Caption search uses SQL `LIKE`, not a full-text index; the current demo scale
  is small enough that the candidate scan is acceptable.
- The frontend still has no block/unblock button; the backend enforcement path
  was exercised by the Phase 7 harness. **Resolved in Phase 8:** a Block button
  was added to the Profile More menu and verified in the browser.

## Phase 8 — Final Integration, Testing and Optimization — Completed

### Modified files

- `backend/include/services/app_service.h`
- `backend/src/controllers/routes.cpp`
- `backend/src/services/helpers.cpp`
- `backend/src/services/posts.cpp`
- `backend/src/services/stories_reels.cpp`
- `frontend/src/pages/Profile.tsx`
- `frontend/src/pages/Settings.tsx`
- `docs/API.md`
- `docs/DSA.md`
- `docs/PHASES.md`
- `README.md`
- `PROGRESS.md`

### Completed functionality

- [x] Audited the full route/service/page surface and reconciled the remaining
      gaps before changing anything; no project, schema, or working feature was
      recreated or removed.
- [x] Made `likedPosts` and `savedPosts` real read-path caches:
      `hydratePost()` now looks up the viewer/post key with `HashTable::search()`
      after startup rebuild and successful like/save writes.
- [x] Made `feedHeap`, `trendingReels`, and `trendingHashtags` real read-path
      heaps: requests insert into the `AppContext` heap under `dsaMutex` and
      drain it to produce ranked output instead of copying a local heap into an
      otherwise unused field.
- [x] Added confined media cleanup on `DELETE /api/posts/<id>`: `post_media.url`
      values are collected before the DB delete, and `removeUploadedFile()`
      removes only regular files under `IG_UPLOAD_DIR` after the delete succeeds.
- [x] Hardened `/uploads/<path>` with `weakly_canonical()` and a path-component
      boundary check, so sibling directories with a matching prefix cannot escape
      the upload root.
- [x] Applied private-account visibility to reels: owners and accepted followers
      can see private-account reels, while other viewers cannot; block exclusions
      remain in the same candidate query.
- [x] Added a Block action to the Profile More menu, calling
      `POST /api/users/<id>/block`; Settings continues to provide Unblock.
- [x] Completed the frontend password-reset flow inside Settings: request a dev
      token, enter a new password, call `POST /api/auth/reset-password`, then log
      out so the new password is used. The backend already invalidates all
      sessions for that user on reset.
- [x] Updated docs to stop describing write-only caches and missing UI controls,
      and corrected stale test counts.

### DSA use

The final read paths now exercise the same structures they populate:
`HashTable` answers viewer like/save membership in `hydratePost()`,
`MaxHeap` drains feed/reel/hashtag rankings, `Trie` answers username and hashtag
prefix searches, `FollowGraph` answers follow/mutual/suggestion queries,
`IdQueue`/`HashTable` answer unread notification/conversation counts, and
`IdDeque`/`HashSet` order and prune stories. MySQL remains authoritative; every
cache is rebuilt at startup and mutated only after successful writes.

### Test results

- Backend rebuild after all C++ changes: passed.
- C++ DSA suite: `All DSA tests passed (105 assertions)`.
- Frontend production build: passed (`1608` modules transformed, `tsc -b` clean).
- Focused Phase 8 journey harness:
  - preparation/mutations (15 journey steps plus delete hygiene): **66/66 passed**;
  - read-only verification: **14/14 passed**;
  - restart persistence: **15/15 passed**;
  - cleanup/restoration: **26/26 passed**.
- An initial cleanup run exposed one orphan temporary hashtag; cleanup was fixed
  to delete temporary hashtags explicitly, and the final run restored all 24
  tracked counts plus the upload-file count exactly.
- Final API smoke with demo `alice` login/logout: passed; health, feed, stories,
  trending reels, unread notifications, and suggestions all returned 200, and
  the session count returned to 14.
- Browser smoke: login, Home feed modes, Search, Explore, Reels, Messages,
  Notifications, Profile, Settings, the Settings reset-password form, and the
  Profile **Block user** menu action all rendered correctly with no console
  errors. The browser session was logged out and temporary reset rows removed.
- Demo state restored: `users=5`, `posts=4`, `post_media=4`, `comments=5`,
  `likes=9`, `saved_posts=3`, `hashtags=11`, `post_hashtags=9`, `searches=12`,
  `followers=12`, `follow_requests=1`, `blocked_users=0`, `stories=3`,
  `story_views=3`, `reels=2`, `reel_views=7`, `reel_likes=5`, `reel_comments=3`,
  `reel_saves=2`, `conversations=3`, `messages=8`, `notifications=6`,
  `sessions=14`, `password_resets=0`, `upload_files=27`.

### Final acceptance

All 15 user-journey steps from `tests/test_api.py` were exercised by the focused
Phase 8 harness without mutating demo data: health/auth, register/login, profile
update, follow, post create, feed modes, like/save/comment, explore/trending,
stories, reels, messaging, notifications, search/recent, forgot/reset password,
and logout/re-login. Backend restart checks confirmed persisted posts,
interactions, hashtags, stories, reels, messages, notifications, recent searches,
and reset credentials. **Phase 8 is complete and no functional blockers remain.**

### Remaining issues

- No functional Phase 8 blockers are known.
- `POST /api/upload` remains available for API clients, but the React UI still
  embeds base64 media in feature-specific post/story/reel/message payloads; it
  was intentionally left in place rather than removed or rewired at final integration.
- Notification unread counts still intersect the newest 100 unread rows from the
  queue with MySQL, so very large unread sets can be undercounted.
- Messaging remains polling-based, and abandoned typing rows still have no TTL
  sweeper; both are documented design limitations.
- Trending scores are still recalculated per request, and caption search still
  uses SQL `LIKE`; both are acceptable at the current demo scale.
