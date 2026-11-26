#include "controllers/routes.h"

#include <filesystem>
#include <optional>

namespace ig::controllers {

static void addCors(crow::response& response) {
  response.set_header("Access-Control-Allow-Origin", "*");
  response.set_header("Access-Control-Allow-Headers",
                      "Content-Type, Authorization");
  response.set_header("Access-Control-Allow-Methods",
                      "GET, POST, PUT, DELETE, OPTIONS");
}

static crow::response respond(const ApiResult& result) {
  crow::response response(result.status, result.body.dump());
  response.set_header("Content-Type", "application/json; charset=utf-8");
  addCors(response);
  return response;
}

static crow::response badRequest(const std::string& message) {
  return respond(ApiResult::fail(400, message));
}

static json parseBody(const crow::request& request, bool* ok) {
  try {
    if (request.body.empty()) {
      *ok = true;
      return json::object();
    }
    *ok = true;
    return json::parse(request.body);
  } catch (...) {
    *ok = false;
    return json::object();
  }
}

static std::optional<long long> auth(const crow::request& request,
                                     AppService& service) {
  return service.authenticate(request.get_header_value("Authorization"));
}

static crow::response requireAuth(const crow::request& request,
                                  AppService& service, long long* userId) {
  auto id = auth(request, service);
  if (!id) return respond(ApiResult::fail(401, "Authentication required"));
  *userId = *id;
  return crow::response(0);  // sentinel: caller checks userId
}

static std::string query(const crow::request& request, const char* name,
                         const std::string& fallback = "") {
  const char* value = request.url_params.get(name);
  return value ? std::string(value) : fallback;
}

static int queryInt(const crow::request& request, const char* name,
                    int fallback) {
  const std::string value = query(request, name);
  if (value.empty()) return fallback;
  try {
    return std::stoi(value);
  } catch (...) {
    return fallback;
  }
}

void registerRoutes(crow::SimpleApp& app, AppContext& context,
                    AppService& service) {
  CROW_ROUTE(app, "/<path>")
      .methods(crow::HTTPMethod::Options)(
          [](const crow::request&, const std::string&) {
            crow::response response(204);
            addCors(response);
            return response;
          });

  CROW_ROUTE(app, "/api/health")([]() {
    json body;
    body["status"] = "ok";
    body["service"] = "instagram-clone-cpp";
    return respond(ApiResult::ok(body));
  });

  CROW_ROUTE(app, "/uploads/<path>")
      ([&context](const crow::request&, const std::string& path) {
        std::error_code ec;
        const std::filesystem::path root = std::filesystem::weakly_canonical(
            std::filesystem::absolute(context.config.uploadDir), ec);
        if (ec) {
          crow::response response(404);
          addCors(response);
          return response;
        }
        const std::filesystem::path target =
            std::filesystem::weakly_canonical(root / path, ec);
        const std::filesystem::path relative = target.lexically_relative(root);
        if (ec || relative.empty() || relative.is_absolute() ||
            *relative.begin() == ".." || !std::filesystem::exists(target) ||
            !std::filesystem::is_regular_file(target)) {
          crow::response response(404);
          addCors(response);
          return response;
        }
        crow::response response(200);
        // Path is already confined to the uploads root above; Crow's safe
        // sanitizer would rewrite the Windows drive colon, so use the unsafe
        // variant here after our own validation.
        response.set_static_file_info_unsafe(target.string());
        addCors(response);
        return response;
      });

  // ------------------------------------------------------------------ auth
  CROW_ROUTE(app, "/api/auth/register")
      .methods(crow::HTTPMethod::Post)([&service](const crow::request& request) {
        bool ok = false;
        json body = parseBody(request, &ok);
        if (!ok) return badRequest("Invalid JSON");
        return respond(service.registerUser(body));
      });

  CROW_ROUTE(app, "/api/auth/login")
      .methods(crow::HTTPMethod::Post)([&service](const crow::request& request) {
        bool ok = false;
        json body = parseBody(request, &ok);
        if (!ok) return badRequest("Invalid JSON");
        return respond(service.login(body));
      });

  CROW_ROUTE(app, "/api/auth/logout")
      .methods(crow::HTTPMethod::Post)([&service](const crow::request& request) {
        long long userId = 0;
        auto rejection = requireAuth(request, service, &userId);
        if (userId == 0) return rejection;
        const std::string token =
            request.get_header_value("Authorization").substr(7);
        return respond(service.logout(token));
      });

  CROW_ROUTE(app, "/api/auth/forgot-password")
      .methods(crow::HTTPMethod::Post)([&service](const crow::request& request) {
        bool ok = false;
        json body = parseBody(request, &ok);
        if (!ok) return badRequest("Invalid JSON");
        return respond(service.requestPasswordReset(body));
      });

  CROW_ROUTE(app, "/api/auth/reset-password")
      .methods(crow::HTTPMethod::Post)([&service](const crow::request& request) {
        bool ok = false;
        json body = parseBody(request, &ok);
        if (!ok) return badRequest("Invalid JSON");
        return respond(service.resetPassword(body));
      });

  CROW_ROUTE(app, "/api/auth/me")([&service](const crow::request& request) {
    long long userId = 0;
    auto rejection = requireAuth(request, service, &userId);
    if (userId == 0) return rejection;
    return respond(service.getUser(userId, userId));
  });

  // ----------------------------------------------------------------- users
  CROW_ROUTE(app, "/api/users/search")([&service](const crow::request& request) {
    long long userId = 0;
    auto rejection = requireAuth(request, service, &userId);
    if (userId == 0) return rejection;
    return respond(service.searchUsers(query(request, "q"), userId));
  });

  CROW_ROUTE(app, "/api/users/suggested")([&service](const crow::request& request) {
    long long userId = 0;
    auto rejection = requireAuth(request, service, &userId);
    if (userId == 0) return rejection;
    return respond(service.suggestedUsers(userId));
  });

  CROW_ROUTE(app, "/api/users/<uint>/suggestions")(
      [&service](const crow::request& request, std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        return respond(service.suggestedUsers(static_cast<long long>(rawId), viewer));
      });

  CROW_ROUTE(app, "/api/users/requests")([&service](const crow::request& request) {
    long long userId = 0;
    auto rejection = requireAuth(request, service, &userId);
    if (userId == 0) return rejection;
    return respond(service.listRequests(userId));
  });

  CROW_ROUTE(app, "/api/users/blocked")([&service](const crow::request& request) {
    long long userId = 0;
    auto rejection = requireAuth(request, service, &userId);
    if (userId == 0) return rejection;
    return respond(service.listBlocked(userId));
  });

  CROW_ROUTE(app, "/api/users/report")
      .methods(crow::HTTPMethod::Post)([&service](const crow::request& request) {
        long long userId = 0;
        auto rejection = requireAuth(request, service, &userId);
        if (userId == 0) return rejection;
        bool ok = false;
        json body = parseBody(request, &ok);
        if (!ok) return badRequest("Invalid JSON");
        return respond(service.report(body, userId));
      });

  CROW_ROUTE(app, "/api/users/<uint>")([&service](const crow::request& request,
                                                  std::uint64_t rawId) {
    long long viewer = 0;
    auto rejection = requireAuth(request, service, &viewer);
    if (viewer == 0) return rejection;
    return respond(service.getUser(static_cast<long long>(rawId), viewer));
  });

  CROW_ROUTE(app, "/api/users/<uint>")
      .methods(crow::HTTPMethod::Put)([&service](const crow::request& request,
                                                 std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        bool ok = false;
        json body = parseBody(request, &ok);
        if (!ok) return badRequest("Invalid JSON");
        return respond(service.updateUser(static_cast<long long>(rawId), viewer,
                                          body));
      });

  CROW_ROUTE(app, "/api/users/<uint>/follow")
      .methods(crow::HTTPMethod::Post)([&service](const crow::request& request,
                                                  std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        return respond(service.follow(static_cast<long long>(rawId), viewer));
      });

  CROW_ROUTE(app, "/api/users/<uint>/follow")
      .methods(crow::HTTPMethod::Delete)([&service](const crow::request& request,
                                                    std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        return respond(service.unfollow(static_cast<long long>(rawId), viewer));
      });

  CROW_ROUTE(app, "/api/users/<uint>/follow-status")(
      [&service](const crow::request& request, std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        return respond(service.followStatus(static_cast<long long>(rawId), viewer));
      });

  CROW_ROUTE(app, "/api/users/<uint>/followers")(
      [&service](const crow::request& request, std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        return respond(service.listFollowers(static_cast<long long>(rawId), viewer));
      });

  CROW_ROUTE(app, "/api/users/<uint>/following")(
      [&service](const crow::request& request, std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        return respond(service.listFollowing(static_cast<long long>(rawId), viewer));
      });

  CROW_ROUTE(app, "/api/users/<uint>/mutual")(
      [&service](const crow::request& request, std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        return respond(service.mutualFollowers(static_cast<long long>(rawId), viewer));
      });

  CROW_ROUTE(app, "/api/users/<uint>/block")
      .methods(crow::HTTPMethod::Post)([&service](const crow::request& request,
                                                  std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        return respond(service.block(static_cast<long long>(rawId), viewer));
      });

  CROW_ROUTE(app, "/api/users/<uint>/block")
      .methods(crow::HTTPMethod::Delete)([&service](const crow::request& request,
                                                    std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        return respond(service.unblock(static_cast<long long>(rawId), viewer));
      });

  CROW_ROUTE(app, "/api/users/requests/<uint>")
      .methods(crow::HTTPMethod::Post)([&service](const crow::request& request,
                                                  std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        bool ok = false;
        json body = parseBody(request, &ok);
        if (!ok) return badRequest("Invalid JSON");
        const bool accept = body.value("accept", true);
        return respond(service.respondToRequest(static_cast<long long>(rawId),
                                                viewer, accept));
      });

  // ------------------------------------------------------------------ posts
  CROW_ROUTE(app, "/api/posts")
      .methods(crow::HTTPMethod::Post)([&service](const crow::request& request) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        bool ok = false;
        json body = parseBody(request, &ok);
        if (!ok) return badRequest("Invalid JSON");
        return respond(service.createPost(viewer, body));
      });

