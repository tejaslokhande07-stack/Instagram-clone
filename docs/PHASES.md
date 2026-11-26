# Implementation Phases

The project was built incrementally. Each phase ended with something runnable
and verified; no phase ended with TODO-only stubs.

## Phase 0 — Portable toolchain and local services

**Goal:** make the project buildable without relying on globally installed
tools or admin rights.

- Added portable Node, CMake and MinGW-w64 under `.tools/`.
- Copied the compiler to `C:\ig-mingw-real\bin` to avoid linker failures caused
  by spaces in the project path.
- Created the `I:` subst-drive workflow for all CMake commands.
- Bootstrapped a local MySQL 8 instance on port 3307 using
  `database/runtime/my-local.ini`.

**Verified:** compiler, CMake configure, MySQL connection and a minimal backend
startup.

## Phase 1 — Custom DSA library

**Goal:** implement and test eight practical data structures with separate
headers and sources.

- `HashTable`, `HashSet`, `FollowGraph`, `MaxHeap`, `Trie`, `IdQueue`,
  `IdDeque`, `DynamicVector`.
- Added insert/search/delete/traversal APIs, growth policies, copy/move
  semantics and template instantiations.
- Wrote `tests/test_dsa.cpp` covering normal paths, duplicates, deletions,
  growth, heap ordering, graph traversals and prefix queries.

**Verified:** `ig_dsa_tests.exe` — **105 assertions passed**.

## Phase 2 — Database schema and seed data

**Goal:** establish MySQL as the permanent source of truth.

- Created 26 normalized tables with foreign keys, unique constraints and
  indexes for relationship/media/message queries.
- Added `database/seed.sql` with five users, stable IDs, posts, hashtags,
  likes, comments, stories, reels, messages, notifications and a pending
  follow request.
- Added local demo media under `uploads/`.

**Verified:** clean re-application of `schema.sql` + `seed.sql`; seeded login
and feed queries.

## Phase 3 — Security and database layer

**Goal:** remove plaintext credentials and make database access safe.

- Argon2id hashing/verification; BLAKE2b-256 token fingerprints.
- CSPRNG opaque bearer tokens; sessions store only fingerprints.
- Prepared statements throughout; 8-connection pool with condition-variable
  wait and move-only connection guards.
- Base64/base64url helpers for binary uploads and reset tokens.

**Verified:** registration/login/logout tests, invalid-password rejection,
token invalidation after logout.

## Phase 4 — Service layer

**Goal:** implement all business logic in C++ independent of HTTP.

- Auth, users/social graph, posts/feed/explore, stories, reels, messages,
  notifications, search and upload services.
- `AppContext` rebuilds DSA indexes from MySQL at startup and updates them
  after successful writes.
- Privacy checks, blocking, follow-request approvals and notification side
  effects implemented centrally.

**Verified:** service methods compiled into `libig_backend.a`; DSA caches
rebuilt successfully on every server start.

## Phase 5 — Crow REST API

**Goal:** expose the service layer over HTTP.

- Registered all routes in `backend/src/controllers/routes.cpp`.
- Added CORS/OPTIONS handling, `/api/health`, JSON error translation and
  `/uploads/<path>` static media serving.
- Added a GET alias for message-by-conversation and password-reset endpoints.
- Fixed Windows static-file serving by validating the path ourselves and using
  Crow's unsafe static-file setter (the safe sanitizer rewrites the drive colon).

**Verified:** `tests/test_api.py` — **57 requests passed**, covering every API
area and error case.

## Phase 6 — React frontend

**Goal:** build a responsive, complete UI on top of the API.

- Scaffolded Vite + React + TypeScript + Tailwind with `/api` and `/uploads`
  proxies.
- Added `AuthContext` (session, theme, refresh) and `ToastContext`.
- Built shared components: layout/navigation, avatar, modal, spinner, post card,
  stories, create-post and create-story modals, user-list modal.
- Built pages: login, register, home, explore, reels, messages, profile,
  notifications, search, settings, post detail.
- Added dark mode, mobile navigation, loading/empty states, toasts, image
  fallbacks and optimistic like/save interactions.

**Verified:** `npm run build` type-checks and bundles successfully. Browser
walkthrough confirmed login, all pages, dark mode, modals, media loading and
protected-route redirects with zero console errors or failed requests.

## Phase 7 — Final verification and documentation (original plan)

**Goal:** leave a clean demo state and explain the work.

- Rebuilt backend after the static-file fix.
- Re-ran DSA tests (105 assertions) and API tests (57 requests).
- Re-applied `schema.sql` + `seed.sql` so the database is pristine.
- Restarted backend with `IG_UPLOAD_DIR=I:\uploads` and Vite dev server.
- Wrote `README.md`, `docs/DSA.md`, `docs/API.md` and this phase document.

**Final state:** backend on port 8080, frontend on port 5173, demo accounts
ready with password `Pass@123`.

## Phase 8 — Final integration, testing and optimization

**Goal:** close the remaining UI/DSA/hygiene gaps and verify the complete journey.

- Turned `likedPosts`, `savedPosts`, `feedHeap`, `trendingReels`, and
  `trendingHashtags` into genuine read-path caches used by response hydration
  and ranking.
- Added post-delete media file cleanup and canonical path-confinement for static
  `/uploads/<path>` serving.
- Applied private-account visibility to reels.
- Added the Profile block action and completed the Settings reset-password form.
- Corrected stale documentation counts and cache/UI claims.
- Ran a focused 15-step journey harness with temporary data, restart checks, and
  exact cleanup, plus final builds, API smoke, and browser smoke.

**Verified:** backend rebuild passed; DSA suite 105 assertions passed; frontend
build passed; journey harness 66/66 prepare, 14/14 verify, 15/15 restart, and
26/26 cleanup; final API smoke and browser smoke passed with no console errors.

**Final state:** demo database and uploads restored to baseline; backend on port
8080 and frontend on port 5173 remain available with demo password `Pass@123`.
