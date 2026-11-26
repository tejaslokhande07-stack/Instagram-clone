# Phase 3 Final Report — Posts, Likes and Comments

Phase 3 is complete. Phase 1 (auth/profile/DSA) and Phase 2 (follow system)
remain intact; all Phase 3 changes are additive or defensive fixes around posts,
likes, saves, comments, feed/explore visibility, and their DSA caches.

## 1. Files changed

### Backend

| File | Change |
|---|---|
| `backend/include/database/database.h` | Declares `Database::executeCount()` for affected-row reporting. |
| `backend/src/database/database.cpp` | Implements `executeCount()` so `INSERT IGNORE` / `DELETE` results can be checked correctly. |
| `backend/include/services/app_service.h` | Adds `interactionKey()`, string-keyed like/save/unread caches, and Phase 3 helper declarations. |
| `backend/src/services/helpers.cpp` | Implements visibility, count, cache, recent-post, hashtag sync/replace helpers; privacy-filtered feed candidates. |
| `backend/src/services/posts.cpp` | Hardens create/update/delete, likes, saves, comments, feed, explore, and saved-post behavior. |
| `backend/src/services/app_context.cpp` | Rebuilds string-keyed caches from MySQL at startup. |
| `backend/src/services/messages_notifications.cpp` | Uses composite keys for unread conversation cache entries. |
| `backend/src/services/users.cpp` | Fixes latent follow-request bug: selecting non-existent `id` from `follow_requests` caused every accept/reject to return 404. |
| `backend/include/dsa/hash_table.hpp` | Adds explicit `HashTable<std::string, bool>` instantiation. |
| `backend/src/dsa/hash_table.cpp` | Adds matching explicit instantiation definition. |
| `tests/test_dsa.cpp` | Adds `testCompositeKeyCache()` for collision-safe cache set/remove semantics. |

### Frontend

| File | Change |
|---|---|
| `frontend/src/components/PostCard.tsx` | Like/save busy guards; server-authoritative count reconciliation; comment loading/error/retry; comment length cap; `onDeleted` callback. |
| `frontend/src/components/CreatePostModal.tsx` | Media count/type validation, caption/location counters and limits, object-URL cleanup. |
| `frontend/src/pages/PostDetail.tsx` | Navigates home after the displayed post is deleted. |

### Documentation / progress

| File | Change |
|---|---|
| `PROGRESS.md` | Phase 3 checklist, results, and notes. |
| `README.md` | Updated DSA assertion expectation to 105 and linked the Phase 3 report/progress docs. |
| `docs/API.md` | Phase 3 endpoint behavior, validation, counts, privacy, and cache notes. |
| `docs/DSA.md` | Updated from 75 to 105 assertions; documents string-keyed caches and privacy-filtered heap candidates. |
| `docs/PHASE3_REPORT.md` | This report. |

The focused API harness lived at `%TEMP%\opencode\phase3_test.py` and was not
added to the repository.

## 2. Implementation details

### Posts

- `POST /api/posts` validates a non-empty `media` array with at most 10 items,
  caption ≤ 5000 characters, and location ≤ 150 characters.
- Invalid media is skipped; if no media can be saved, the inserted post is
  rolled back with a `400`.
- `PUT /api/posts/:id` is author-only, preserves omitted caption/location, and
  resynchronizes hashtag rows after the edit.
- `DELETE /api/posts/:id` is author-only, clears hashtag usage, and removes the
  post from DSA caches.
- Hashtags are lowercased and deduplicated per caption, so `usage_count`
  represents distinct posts, not repeated occurrences.

### Likes and saves

- Duplicate `INSERT IGNORE` / `DELETE` effects are detected with
  `executeCount()` affected rows instead of the auto-increment ID.
- Like/unlike return `{ postId, liked, likeCount }`; save/unsave return
  `{ postId, saved }`.
- Duplicate likes no longer create duplicate notifications.
- Cache membership keys are `interactionKey(viewer, post)` (`"viewer:post"`),
  eliminating collisions from numeric sums such as `1 + 4 == 2 + 3`.
- `GET /api/posts/saved` filters blocked and private posts.

### Comments

- `POST /api/posts/:id/comments` trims and validates a 1–1000 character body.
- List/add/delete responses include `commentCount` in addition to the comment
  data.
- Only the comment author may delete a comment.
- Missing posts return `404`; inaccessible posts return `403`.

### Privacy and feed/explore

- `canViewPost()` enforces owner/block/private-follow rules for all post
  interactions.
- Feed, explore, and saved-post SQL queries exclude blocked relationships and
  private authors unless the viewer is an accepted follower.
- A pending follow request does not unlock a private post.
- MySQL supplies privacy-filtered candidate rows; the C++ `MaxHeap` then scores
  and orders them using recency, likes, comments, follow relationship, and
  recent activity.