  CROW_ROUTE(app, "/api/posts")([&service](const crow::request& request) {
    long long viewer = 0;
    auto rejection = requireAuth(request, service, &viewer);
    if (viewer == 0) return rejection;
    const std::string userId = query(request, "userId");
    if (!userId.empty()) {
      try {
        return respond(service.getPostsByUser(std::stoll(userId), viewer,
                                              queryInt(request, "page", 1),
                                              queryInt(request, "limit", 12)));
      } catch (...) {
        return badRequest("Invalid userId");
      }
    }
    return respond(service.getFeed(viewer, queryInt(request, "page", 1),
                                   queryInt(request, "limit", 12), "latest"));
  });

  CROW_ROUTE(app, "/api/posts/saved")([&service](const crow::request& request) {
    long long viewer = 0;
    auto rejection = requireAuth(request, service, &viewer);
    if (viewer == 0) return rejection;
    return respond(service.savedPosts(viewer));
  });

  CROW_ROUTE(app, "/api/posts/<uint>")(
      [&service](const crow::request& request, std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        return respond(service.getPost(static_cast<long long>(rawId), viewer));
      });

  CROW_ROUTE(app, "/api/posts/<uint>")
      .methods(crow::HTTPMethod::Put)([&service](const crow::request& request,
                                                 std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        bool ok = false;
        json body = parseBody(request, &ok);
        if (!ok) return badRequest("Invalid JSON");
        return respond(service.updatePost(static_cast<long long>(rawId), viewer,
                                          body));
      });

