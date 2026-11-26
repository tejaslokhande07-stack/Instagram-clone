# Instagram Clone — React + C++17 Crow + MySQL + Custom DSA

A complete, working Instagram-style web application built for a data-structures
course project. The browser talks to a C++17 REST API; MySQL is the permanent
source of truth; eight custom DSA implementations are maintained as real
application-level indexes/caches and are exercised by unit tests and feature
flows — not left as unused demos.

---

## 1. What is implemented

### Frontend (`frontend/`)
- React 18 + TypeScript + Vite + Tailwind CSS + React Router.
- Login, registration, protected routes, persistent opaque bearer token.
- Responsive layout: desktop sidebar, mobile top bar and bottom navigation.
- Home feed with **For you / Following / Recent** modes, stories viewer, story
  creator, post composer, likes, comments, saves, shares, reports, deletion.
- Explore by hashtag, global search with recent searches, post detail page.
- Reels feed with latest/trending modes, views, likes, saves, comments, upload.
- Direct messages with conversation search, polling, typing indicator, read
  receipts and image attachments.
- Notifications with unread badge and mark-all-read.
- Profiles with followers/following/mutual lists, private accounts, follow
  requests, edit profile, report, and block action.
- Settings page with dark mode toggle, blocked-user unblocking, password reset
  token request + inline new-password submission, and logout.
- Toasts, loading states, empty states, gradient story rings, heart animation.

### Backend (`backend/`)
- C++17 Crow HTTP server; JSON via nlohmann/json.
- Argon2id password hashing and verification.
- Opaque random bearer tokens; only BLAKE2b-256 fingerprints are stored.
- Prepared MySQL statements with an 8-connection pool and RAII guards.
- Full service layer for auth, users, social graph, posts, feed/explore,
  stories, reels, messages, notifications, search and uploads.
- CORS, `OPTIONS` preflight, `/api/health`, and static media serving from
  `/uploads/<path>`.
- Base64 JSON uploads written to the local `uploads/` directory.

### Database (`database/`)
- Normalized MySQL 8 schema: 26 tables covering users, sessions, resets,
  profiles, posts, media, hashtags, stories, reels, relationships, messages,
  notifications, reports and searches.
- Reproducible `seed.sql` with five demo accounts, posts, hashtags, likes,
  comments, stories, reels, messages, notifications and a pending follow request.
- Demo media under `uploads/` (SVG avatars/posts/stories, reel thumbnail, MP4).

### DSA (`backend/include/dsa/`, `backend/src/dsa/`)
| Structure | Real use in the app |
|---|---|
| `HashTable` | username/id lookup indexes rebuilt from MySQL at startup |
| `FollowGraph` | adjacency lists for follow edges, BFS/DFS, mutual and suggestions |
| `MaxHeap` | feed ranking, trending reels and trending hashtags |
| `Trie` | username and hashtag autocomplete/search |
| `Queue` | notification event processor |
| `Deque` | recent activity/story ordering |
| `HashTable` | composite-key like/save/unread membership and lookup indexes |
| `HashSet` | active-story membership and BFS visited set |
| `DynamicVector` | in-memory collections of IDs, comments and media |

See [`docs/DSA.md`](docs/DSA.md) for APIs, complexities and integration points.

---

## 2. Architecture

```
Browser (React/TS)  --/api-->  Vite dev proxy  --/api-->  Crow C++17 server
        |                                                   |
        |--/uploads------------------------------------------>|--/uploads/<path>
                                                            |
                                                    MySQL 8 (port 3307)
                                                            |
                                              DSA indexes/caches in memory
```

MySQL is authoritative. Every write goes to a prepared statement first; DSA
structures are rebuilt at startup and updated on successful writes. If the
process restarts, the indexes are reconstructed from the database.

---

## 3. Prerequisites and portable toolchain

The repository already contains portable tools under `.tools/`:

