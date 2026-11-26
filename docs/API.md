# REST API Reference

Base URL: `http://127.0.0.1:8080`
All application endpoints are prefixed with `/api`. Media is served from
`/uploads/<filename>`.

## Conventions

- Request/response body is JSON (`Content-Type: application/json`).
- Protected endpoints require `Authorization: Bearer <token>`.
- Successful login/registration returns `{ "token": "...", "user": {...} }`.
- Errors return `{ "error": "message" }` with an appropriate HTTP status.
- `GET /api/health` is public and returns `{ "status": "ok", "service": "..." }`.
- `OPTIONS` is handled globally for CORS preflight.

Parameter notation: `<uint>` is an unsigned integer path parameter;
`?a=b` denotes a query parameter.

---

## Authentication

| Method | Path | Auth | Description |
|---|---|---|---|
| POST | `/api/auth/register` | no | Create account. Body: `username`, `email`, `password`, optional `fullName`. Returns token + user. |
| POST | `/api/auth/login` | no | Body: `username` (or email), `password`. Returns token + user. |
| POST | `/api/auth/logout` | yes | Invalidates the current session token. |
| GET | `/api/auth/me` | yes | Returns the authenticated user. |
| POST | `/api/auth/forgot-password` | no | Body: `email`. Returns a `devToken` because no mail service is configured. |
| POST | `/api/auth/reset-password` | no | Body: `token`, `password`. |

---

## Users and social graph

| Method | Path | Auth | Description |
|---|---|---|---|
| GET | `/api/users/<uint>` | yes | Public profile with counts and viewer relationship. |
| PUT | `/api/users/<uint>` | yes | Update own profile: `fullName`, `bio`, `website`, `email`, `profilePicture`, `isPrivate`. |
| GET | `/api/users/search?q=` | yes | Trie-backed username prefix search; returns `results`. |
| GET | `/api/users/suggested` | yes | Legacy graph-backed suggestions; returns `users`. |
| GET | `/api/users/<uint>/suggestions` | yes | BFS suggestions for the authenticated viewer; path ID is validated but never trusted as the follower. Returns `users`. |
| POST | `/api/users/<uint>/follow` | yes | Follow using the authenticated user as `follower_id`; private targets return status `requested`. Duplicate requests are idempotent. |
| DELETE | `/api/users/<uint>/follow` | yes | Unfollow (also cancels a pending request). |
| GET | `/api/users/<uint>/follow-status` | yes | Returns `isFollowing`, `followStatus`, `followersCount`, `followingCount`, `mutualCount`, `isOwnProfile`. |
| GET | `/api/users/<uint>/followers` | yes | Follower list from MySQL. |
| GET | `/api/users/<uint>/following` | yes | Following list from MySQL. |
| GET | `/api/users/<uint>/mutual` | yes | Mutual follows; returns `users`. |
| GET | `/api/users/requests` | yes | Pending follow requests for a private account. |
| POST | `/api/users/requests/<uint>` | yes | Body: `{ "accept": true|false }`. |
| POST | `/api/users/<uint>/block` | yes | Block user. |
| DELETE | `/api/users/<uint>/block` | yes | Unblock user. |
| GET | `/api/users/blocked` | yes | Blocked users. |
| POST | `/api/users/report` | yes | Body: `userId`, optional `postId`, `reason`, `details`. |

---

## Posts, feed and explore

| Method | Path | Auth | Description |
|---|---|---|---|
| POST | `/api/posts` | yes | Body: `caption`, `location`, `media: [{data, filename, type}]`. Returns `post`. |
| GET | `/api/posts?userId=&page=&limit=` | yes | Posts by user. |
| GET | `/api/posts/saved` | yes | Saved posts. |
| GET | `/api/posts/<uint>` | yes | Single hydrated post. |
| PUT | `/api/posts/<uint>` | yes | Update own caption/location. |
| DELETE | `/api/posts/<uint>` | yes | Delete own post. |
| POST / DELETE | `/api/posts/<uint>/like` | yes | Like / unlike. |
| POST / DELETE | `/api/posts/<uint>/save` | yes | Save / unsave. |
| GET | `/api/posts/<uint>/comments` | yes | Comments. |
| POST | `/api/posts/<uint>/comments` | yes | Body: `{ "body": "..." }`; returns `comment`. |
| DELETE | `/api/comments/<uint>` | yes | Delete own comment. |
| GET | `/api/feed?mode=following|popular|recent&page=&limit=` | yes | Feed; returns `posts`, `page`, `order`, `hasMore`. |
| GET | `/api/explore?tag=&page=&limit=` | yes | Explore by hashtag. |
| GET | `/api/hashtags/trending` | yes | Returns `hashtags`. |

