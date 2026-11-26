#include "services/app_service.h"

#include <algorithm>

namespace ig {

ApiResult AppService::getUser(long long id, long long viewer) {
  User user;
  if (!userById(id, &user)) return ApiResult::fail(404, "User not found");
  if (isBlocked(id, viewer) && viewer != id) {
    return ApiResult::fail(403, "Profile unavailable");
  }

  auto count = [&](const std::string& table, const std::string& column) {
    std::vector<Row> rows;
    ctx_.db.query("SELECT COUNT(*) AS n FROM " + table + " WHERE " + column +
                      " = ?",
                  {Param::of(id)}, &rows, nullptr);
    return rows.empty() ? 0LL : rows[0].integer("n");
  };

  json profile = publicUser(user);
  profile["postsCount"] = count("posts", "user_id");
  profile["followersCount"] = count("followers", "following_id");
  profile["followingCount"] = count("followers", "follower_id");
  profile["isFollowing"] = isFollowing(viewer, id);
  profile["isOwnProfile"] = (viewer == id);

  std::vector<Row> request;
  ctx_.db.query("SELECT status FROM follow_requests WHERE requester_id = ? "
                "AND target_id = ?",
                {Param::of(viewer), Param::of(id)}, &request, nullptr);
  profile["followStatus"] =
      request.empty() ? "none" : request[0].str("status", "none");

  if (viewer != id) {
    auto mutual = ctx_.graph.mutualFollows(viewer, id);
    profile["mutualCount"] = static_cast<long long>(mutual.size());
  } else {
    profile["mutualCount"] = 0;
  }
  return ApiResult::ok(profile);
}

ApiResult AppService::updateUser(long long id, long long viewer,
                                 const json& payload) {
  if (id != viewer) return ApiResult::fail(403, "Not authorised");
  User user;
  if (!userById(id, &user)) return ApiResult::fail(404, "User not found");

  const std::string fullName = payload.value("fullName", user.fullName);
  const std::string bio = payload.value("bio", user.bio);
  const std::string website = payload.value("website", user.website);
  const std::string picture = payload.value("profilePicture", user.profilePicture);
  const bool isPrivate = payload.value("isPrivate", user.isPrivate);
  if (fullName.size() > 100 || bio.size() > 500 || website.size() > 255) {
    return ApiResult::fail(400, "Field exceeds maximum length");
  }

  std::string error;
  if (!ctx_.db.execute("UPDATE users SET full_name = ?, bio = ?, website = ?, "
                       "profile_picture = ?, is_private = ? WHERE id = ?",
                       {Param::of(fullName), Param::of(bio), Param::of(website),
                        Param::of(picture), Param::of(isPrivate ? 1 : 0),
                        Param::of(id)},
                       &error, nullptr)) {
    return ApiResult::fail(500, error);
  }
  if (payload.contains("email")) {
    const std::string email = payload.value("email", "");
    if (email.find('@') == std::string::npos) {
      return ApiResult::fail(400, "Invalid email");
    }
    ctx_.db.execute("UPDATE users SET email = ? WHERE id = ?",
                    {Param::of(email), Param::of(id)}, nullptr);
  }
  userById(id, &user);
  return ApiResult::ok(publicUser(user));
}

ApiResult AppService::searchUsers(const std::string& query, long long viewer) {
  if (query.size() < 1) return ApiResult::ok({{"results", json::array()}});

  std::vector<std::string> names;
  {
    std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
    names = ctx_.usernameTrie.startsWith(query, 20);
  }
  // Also match full names / emails with a bounded prepared LIKE query.
  std::vector<Row> rows;
  ctx_.db.query("SELECT id, username FROM users WHERE username LIKE ? OR "
                "full_name LIKE ? LIMIT 20",
                {Param::of(query + "%"), Param::of("%" + query + "%")}, &rows,
                nullptr);
  for (const Row& row : rows) names.push_back(row.str("username"));

  std::sort(names.begin(), names.end());
  names.erase(std::unique(names.begin(), names.end()), names.end());

  json results = json::array();
  for (const std::string& name : names) {
    auto id = ctx_.userIdFor(name);
    if (!id || *id == viewer || isBlocked(*id, viewer)) continue;
    results.push_back(publicUserById(*id));
    if (results.size() >= 20) break;
  }
  return ApiResult::ok({{"results", results}});
}

ApiResult AppService::follow(long long target, long long viewer) {
  if (target == viewer) return ApiResult::fail(400, "Cannot follow yourself");
  User user;
  if (!userById(target, &user)) return ApiResult::fail(404, "User not found");
  if (isBlocked(target, viewer)) return ApiResult::fail(403, "Blocked user");

  // MySQL is the source of truth. Check it before writing so repeated clicks
  // are idempotent and never create duplicate notifications.
  std::vector<Row> existing;
  ctx_.db.query("SELECT 1 AS x FROM followers WHERE follower_id = ? AND "
                "following_id = ?",
                {Param::of(viewer), Param::of(target)}, &existing, nullptr);
  if (!existing.empty()) {
    return ApiResult::ok({{"status", "following"}, {"alreadyFollowing", true}});
  }

  if (user.isPrivate) {
    std::vector<Row> pending;
    ctx_.db.query("SELECT status FROM follow_requests WHERE requester_id = ? "
                  "AND target_id = ?",
                  {Param::of(viewer), Param::of(target)}, &pending, nullptr);
    if (!pending.empty() && pending[0].str("status", "") == "pending") {
      return ApiResult::ok({{"status", "requested"}, {"alreadyRequested", true}});
    }
    std::string error;
    if (!ctx_.db.execute("INSERT INTO follow_requests (requester_id, target_id, status) "
                         "VALUES (?, ?, 'pending') ON DUPLICATE KEY UPDATE "
                         "status = 'pending', created_at = CURRENT_TIMESTAMP",
                         {Param::of(viewer), Param::of(target)}, &error, nullptr)) {
      return ApiResult::fail(500, error);
    }
    notify(target, viewer, "follow_request", viewer,
           ctx_.usernameFor(viewer) + " requested to follow you");
    return ApiResult::ok({{"status", "requested"}});
  }

  std::string error;
  if (!ctx_.db.execute("INSERT INTO followers (follower_id, following_id) "
                       "VALUES (?, ?)",
                       {Param::of(viewer), Param::of(target)}, &error, nullptr)) {
    return ApiResult::fail(500, error);
  }
  ctx_.db.execute("UPDATE follow_requests SET status = 'accepted' WHERE "
                  "requester_id = ? AND target_id = ?",
                  {Param::of(viewer), Param::of(target)}, nullptr);
  {
    std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
    ctx_.graph.addFollow(viewer, target);
  }
  notify(target, viewer, "follow", viewer,
         ctx_.usernameFor(viewer) + " started following you");
  return ApiResult::ok({{"status", "following"}});
}

ApiResult AppService::unfollow(long long target, long long viewer) {
  if (target == viewer) return ApiResult::fail(400, "Cannot unfollow yourself");
  User user;
  if (!userById(target, &user)) return ApiResult::fail(404, "User not found");

  std::string error;
  if (!ctx_.db.execute("DELETE FROM followers WHERE follower_id = ? AND "
                       "following_id = ?",
                       {Param::of(viewer), Param::of(target)}, &error, nullptr)) {
    return ApiResult::fail(500, error);
  }
  ctx_.db.execute("DELETE FROM follow_requests WHERE requester_id = ? AND "
                  "target_id = ?",
                  {Param::of(viewer), Param::of(target)}, nullptr);
  {
    std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
    ctx_.graph.removeFollow(viewer, target);
  }
  return ApiResult::ok({{"status", "unfollowed"}});
}

ApiResult AppService::followStatus(long long target, long long viewer) {
  User user;
  if (!userById(target, &user)) return ApiResult::fail(404, "User not found");
  if (isBlocked(target, viewer) && viewer != target) {
    return ApiResult::fail(403, "Profile unavailable");
  }

  auto count = [&](const std::string& column) {
    std::vector<Row> rows;
    ctx_.db.query("SELECT COUNT(*) AS n FROM followers WHERE " + column + " = ?",
                  {Param::of(target)}, &rows, nullptr);
    return rows.empty() ? 0LL : rows[0].integer("n");
  };

  std::vector<Row> request;
  ctx_.db.query("SELECT status FROM follow_requests WHERE requester_id = ? "
                "AND target_id = ?",
                {Param::of(viewer), Param::of(target)}, &request, nullptr);

  std::size_t mutual = 0;
  if (viewer != target) {
    std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
    mutual = ctx_.graph.mutualFollows(viewer, target).size();
  }

  return ApiResult::ok({{"targetId", target},
                        {"isFollowing", isFollowing(viewer, target)},
                        {"followStatus", request.empty()
                                             ? "none"
                                             : request[0].str("status", "none")},
                        {"followersCount", count("following_id")},
                        {"followingCount", count("follower_id")},
                        {"mutualCount", static_cast<long long>(mutual)},
                        {"isOwnProfile", viewer == target}});
}

static json userListJson(const std::vector<Row>& rows, const std::string& column) {
  json result = json::array();
  for (const Row& row : rows) result.push_back(row.integer(column));
  return result;
}

ApiResult AppService::listFollowers(long long id, long long viewer) {
  User user;
  if (!userById(id, &user)) return ApiResult::fail(404, "User not found");
  std::vector<Row> rows;
  ctx_.db.query("SELECT follower_id FROM followers WHERE following_id = ? "
                "ORDER BY created_at DESC",
                {Param::of(id)}, &rows, nullptr);
  json ids = userListJson(rows, "follower_id");
  json users = json::array();
  for (const auto& value : ids) {
    long long uid = value.get<long long>();
    if (!isBlocked(uid, viewer)) users.push_back(publicUserById(uid));
  }
  return ApiResult::ok({{"users", users}});
}

ApiResult AppService::listFollowing(long long id, long long viewer) {
  User user;
  if (!userById(id, &user)) return ApiResult::fail(404, "User not found");
  std::vector<Row> rows;
  ctx_.db.query("SELECT following_id FROM followers WHERE follower_id = ? "
                "ORDER BY created_at DESC",
                {Param::of(id)}, &rows, nullptr);
  json users = json::array();
  for (const Row& row : rows) {
    long long uid = row.integer("following_id");
    if (!isBlocked(uid, viewer)) users.push_back(publicUserById(uid));
  }
  return ApiResult::ok({{"users", users}});
}

ApiResult AppService::listRequests(long long viewer) {
  std::vector<Row> rows;
  ctx_.db.query("SELECT requester_id FROM follow_requests WHERE target_id = ? "
                "AND status = 'pending' ORDER BY created_at DESC",
                {Param::of(viewer)}, &rows, nullptr);
  json users = json::array();
  for (const Row& row : rows) users.push_back(publicUserById(row.integer("requester_id")));
  return ApiResult::ok({{"requests", users}});
}

ApiResult AppService::respondToRequest(long long requester, long long viewer,
                                       bool accept) {
  std::vector<Row> rows;
  ctx_.db.query("SELECT 1 AS x FROM follow_requests WHERE requester_id = ? AND "
                "target_id = ? AND status = 'pending'",
                {Param::of(requester), Param::of(viewer)}, &rows, nullptr);
  if (rows.empty()) return ApiResult::fail(404, "Follow request not found");

  ctx_.db.execute("UPDATE follow_requests SET status = ? WHERE requester_id = ? "
                  "AND target_id = ?",
                  {Param::of(accept ? "accepted" : "rejected"),
                   Param::of(requester), Param::of(viewer)},
                  nullptr);
  if (accept) {
    ctx_.db.execute("INSERT IGNORE INTO followers (follower_id, following_id) "
                    "VALUES (?, ?)",
                    {Param::of(requester), Param::of(viewer)}, nullptr);
    {
      std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
      ctx_.graph.addEdge(requester, viewer);
    }
    notify(requester, viewer, "follow", viewer,
           ctx_.usernameFor(viewer) + " accepted your follow request");
  }
  return ApiResult::ok({{"status", accept ? "accepted" : "rejected"}});
}

ApiResult AppService::block(long long target, long long viewer) {
  if (target == viewer) return ApiResult::fail(400, "Cannot block yourself");
  ctx_.db.execute("INSERT IGNORE INTO blocked_users (blocker_id, blocked_id) "
                  "VALUES (?, ?)",
                  {Param::of(viewer), Param::of(target)}, nullptr);
  ctx_.db.execute("DELETE FROM followers WHERE (follower_id = ? AND following_id = ?) "
                  "OR (follower_id = ? AND following_id = ?)",
                  {Param::of(viewer), Param::of(target), Param::of(target),
                   Param::of(viewer)},
                  nullptr);
  ctx_.db.execute("DELETE FROM follow_requests WHERE (requester_id = ? AND target_id = ?) "
                  "OR (requester_id = ? AND target_id = ?)",
                  {Param::of(viewer), Param::of(target), Param::of(target),
                   Param::of(viewer)},
                  nullptr);
  {
    std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
    ctx_.graph.removeEdge(viewer, target);
    ctx_.graph.removeEdge(target, viewer);
  }
  return ApiResult::ok({{"status", "blocked"}});
}

ApiResult AppService::unblock(long long target, long long viewer) {
  ctx_.db.execute("DELETE FROM blocked_users WHERE blocker_id = ? AND blocked_id = ?",
                  {Param::of(viewer), Param::of(target)}, nullptr);
  return ApiResult::ok({{"status", "unblocked"}});
}

ApiResult AppService::listBlocked(long long viewer) {
  std::vector<Row> rows;
  ctx_.db.query("SELECT blocked_id FROM blocked_users WHERE blocker_id = ?",
                {Param::of(viewer)}, &rows, nullptr);
  json users = json::array();
  for (const Row& row : rows) users.push_back(publicUserById(row.integer("blocked_id")));
  return ApiResult::ok({{"users", users}});
}

ApiResult AppService::report(const json& payload, long long viewer) {
  const long long target = payload.value("userId", 0LL);
  const long long postId = payload.value("postId", 0LL);
  const std::string reason = payload.value("reason", "");
  const std::string details = payload.value("details", "");
  if (target <= 0 && postId <= 0) {
    return ApiResult::fail(400, "Report requires a user or post");
  }
  if (reason.empty()) return ApiResult::fail(400, "Reason is required");
  ctx_.db.execute("INSERT INTO reports (reporter_id, target_user_id, post_id, "
                  "reason, details) VALUES (?, ?, ?, ?, ?)",
                  {Param::of(viewer),
                   target > 0 ? Param::of(target) : Param::null(),
                   postId > 0 ? Param::of(postId) : Param::null(),
                   Param::of(reason.substr(0, 60)), Param::of(details.substr(0, 2000))},
                  nullptr);
  return ApiResult::created({{"message", "Report submitted"}});
}

ApiResult AppService::mutualFollowers(long long other, long long viewer) {
  User user;
  if (!userById(other, &user)) return ApiResult::fail(404, "User not found");
  std::vector<long long> ids;
  {
    std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
    ids = ctx_.graph.mutualFollows(viewer, other);
  }
  json users = json::array();
  for (long long id : ids) users.push_back(publicUserById(id));
  return ApiResult::ok({{"users", users}});
}

ApiResult AppService::suggestedUsers(long long viewer) {
  return suggestedUsers(viewer, viewer);
}

ApiResult AppService::suggestedUsers(long long target, long long viewer) {
  // `target` is validated because it is part of the URL, but suggestions are
  // always generated for the authenticated viewer, never for a trusted path ID.
  User targetUser;
  if (!userById(target, &targetUser)) return ApiResult::fail(404, "User not found");

  std::vector<long long> ids;
  {
    std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
    ids = ctx_.graph.suggestions(viewer, 12);
  }

  json users = json::array();
  for (long long id : ids) {
    if (id == viewer || isBlocked(id, viewer) || isFollowing(viewer, id)) continue;
    std::vector<Row> pending;
    ctx_.db.query("SELECT 1 AS x FROM follow_requests WHERE requester_id = ? "
                  "AND target_id = ? AND status = 'pending'",
                  {Param::of(viewer), Param::of(id)}, &pending, nullptr);
    if (!pending.empty()) continue;  // private accounts support follow requests
    json user = publicUserById(id);
    if (!user.empty()) users.push_back(user);
    if (users.size() >= 12) break;
  }

  if (users.empty()) {
    // Fallback for a new/isolated graph: newest accounts, still filtered by the
    // same self/following/pending/blocked rules. Private accounts are allowed
    // because the app implements the follow-request flow for them.
    std::vector<Row> rows;
    ctx_.db.query("SELECT id FROM users WHERE id != ? "
                  "AND id NOT IN (SELECT following_id FROM followers WHERE follower_id = ?) "
                  "AND id NOT IN (SELECT target_id FROM follow_requests "
                  "WHERE requester_id = ? AND status = 'pending') "
                  "ORDER BY created_at DESC LIMIT 12",
                  {Param::of(viewer), Param::of(viewer), Param::of(viewer)},
                  &rows, nullptr);
    for (const Row& row : rows) {
      long long id = row.integer("id");
      if (isBlocked(id, viewer)) continue;
      json user = publicUserById(id);
      if (!user.empty()) users.push_back(user);
    }
  }
  return ApiResult::ok({{"users", users}});
}

}  // namespace ig