### DSA caches

- `HashTable<std::string, bool>` stores liked posts, saved posts, and unread
  conversations.
- `AppContext::rebuildIndexes()` reconstructs those caches from MySQL at
  startup, alongside the username index, follow graph, trie, recent-activity
  deque, notification queue, and feed heap.
- Caches are mutated only after successful database writes.

## 3. API surface verified

Existing Phase 3 endpoints exercised directly:

- `GET /api/health`
- `POST /api/auth/register`, `GET /api/auth/me`
- `POST /api/posts`, `GET /api/posts/:id`, `PUT /api/posts/:id`,
  `DELETE /api/posts/:id`
- `POST /api/posts/:id/like`, `DELETE /api/posts/:id/like`
- `POST /api/posts/:id/save`, `DELETE /api/posts/:id/save`,
  `GET /api/posts/saved`
- `GET /api/posts/:id/comments`, `POST /api/posts/:id/comments`,
  `DELETE /api/comments/:id`
- `GET /api/feed?mode=popular`, `GET /api/explore?tag=...`
- `PUT /api/users/:id` (private toggle)
- `POST /api/users/:id/follow`, `GET /api/users/requests`,
  `POST /api/users/requests/:requester`

## 4. Database changes

No schema change was required. Phase 3 uses the existing tables:
`posts`, `post_media`, `likes`, `saved_posts`, `comments`, `hashtags`,
`post_hashtags`, `followers`, `follow_requests`, `blocked_users`, and
`notifications`. MySQL remains the permanent source of truth.

Behavioral DB effects verified:

- Like and save rows are unique per `(user_id, post_id)`.
- Duplicate likes produce one notification per like action, not per request.
- Hashtag usage is decremented on edit/delete and deduplicated on create.
- Post, like, save, comment, and hashtag state survives a backend restart.

## 5. Frontend features

- Composer: image-only file selection, maximum 10 images, caption/location
  counters and hard limits, toast feedback, and object-URL cleanup on close.
- Post card: optimistic like/save with busy guards and response reconciliation,
  double-tap like, save state, comment count, and post menu.
- Comments modal: loading spinner, deterministic error state with **Try again**,
  disabled composer while loading/erroring, add/delete comment, and updated
  counts.
- Explore: hashtag search and grid/detail modal now respect private/blocked
  visibility through the backend.
- Direct post route (`/post/:id`) loads a single hydrated post and returns home
  after deletion.

## 6. Test results

| Test | Result |
|---|---|
| C++ DSA suite (`ig_dsa_tests.exe`) | **105 assertions passed** |
| Focused Phase 3 API prepare | **60/60 passed** |
| Focused Phase 3 API verify after backend restart | **16/16 passed** |
| Focused Phase 3 API cleanup | **5/5 passed** |
| Backend build after final fix | passed |
| Frontend production build | passed, `1608` modules transformed |
| Browser smoke | passed |

Browser smoke covered: create post → like → unlike → comment fetch failure →
retry → add comment → explore hashtag → `/post/:id` → delete post. No console
errors and no failed requests were observed; only pre-existing React Router v7
future-flag warnings remain.

Demo state after cleanup: `users=5`, `temp_users=0`, `posts=4`, `comments=5`,
`likes=9`, `saved=3`, `notifications=6`. Temporary users, posts, comments, likes,
saves, notifications, hashtags, and the smoke upload were removed.

## 7. Demo steps

1. Start MySQL on port `3307` using
   `database/runtime/my-local.ini`.
2. Start the backend from `I:\build\ig_server.exe` with
   `C:\ig-mingw-real\bin` on `PATH` and `IG_UPLOAD_DIR=I:\uploads`.
3. Start the frontend with `npm run dev` (or serve `frontend/dist`).
4. Log in as `alice` / `Pass@123`.
5. Click **Create**, choose up to 10 images, add a caption with `#demo`, and
   share.
6. Like/unlike and save/unsave the new post; verify counts change and persist
   after a backend restart.
7. Open comments; add and delete a comment and verify the count updates.
8. Search `#demo` on Explore and open the post from the grid.
9. As `bob`, request to follow a private `alice`; confirm the post stays hidden
   until `alice` accepts the request.

## 8. Remaining issues

- No functional Phase 3 blockers are known.
- React Router emits two pre-existing v7 future-flag warnings in the browser
  console; they do not affect behavior.
- Deleting a post removes its database rows but does not physically delete
  uploaded media files from `IG_UPLOAD_DIR`. Manual cleanup was performed for
  the smoke upload; adding a storage garbage collector would be a future
  improvement outside Phase 3.
- The full `tests/test_api.py` suite was intentionally not run because it spans
  other feature areas and mutates demo data. Verification used the focused
  Phase 3 harness plus restart, browser, DSA, and build checks.