### Phase 3 behavior details

- `POST /api/posts` requires `media` to be a non-empty array of at most 10
  items. Caption length is capped at 5000 characters and location at 150.
  Media items accept base64 `data`, `filename`, and `type` (`image`/`video`);
  invalid media is skipped, and the post is rolled back if none can be saved.
- Hashtags are parsed from `#tag`, lowercased, and deduplicated per caption.
  `hashtags.usage_count` therefore counts posts, not repeated occurrences.
  Editing a post resyncs its hashtag rows; deleting a post clears its usage.
- `PUT /api/posts/<uint>` and `DELETE /api/posts/<uint>` are author-only.
  Omitted caption/location values are preserved on update. Responses return the
  hydrated `post` with `likeCount`, `commentCount`, `likedByViewer`,
  `savedByViewer`, and `hashtags`.
- Like and save endpoints are idempotent. `POST/DELETE .../like` returns
  `{ "postId", "liked", "likeCount" }`; a duplicate like does not create a
  second notification. `POST/DELETE .../save` returns `{ "postId", "saved" }`.
- `POST /api/posts/<uint>/comments` requires a trimmed 1–1000 character `body`.
  List/add/delete responses include `commentCount`; only the comment author may
  delete a comment.
- Missing posts return `404`. Posts the viewer may not see (blocked relationship
  or private author without an accepted follow) return `403` for
  get/edit/like/save/comment operations. Feed, explore, and saved lists silently
  omit unavailable posts. A pending follow request does not unlock a post.
- `GET /api/feed?mode=following|popular`, `GET /api/explore`, and
  `GET /api/posts/saved` return `{ "posts": [...] }`; feed also returns `page`
  and `hasMore`. Explore accepts `?tag=...`; without a tag it falls back to
  latest eligible posts. Candidate rows are privacy-filtered in MySQL before
  the C++ `MaxHeap` ranking step.
- Like/save state is cached in memory but rebuilt from MySQL at startup, so
  interactions survive a backend restart.

### Phase 4 feed behavior

- `GET /api/feed` accepts `mode=following`, `mode=popular` (default ranked), and
  `mode=recent` / `mode=chronological`. Unknown modes fall back to ranked.
- All modes are authenticated and privacy-filtered: own posts, public posts,
  and accepted-follow private posts are eligible; blocked relationships are
  excluded. `following` further restricts to the viewer and followed accounts.
- `mode=recent` returns `order: "chronological"` and uses the SQL
  `created_at DESC` order directly, bypassing ranking.
- `mode=popular` and `mode=following` return `order: "ranked"` and score
  candidates with the custom `MaxHeap` using recency, likes, comments, follow
  relationship, and recent activity.
- `page` defaults to 1; `limit` is clamped to 5–30. Responses include `page`,
  `mode`, `order`, and `hasMore` so the UI can implement Load more pagination.

---

## Stories and reels

| Method | Path | Auth | Description |
|---|---|---|---|
| POST | `/api/stories` | yes | Body: `data`, `filename`, `type`. |
| GET | `/api/stories` | yes | Returns `stories` and `mine`. |
| POST | `/api/stories/<uint>/view` | yes | Record a view. |
| POST | `/api/reels` | yes | Body: `data`, `filename`, `caption`, optional `thumbnailData`, `thumbnailFilename`. |
| GET | `/api/reels?mode=latest|trending&page=&limit=` | yes | Reels feed; trending uses MaxHeap. |
| GET | `/api/reels/saved` | yes | Saved reels. |
| POST / DELETE | `/api/reels/<uint>/like` | yes | Like / unlike. |
| POST / DELETE | `/api/reels/<uint>/save` | yes | Save / unsave. |
| POST | `/api/reels/<uint>/view` | yes | Record a view. |
| GET / POST | `/api/reels/<uint>/comments` | yes | List / add reel comments. |

### Phase 5 story and reel behavior