- Node **v22.14.0** → `.tools/node/dist`
- CMake **3.31.6** → `.tools/cmake/cmake-3.31.6-windows-x86_64/bin`
- MinGW-w64 GCC **16.2.0** → copied to `C:\ig-mingw-real\bin`

MinGW libraries are required at runtime, so add `C:\ig-mingw-real\bin` to `PATH`
when starting the server.

Windows paths with spaces break MinGW linking. Use a subst drive:

```powershell
subst I: "C:\Users\Dell\Downloads\Instagram clone"
```

All commands below assume `I:` exists.

---

## 4. Quick start

### 4.1 Start MySQL

The Windows service cannot be started without admin rights, so a local instance
is configured on port 3307:

```powershell
& 'C:\Program Files\MySQL\MySQL Server 8.0\bin\mysqld.exe' `
  --defaults-file='C:/Users/Dell/Downloads/Instagram clone/database/runtime/my-local.ini' `
  --console
```

Apply the schema and demo data:

```powershell
Get-Content 'I:\database\schema.sql' -Raw |
  & 'C:\Program Files\MySQL\MySQL Server 8.0\bin\mysql.exe' -h 127.0.0.1 -P 3307 -u ig_app -pig_app_password

Get-Content 'I:\database\seed.sql' -Raw |
  & 'C:\Program Files\MySQL\MySQL Server 8.0\bin\mysql.exe' -h 127.0.0.1 -P 3307 -u ig_app -pig_app_password
```

### 4.2 Build the C++ backend

```powershell
$env:PATH = "C:\ig-mingw-real\bin;I:\.tools\cmake\cmake-3.31.6-windows-x86_64\bin;$env:PATH"

cmake -S I:\backend -B I:\build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_C_COMPILER=C:/ig-mingw-real/bin/gcc.exe `
  -DCMAKE_CXX_COMPILER=C:/ig-mingw-real/bin/g++.exe `
  -DCMAKE_MAKE_PROGRAM=C:/ig-mingw-real/bin/mingw32-make.exe

cmake --build I:\build -j 4
```

### 4.3 Start the backend

```powershell
$env:PATH = "C:\ig-mingw-real\bin;$env:PATH"
$env:IG_UPLOAD_DIR = 'I:\uploads'
I:\build\ig_server.exe
```

Expected output:

```
Connected to MySQL 127.0.0.1:3307 and rebuilt DSA indexes
Instagram clone backend listening on 127.0.0.1:8080
```

Health check: <http://127.0.0.1:8080/api/health>

### 4.4 Start the frontend

```powershell
$env:PATH = "I:\.tools\node\dist;$env:PATH"
cd I:\frontend
npm install --no-audit --no-fund   # only needed once
npm run dev
```

Open <http://127.0.0.1:5173>. Vite proxies `/api` and `/uploads` to the C++
server on port 8080.

Production build:

```powershell
npm run build
```

---

## 5. Demo accounts

Every demo account uses password **`Pass@123`**:

| Username | Notes |
|---|---|
| `alice` | Main demo user; has a pending follow request to `diana` |
| `bob` | Follows/alice interactions, messages, notifications |
| `charlie` | Additional feed/social data |
| `diana` | Private account; review follow requests as this user |
| `admin` | Verified demo account |

Login page has one-click buttons that fill these credentials.

---

## 6. Configuration

Backend environment variables (defaults shown):

| Variable | Default |
|---|---|
| `IG_HOST` | `127.0.0.1` |
| `IG_PORT` | `8080` |
| `IG_DB_HOST` | `127.0.0.1` |
| `IG_DB_PORT` | `3307` |
| `IG_DB_USER` | `ig_app` |
| `IG_DB_PASSWORD` | `ig_app_password` |
| `IG_DB_NAME` | `instagram_clone` |
| `IG_UPLOAD_DIR` | `uploads` |
| `IG_PUBLIC_URL` | `http://localhost:8080` |