  CROW_ROUTE(app, "/api/posts/<uint>")
      .methods(crow::HTTPMethod::Delete)([&service](const crow::request& request,
                                                    std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        return respond(service.deletePost(static_cast<long long>(rawId), viewer));
      });

  CROW_ROUTE(app, "/api/posts/<uint>/like")
      .methods(crow::HTTPMethod::Post)([&service](const crow::request& request,
                                                  std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        return respond(service.likePost(static_cast<long long>(rawId), viewer));
      });

  CROW_ROUTE(app, "/api/posts/<uint>/like")
      .methods(crow::HTTPMethod::Delete)([&service](const crow::request& request,
                                                    std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        return respond(service.unlikePost(static_cast<long long>(rawId), viewer));
      });

  CROW_ROUTE(app, "/api/posts/<uint>/save")
      .methods(crow::HTTPMethod::Post)([&service](const crow::request& request,
                                                  std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        return respond(service.savePost(static_cast<long long>(rawId), viewer));
      });

  CROW_ROUTE(app, "/api/posts/<uint>/save")
      .methods(crow::HTTPMethod::Delete)([&service](const crow::request& request,
                                                    std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        return respond(service.unsavePost(static_cast<long long>(rawId), viewer));
      });

  CROW_ROUTE(app, "/api/posts/<uint>/comments")
      .methods(crow::HTTPMethod::Post)([&service](const crow::request& request,
                                                  std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        bool ok = false;
        json body = parseBody(request, &ok);
        if (!ok) return badRequest("Invalid JSON");
        return respond(service.addComment(static_cast<long long>(rawId), viewer,
                                          body));
      });

  CROW_ROUTE(app, "/api/posts/<uint>/comments")(
      [&service](const crow::request& request, std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        return respond(service.getComments(static_cast<long long>(rawId), viewer));
      });

  CROW_ROUTE(app, "/api/comments/<uint>")
      .methods(crow::HTTPMethod::Delete)([&service](const crow::request& request,
                                                    std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        return respond(service.deleteComment(static_cast<long long>(rawId), viewer));
      });