- Stories require base64 `data`; `filename` selects the extension and `type`
  (`image`/`video`) selects the stored media type. `duration` is clamped to
  1000–30000 ms. New stories expire 24 hours after creation.
- `GET /api/stories` returns two arrays: `stories` (active stories from accounts
  the viewer follows, excluding blocked relationships) and `mine` (the viewer's
  own active stories). Both are ordered oldest → newest within each user and
  grouped by user in the UI. Expired stories are pruned from the DSA cache and
  hidden even if their row still exists.
- `POST /api/stories/<uint>/view` is idempotent via the `story_views` primary
  key. The first view of another user's story creates one `story_view`
  notification; repeat views do not duplicate the view row or notification.
- `GET /api/reels?mode=latest` returns active reels in `created_at DESC` order.
  `mode=trending` scores up to 200 privacy-filtered reels with the custom
  `MaxHeap` using log-scaled views/likes/comments plus an exponential recency
  boost, then paginates the ranked result. Unknown modes are treated as latest.
- Reel views are idempotent per viewer (`reel_views` primary key): only the
  first view increments `reels.views_count`. Likes are also idempotent and
  notify only on the first like; comments and saves are persisted in MySQL.
- `GET /api/reels/saved` returns the viewer's saved reels in save-time order.
  The Reels UI exposes Latest, Trending, and Saved tabs.

---

## Messages and notifications

| Method | Path | Auth | Description |
|---|---|---|---|
| GET | `/api/messages/conversations?q=` | yes | Conversation list, optionally searched. |
| POST | `/api/messages/conversation/<uint>` | yes | Get or create conversation with user id; body may repeat `recipientId`. Returns `conversationId`. |
| GET | `/api/messages/conversation/<uint>` | yes | Alias returning messages for a peer user id. |
| GET | `/api/messages/<uint>?before=&limit=` | yes | Messages in a conversation; also `typing`. |
| POST | `/api/messages` | yes | Body: `conversationId`, `body`, optional `mediaData`, `filename`. Returns `message`. |
| POST | `/api/messages/<uint>/read` | yes | Mark conversation read. |
| POST | `/api/messages/<uint>/typing` | yes | Body: `{ "typing": true|false }`. |
| GET | `/api/messages/unread` | yes | Returns `{ "count": n }` unread conversations. |
| GET | `/api/notifications` | yes | Notification list. |
| GET | `/api/notifications/unread` | yes | Returns `{ "count": n }`. |
| POST | `/api/notifications/read-all` | yes | Mark all read. |
| POST | `/api/notifications/<uint>/read` | yes | Mark one read. |

### Phase 6 messaging and notification behavior

- `GET /api/messages/conversations` excludes typing indicators from the
  `lastMessage` preview and shows `[Photo]` for media-only messages. The UI
  polls this endpoint every 8 seconds and the sidebar polls unread counts every
  15 seconds; local `ig:messages-changed` / `ig:notifications-changed` events
  refresh badges immediately after send/read actions.
- Typing state is stored as short-lived rows with `typing = 1`. Enabling typing
  replaces any previous typing row from that sender; sending a message or
  disabling typing deletes it. Typing rows never count as unread and are hidden
  from message history and previews.
- `GET /api/messages/unread` returns the number of distinct conversations with
  unread real messages for the viewer. The count is derived from the
  `unreadConversations` DSA cache, which is rebuilt from MySQL on every request
  so restarts and external changes cannot leave it stale. Marking a conversation
  read clears the matching key.
- `GET /api/notifications/unread` intersects the bounded `notificationQueue`
  cache with the viewer's unread notification ids from MySQL. The queue is
  rebuilt from the newest 100 unread rows at startup and after single/read-all
  transitions. `POST /api/notifications/<uint>/read` returns 404 when the id is
  not owned by the viewer.
- Conversation creation, reads, and message history remain participant-only;
  blocked users cannot start a new conversation with the blocker.

---

## Search and uploads

| Method | Path | Auth | Description |
|---|---|---|---|
| GET | `/api/search?q=` | yes | Returns `users`, `hashtags`, `posts` and records the query. |
| GET | `/api/search/recent` | yes | Returns unique `queries`, newest first. |
| POST | `/api/upload` | yes | Body: `data`, `filename`, optional `type`; returns `{ "url": "/uploads/..." }`. |
| GET | `/uploads/<path>` | no | Serve local media from `IG_UPLOAD_DIR`. |