Frontend proxy targets are configured in `frontend/vite.config.ts`.

---

## 7. Tests

### DSA unit tests

```powershell
$env:PATH = "C:\ig-mingw-real\bin;I:\build;$env:PATH"
I:\build\ig_dsa_tests.exe
```

Expected: `All DSA tests passed (105 assertions)`.

### Backend integration tests

With the server running:

```powershell
python I:\tests\test_api.py
```

Expected: `API integration tests passed (57 requests)`.

The API test creates temporary users/posts, exercises every feature area,
verifies password reset and token invalidation, then leaves the service running.
Re-apply `schema.sql` and `seed.sql` afterwards to restore the pristine demo
state.

### Frontend verification

`npm run build` type-checks with `tsc -b` and produces `frontend/dist/`.
The app was also exercised in a browser: login, feed modes, stories, explore,
reels, messages, notifications, search, profile, post detail, settings, dark
mode, create-post modal, media loading, protected-route redirects, the Settings
reset-password form, and the Profile block action. The final Phase 8 smoke run
reported no console errors.

---

## 8. Documentation

- [`docs/DSA.md`](docs/DSA.md) — custom structures, operation complexities,
  tests, and exactly where each structure is used.
- [`docs/API.md`](docs/API.md) — REST endpoint reference.
- [`docs/PHASES.md`](docs/PHASES.md) — implementation phases and what worked at
  the end of each phase.
- [`docs/PHASE3_REPORT.md`](docs/PHASE3_REPORT.md) — final Phase 3 (posts,
  likes and comments) implementation and verification report.
- [`PROGRESS.md`](PROGRESS.md) — current phase status and verification summary.

---

## 9. Security notes

- Passwords are hashed with **Argon2id**; no plaintext password is stored.
- Bearer tokens are CSPRNG-generated opaque strings. The database stores only
  a BLAKE2b-256 fingerprint, so a database leak cannot replay sessions.
- Logout deletes the session fingerprint; subsequent `/auth/me` returns 401.
- All SQL uses prepared statements; the connection pool returns move-only
  guards so connections cannot be accidentally leaked.
- Upload paths are confined to the uploads root; the static file handler uses
  the validated absolute path.
- Protected frontend routes redirect unauthenticated users to `/login`.

This is an academic demo: password-reset tokens are returned in the API
response because no email service is configured.

---

## 10. Troubleshooting

| Symptom | Fix |
|---|---|
| `I:` missing | `subst I: "C:\Users\Dell\Downloads\Instagram clone"` |
| Server exits with code `0xC000007B` | Add `C:\ig-mingw-real\bin` to `PATH` before launching |
| Link fails from the original path | Build from `I:\`, not the path containing spaces |
| Port 8080 refused | Start the C++ server; check `IG_PORT` |
| Port 3307 refused | Start the local `mysqld` with `database/runtime/my-local.ini` |
| Images 404 | Ensure `IG_UPLOAD_DIR=I:\uploads` and files exist in `uploads/` |
| Vite `/api` 404 or proxy error | Ensure backend is running and Vite proxy target is port 8080 |
| Browser shows a TS/JSX syntax error from Vite | Stop Vite, delete `frontend/node_modules/.vite`, restart `npm run dev` |

---

## 11. Project layout

```
backend/
  include/          headers (controllers, core, database, dsa, models, services)
  src/              implementations
  vendor/           Crow, nlohmann/json, Argon2/BLAKE2
frontend/
  src/components/   shared UI
  src/context/      AuthContext, ToastContext
  src/pages/        route pages
  src/lib/          API client, types, helpers
database/
  schema.sql        normalized MySQL schema
  seed.sql          reproducible demo data
  runtime/          local MySQL config
tests/
  test_dsa.cpp      DSA unit tests
  test_api.py       end-to-end REST tests
uploads/            demo media and runtime uploads
docs/               DSA, API and phase documentation
```