  // ------------------------------------------------------------ feed/search
  CROW_ROUTE(app, "/api/feed")([&service](const crow::request& request) {
    long long viewer = 0;
    auto rejection = requireAuth(request, service, &viewer);
    if (viewer == 0) return rejection;
    return respond(service.getFeed(viewer, queryInt(request, "page", 1),
                                   queryInt(request, "limit", 10),
                                   query(request, "mode", "following")));
  });

  CROW_ROUTE(app, "/api/explore")([&service](const crow::request& request) {
    long long viewer = 0;
    auto rejection = requireAuth(request, service, &viewer);
    if (viewer == 0) return rejection;
    return respond(service.explore(viewer, query(request, "tag"),
                                   queryInt(request, "page", 1),
                                   queryInt(request, "limit", 12)));
  });

  CROW_ROUTE(app, "/api/hashtags/trending")(
      [&service](const crow::request& request) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        return respond(service.trendingHashtags());
      });

  CROW_ROUTE(app, "/api/search")([&service](const crow::request& request) {
    long long viewer = 0;
    auto rejection = requireAuth(request, service, &viewer);
    if (viewer == 0) return rejection;
    return respond(service.search(query(request, "q"), viewer));
  });

  CROW_ROUTE(app, "/api/search/recent")([&service](const crow::request& request) {
    long long viewer = 0;
    auto rejection = requireAuth(request, service, &viewer);
    if (viewer == 0) return rejection;
    return respond(service.recentSearches(viewer));
  });

  // ---------------------------------------------------------------- stories
  CROW_ROUTE(app, "/api/stories")
      .methods(crow::HTTPMethod::Post)([&service](const crow::request& request) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        bool ok = false;
        json body = parseBody(request, &ok);
        if (!ok) return badRequest("Invalid JSON");
        return respond(service.createStory(viewer, body));
      });

  CROW_ROUTE(app, "/api/stories")([&service](const crow::request& request) {
    long long viewer = 0;
    auto rejection = requireAuth(request, service, &viewer);
    if (viewer == 0) return rejection;
    return respond(service.getStories(viewer));
  });

  CROW_ROUTE(app, "/api/stories/<uint>/view")
      .methods(crow::HTTPMethod::Post)([&service](const crow::request& request,
                                                  std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        return respond(service.viewStory(static_cast<long long>(rawId), viewer));
      });

  // ------------------------------------------------------------------ reels
  CROW_ROUTE(app, "/api/reels")
      .methods(crow::HTTPMethod::Post)([&service](const crow::request& request) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        bool ok = false;
        json body = parseBody(request, &ok);
        if (!ok) return badRequest("Invalid JSON");
        return respond(service.createReel(viewer, body));
      });

  CROW_ROUTE(app, "/api/reels")([&service](const crow::request& request) {
    long long viewer = 0;
    auto rejection = requireAuth(request, service, &viewer);
    if (viewer == 0) return rejection;
    return respond(service.getReels(viewer, queryInt(request, "page", 1),
                                    queryInt(request, "limit", 8),
                                    query(request, "mode", "latest")));
  });

  CROW_ROUTE(app, "/api/reels/saved")([&service](const crow::request& request) {
    long long viewer = 0;
    auto rejection = requireAuth(request, service, &viewer);
    if (viewer == 0) return rejection;
    return respond(service.savedReels(viewer));
  });

  CROW_ROUTE(app, "/api/reels/<uint>/like")
      .methods(crow::HTTPMethod::Post)([&service](const crow::request& request,
                                                  std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        return respond(service.likeReel(static_cast<long long>(rawId), viewer));
      });

  CROW_ROUTE(app, "/api/reels/<uint>/like")
      .methods(crow::HTTPMethod::Delete)([&service](const crow::request& request,
                                                    std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        return respond(service.unlikeReel(static_cast<long long>(rawId), viewer));
      });

  CROW_ROUTE(app, "/api/reels/<uint>/save")
      .methods(crow::HTTPMethod::Post)([&service](const crow::request& request,
                                                  std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        return respond(service.saveReel(static_cast<long long>(rawId), viewer));
      });

  CROW_ROUTE(app, "/api/reels/<uint>/save")
      .methods(crow::HTTPMethod::Delete)([&service](const crow::request& request,
                                                    std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        return respond(service.unsaveReel(static_cast<long long>(rawId), viewer));
      });

  CROW_ROUTE(app, "/api/reels/<uint>/view")
      .methods(crow::HTTPMethod::Post)([&service](const crow::request& request,
                                                  std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        return respond(service.viewReel(static_cast<long long>(rawId), viewer));
      });

  CROW_ROUTE(app, "/api/reels/<uint>/comments")
      .methods(crow::HTTPMethod::Post)([&service](const crow::request& request,
                                                  std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        bool ok = false;
        json body = parseBody(request, &ok);
        if (!ok) return badRequest("Invalid JSON");
        return respond(service.commentReel(static_cast<long long>(rawId), viewer,
                                           body));
      });

  CROW_ROUTE(app, "/api/reels/<uint>/comments")(
      [&service](const crow::request& request, std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        return respond(service.getReelComments(static_cast<long long>(rawId), viewer));
      });

  // --------------------------------------------------------------- messages
  CROW_ROUTE(app, "/api/messages/conversations")(
      [&service](const crow::request& request) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        const std::string q = query(request, "q");
        if (!q.empty()) return respond(service.searchConversations(viewer, q));
        return respond(service.listConversations(viewer));
      });

  CROW_ROUTE(app, "/api/messages/unread")(
      [&service](const crow::request& request) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        return respond(service.unreadConversationCount(viewer));
      });

  CROW_ROUTE(app, "/api/messages/conversation/<uint>")
      .methods(crow::HTTPMethod::Post)([&service](const crow::request& request,
                                                  std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        bool ok = false;
        json body = parseBody(request, &ok);
        if (!ok) return badRequest("Invalid JSON");
        const long long recipient = body.value("recipientId", 0LL);
        if (recipient == 0) return badRequest("recipientId required");
        return respond(service.getOrCreateConversation(recipient, viewer));
      });

  CROW_ROUTE(app, "/api/messages/conversation/<uint>")(
      [&service](const crow::request& request, std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        return respond(service.getMessages(static_cast<long long>(rawId), viewer,
                                           queryInt(request, "before", 0),
                                           queryInt(request, "limit", 50)));
      });

  CROW_ROUTE(app, "/api/messages/<uint>")(
      [&service](const crow::request& request, std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        return respond(service.getMessages(static_cast<long long>(rawId), viewer,
                                           queryInt(request, "before", 0),
                                           queryInt(request, "limit", 50)));
      });

  CROW_ROUTE(app, "/api/messages")
      .methods(crow::HTTPMethod::Post)([&service](const crow::request& request) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        bool ok = false;
        json body = parseBody(request, &ok);
        if (!ok) return badRequest("Invalid JSON");
        return respond(service.sendMessage(viewer, body));
      });

  CROW_ROUTE(app, "/api/messages/<uint>/read")
      .methods(crow::HTTPMethod::Post)([&service](const crow::request& request,
                                                  std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        return respond(service.markConversationRead(static_cast<long long>(rawId),
                                                    viewer));
      });

  CROW_ROUTE(app, "/api/messages/<uint>/typing")
      .methods(crow::HTTPMethod::Post)([&service](const crow::request& request,
                                                  std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        bool ok = false;
        json body = parseBody(request, &ok);
        if (!ok) return badRequest("Invalid JSON");
        return respond(service.setTyping(static_cast<long long>(rawId), viewer,
                                         body.value("typing", true)));
      });

  // ---------------------------------------------------------- notifications
  CROW_ROUTE(app, "/api/notifications")([&service](const crow::request& request) {
    long long viewer = 0;
    auto rejection = requireAuth(request, service, &viewer);
    if (viewer == 0) return rejection;
    return respond(service.getNotifications(viewer));
  });

  CROW_ROUTE(app, "/api/notifications/unread")(
      [&service](const crow::request& request) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        return respond(service.unreadNotifications(viewer));
      });

  CROW_ROUTE(app, "/api/notifications/read-all")
      .methods(crow::HTTPMethod::Post)([&service](const crow::request& request) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        return respond(service.markAllNotificationsRead(viewer));
      });

  CROW_ROUTE(app, "/api/notifications/<uint>/read")
      .methods(crow::HTTPMethod::Post)([&service](const crow::request& request,
                                                  std::uint64_t rawId) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        return respond(service.markNotificationRead(static_cast<long long>(rawId),
                                                    viewer));
      });

  // ------------------------------------------------------------------ media
  CROW_ROUTE(app, "/api/upload")
      .methods(crow::HTTPMethod::Post)([&service](const crow::request& request) {
        long long viewer = 0;
        auto rejection = requireAuth(request, service, &viewer);
        if (viewer == 0) return rejection;
        bool ok = false;
        json body = parseBody(request, &ok);
        if (!ok) return badRequest("Invalid JSON");
        return respond(service.uploadMedia(viewer, body));
      });
}

}  // namespace ig::controllers