### Phase 7 search and discovery behavior

- `GET /api/search?q=` trims the query, strips one leading `#`, and lowercases
  it. Empty queries return empty arrays without writing a search row.
- Hashtag suggestions come from the `hashtagTrie` prefix index and are hydrated
  from `hashtags`; only tags with `usage_count > 0` are returned as
  `{ tag, count }`, ordered by count then tag name (at most 10).
- Post results match the normalized query in captions or exact hashtags, are
  capped at 12, and include hydrated `likedByViewer` and `savedByViewer` flags.
  Blocked relationships and private authors without an accepted follow are
  excluded. People results come from username prefix search; browsing people does
  not itself record a search.
- `recordSearch()` stores at most 120 characters, ignores queries shorter than
  two characters, and skips a write when the normalized query equals the
  viewer's most recent search. `GET /api/search/recent` applies `GROUP BY query`
  and returns each query once, ordered by its most recent timestamp.
- `GET /api/hashtags/trending` ranks active tags with the custom `MaxHeap`. The
  score uses total usage plus a weighted seven-day recency boost; each result
  includes `tag`, `count`, `recentCount`, and `score`, ordered by score.
- `GET /api/explore?tag=` accepts a leading `#`, trims, and lowercases the tag
  before filtering `post_hashtags`. It returns `{ posts, tag }` with the same
  block/privacy filtering and viewer like/save hydration as search. With no tag
  it falls back to latest eligible posts.

### Phase 8 final integration behavior

- `GET /api/feed?mode=popular|following`, `GET /api/reels?mode=trending`, and
  `GET /api/hashtags/trending` now insert into and drain the corresponding
  `AppContext` `MaxHeap` instances. The heaps are rebuilt from MySQL at startup
  and repopulated per request, so ranking uses the custom structure on the read
  path rather than a disconnected local copy.
- Viewer `likedByViewer` and `savedByViewer` flags are answered from the
  `likedPosts` / `savedPosts` `HashTable` caches in `hydratePost()`. The keys are
  rebuilt from `likes` / `saved_posts` at startup and changed only after
  successful writes; MySQL remains the source of truth.
- `DELETE /api/posts/<id>` removes the post's `post_media` files from
  `IG_UPLOAD_DIR` after the database delete succeeds. `removeUploadedFile()`
  rejects non-`/uploads/` URLs, nested paths, and any target outside the canonical
  upload root.
- `GET /uploads/<path>` uses `weakly_canonical()` plus a path-component boundary
  check, so a sibling directory whose name starts with the upload root name
  cannot be served.
- `GET /api/reels` applies the same private-account rule as feed/story discovery:
  private-account reels are visible to the owner and accepted followers only,
  with blocked relationships still excluded.
- `POST /api/auth/forgot-password` and `POST /api/auth/reset-password` are now
  reachable from the Settings UI. A successful reset deletes all sessions for
  that user, so the client logs out and the user signs in with the new password.
- `POST /api/users/<id>/block` is now exposed in the Profile More menu; Settings
  continues to list blocked users and call `DELETE /api/users/<id>/block`.
- `POST /api/upload` is supported for API clients that want a standalone media
  URL. The current React UI still sends base64 bytes inside post, story, reel,
  and message payloads; no feature was rewired at final integration.

---

## Example: create a post with an uploaded image

```json
POST /api/posts
Authorization: Bearer <token>
{
  "caption": "Sunset #travel #sunset",
  "location": "Lahore",
  "media": [
    {
      "data": "<base64-encoded image bytes>",
      "filename": "sunset.png",
      "type": "image"
    }
  ]
}
```

Response:

```json
{
  "post": {
    "id": 12,
    "userId": 1,
    "username": "alice",
    "caption": "Sunset #travel #sunset",
    "hashtags": ["travel", "sunset"],
    "media": [{ "id": 14, "url": "/uploads/sunset.png", "type": "image", "position": 0 }],
    "likeCount": 0,
    "commentCount": 0,
    "likedByViewer": false,
    "savedByViewer": false
  }
}
```

For complete request/response assertions, see `tests/test_api.py` and the focused
Phase 3 harness described in `docs/PHASE3_REPORT.md`.
