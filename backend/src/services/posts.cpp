#include "services/app_service.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <unordered_map>

namespace ig {

static std::string trimCopy(const std::string& value) {
  std::size_t first = 0;
  while (first < value.size() &&
         std::isspace(static_cast<unsigned char>(value[first]))) {
    ++first;
  }
  std::size_t last = value.size();
  while (last > first &&
         std::isspace(static_cast<unsigned char>(value[last - 1]))) {
    --last;
  }
  return value.substr(first, last - first);
}

static double hoursSince(const std::string& timestamp) {
  // MySQL DATETIME: YYYY-MM-DD HH:MM:SS (UTC on this local instance).
  if (timestamp.size() < 19) return 0.0;
  std::tm tm{};
  std::istringstream ss(timestamp.substr(0, 19));
  ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
  if (ss.fail()) return 0.0;
  const std::time_t then = std::mktime(&tm);
  const std::time_t now = std::time(nullptr);
  return std::max(0.0, static_cast<double>(now - then) / 3600.0);
}

ApiResult AppService::createPost(long long viewer, const json& payload) {
  if (!payload.contains("media") || !payload["media"].is_array() ||
      payload["media"].empty()) {
    return ApiResult::fail(400, "At least one image is required");
  }
  if (payload["media"].size() > 10) {
    return ApiResult::fail(400, "A post can contain at most 10 media items");
  }
  const std::string caption = payload.value("caption", "");
  const std::string location = payload.value("location", "");
  if (caption.size() > 5000) return ApiResult::fail(400, "Caption is too long");
  if (location.size() > 150) {
    return ApiResult::fail(400, "Location is too long");
  }

  std::uint64_t postId = 0;
  std::string error;
  if (!ctx_.db.execute("INSERT INTO posts (user_id, caption, location) VALUES (?, ?, ?)",
                       {Param::of(viewer), Param::of(caption),
                        Param::of(location.substr(0, 150))},
                       &error, &postId)) {
    return ApiResult::fail(500, error);
  }

  int position = 0;
  for (const auto& item : payload["media"]) {
    const std::string data = item.value("data", "");
    const std::string filename = item.value("filename", "image.jpg");
    const std::string type = item.value("type", "image");
    const std::string url = saveBase64File(
        data, filename, type == "video" ? "mp4" : "jpg");
    if (url.empty()) continue;
    ctx_.db.execute("INSERT INTO post_media (post_id, url, media_type, position) "
                    "VALUES (?, ?, ?, ?)",
                    {Param::of(static_cast<long long>(postId)), Param::of(url),
                     Param::of(type == "video" ? "video" : "image"),
                     Param::of(position++)},
                    nullptr);
  }
  if (position == 0) {
    ctx_.db.execute("DELETE FROM posts WHERE id = ?",
                    {Param::of(static_cast<long long>(postId))}, nullptr);
    return ApiResult::fail(400, "No valid media was uploaded");
  }
  syncHashtags(static_cast<long long>(postId), caption);
  touchRecentPost(static_cast<long long>(postId));

  std::vector<Row> rows;
  ctx_.db.query("SELECT p.id, p.user_id, p.caption, p.location, p.created_at, "
                "u.username, u.profile_picture, 0 AS like_count, "
                "0 AS comment_count, 0 AS liked, 0 AS saved "
                "FROM posts p JOIN users u ON u.id = p.user_id WHERE p.id = ?",
                {Param::of(static_cast<long long>(postId))}, &rows, nullptr);
  Post post = rows.empty() ? Post{} : hydratePost(rows[0], viewer);
  return ApiResult::created({{"post", postToJson(post)}});
}

ApiResult AppService::getFeed(long long viewer, int page, int limit,
                              const std::string& mode) {
  page = std::max(page, 1);
  limit = std::min(std::max(limit, 5), 30);
  std::vector<Row> rows = fetchFeedRows(viewer, mode);
  const std::size_t start = (page - 1) * limit;
  json posts = json::array();

  // Chronological mode: fetchFeedRows already orders by created_at DESC, so the
  // custom heap is intentionally bypassed here. This is O(n) hydration plus the
  // indexed SQL scan; no ranking is applied.
  const bool chronological = mode == "recent" || mode == "chronological";
  if (chronological) {
    for (std::size_t i = start;
         i < rows.size() && posts.size() < static_cast<std::size_t>(limit); ++i) {
      posts.push_back(postToJson(hydratePost(rows[i], viewer)));
    }
    return ApiResult::ok({{"posts", posts},
                          {"page", page},
                          {"mode", mode},
                          {"order", "chronological"},
                          {"hasMore", start + limit < rows.size()}});
  }

  /*
   * FEED RANKING FORMULA (custom max-heap, ig::dsa::MaxHeap)
   * -------------------------------------------------------
   * score = W_R * recency + W_L * likes + W_C * comments
   *         + B_F * followsAuthor + B_A * recentActivity
   *
   * recency       = exp(-ageHours / 24)              in (0, 1]
   * likes         = ln(1 + likeCount) / ln(1 + 1000) clipped to [0, 1]
   * comments      = ln(1 + commentCount) / ln(1 + 300) clipped to [0, 1]
   * followsAuthor = 1 when viewer follows the author, else 0
   * recentActivity= 1 when the post is in the recentActivity deque
   * weights: W_R = 4.0, W_L = 2.0, W_C = 2.5, B_F = 1.5, B_A = 0.4
   *
   * The heap gives O(log n) insert and pop, so building and draining the ranked
   * order is O(n log n) after the MySQL candidate scan. Logarithmic engagement
   * prevents viral posts from dominating forever, and recency decays over 24h.
   */
  constexpr double W_R = 4.0, W_L = 2.0, W_C = 2.5, B_F = 1.5, B_A = 0.4;
  {
    std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
    ctx_.feedHeap.clear();
  }
  std::vector<Post> candidates;
  std::unordered_map<long long, std::size_t> candidateIndex;
  std::vector<long long> recentIds;
  {
    std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
    ctx_.recentActivity.forEach([&](long long id) { recentIds.push_back(id); });
  }
  for (const Row& row : rows) {
    Post post = hydratePost(row, viewer);
    const double recency = std::exp(-hoursSince(post.createdAt) / 24.0);
    const double likes = std::log1p(static_cast<double>(post.likeCount)) /
                         std::log1p(1000.0);
    const double comments = std::log1p(static_cast<double>(post.commentCount)) /
                            std::log1p(300.0);
    bool follows = false;
    {
      std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
      follows = ctx_.graph.isFollowing(viewer, post.userId);
    }
    const bool active = std::find(recentIds.begin(), recentIds.end(), post.id) !=
                        recentIds.end();
    post.score = W_R * std::min(recency, 1.0) +
                 W_L * std::min(likes, 1.0) + W_C * std::min(comments, 1.0) +
                 (follows ? B_F : 0.0) + (active ? B_A : 0.0);
    {
      std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
      ctx_.feedHeap.insert(post.id, post.score);
    }
    candidateIndex[post.id] = candidates.size();
    candidates.push_back(std::move(post));
  }

  std::vector<HeapItem> ordered;
  {
    std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
    ordered.reserve(ctx_.feedHeap.size());
    while (!ctx_.feedHeap.empty()) {
      ordered.push_back(ctx_.feedHeap.pop());  // pop yields sorted order
    }
  }
  for (std::size_t i = start;
       i < ordered.size() && posts.size() < static_cast<std::size_t>(limit); ++i) {
    auto it = candidateIndex.find(ordered[i].id);
    if (it != candidateIndex.end()) posts.push_back(postToJson(candidates[it->second]));
  }
  return ApiResult::ok({{"posts", posts},
                        {"page", page},
                        {"mode", mode},
                        {"order", "ranked"},
                        {"hasMore", start + limit < ordered.size()}});
}

ApiResult AppService::getPost(long long id, long long viewer) {
  std::vector<Row> rows;
  ctx_.db.query("SELECT p.id, p.user_id, p.caption, p.location, p.created_at, "
                "u.username, u.profile_picture, "
                "(SELECT COUNT(*) FROM likes l WHERE l.post_id = p.id) AS like_count, "
                "(SELECT COUNT(*) FROM comments c WHERE c.post_id = p.id) AS comment_count, "
                "EXISTS(SELECT 1 FROM likes l2 WHERE l2.post_id = p.id AND l2.user_id = ?) AS liked, "
                "EXISTS(SELECT 1 FROM saved_posts s WHERE s.post_id = p.id AND s.user_id = ?) AS saved "
                "FROM posts p JOIN users u ON u.id = p.user_id WHERE p.id = ?",
                {Param::of(viewer), Param::of(viewer), Param::of(id)}, &rows,
                nullptr);
  if (rows.empty()) return ApiResult::fail(404, "Post not found");
  if (!canView(rows[0].integer("user_id"), viewer)) {
    return ApiResult::fail(403, "Post unavailable");
  }
  return ApiResult::ok({{"post", postToJson(hydratePost(rows[0], viewer))}});
}

ApiResult AppService::getPostsByUser(long long id, long long viewer, int page,
                                     int limit) {
  if (!canView(id, viewer)) return ApiResult::ok({{"posts", json::array()}});
  page = std::max(page, 1);
  limit = std::min(std::max(limit, 5), 30);
  std::vector<Row> rows;
  ctx_.db.query("SELECT p.id, p.user_id, p.caption, p.location, p.created_at, "
                "u.username, u.profile_picture, "
                "(SELECT COUNT(*) FROM likes l WHERE l.post_id = p.id) AS like_count, "
                "(SELECT COUNT(*) FROM comments c WHERE c.post_id = p.id) AS comment_count, "
                "EXISTS(SELECT 1 FROM likes l2 WHERE l2.post_id = p.id AND l2.user_id = ?) AS liked, "
                "EXISTS(SELECT 1 FROM saved_posts s WHERE s.post_id = p.id AND s.user_id = ?) AS saved "
                "FROM posts p JOIN users u ON u.id = p.user_id "
                "WHERE p.user_id = ? ORDER BY p.created_at DESC LIMIT ? OFFSET ?",
                {Param::of(viewer), Param::of(viewer), Param::of(id),
                 Param::of(limit), Param::of((page - 1) * limit)},
                &rows, nullptr);
  json posts = json::array();
  for (const Row& row : rows) posts.push_back(postToJson(hydratePost(row, viewer)));
  return ApiResult::ok({{"posts", posts}});
}

ApiResult AppService::updatePost(long long id, long long viewer, const json& payload) {
  std::vector<Row> rows;
  ctx_.db.query("SELECT user_id, caption, location FROM posts WHERE id = ?",
                {Param::of(id)}, &rows, nullptr);
  if (rows.empty()) return ApiResult::fail(404, "Post not found");
  if (rows[0].integer("user_id") != viewer) {
    return ApiResult::fail(403, "Only the author can edit this post");
  }
  const std::string caption = payload.value("caption", rows[0].str("caption"));
  const std::string location = payload.value("location", rows[0].str("location"));
  if (caption.size() > 5000) return ApiResult::fail(400, "Caption is too long");
  if (location.size() > 150) {
    return ApiResult::fail(400, "Location is too long");
  }
  ctx_.db.execute("UPDATE posts SET caption = ?, location = ? WHERE id = ?",
                  {Param::of(caption), Param::of(location), Param::of(id)},
                  nullptr);
  replaceHashtags(id, caption);
  touchRecentPost(id);
  return getPost(id, viewer);
}

ApiResult AppService::deletePost(long long id, long long viewer) {
  std::vector<Row> rows;
  ctx_.db.query("SELECT user_id FROM posts WHERE id = ?", {Param::of(id)}, &rows,
                nullptr);
  if (rows.empty()) return ApiResult::fail(404, "Post not found");
  if (rows[0].integer("user_id") != viewer) {
    return ApiResult::fail(403, "Only the author can delete this post");
  }
  std::vector<Row> mediaRows;
  ctx_.db.query("SELECT url FROM post_media WHERE post_id = ?", {Param::of(id)},
                &mediaRows, nullptr);
  std::vector<std::string> mediaUrls;
  for (const Row& media : mediaRows) mediaUrls.push_back(media.str("url"));

  replaceHashtags(id, "");
  std::string error;
  if (!ctx_.db.execute("DELETE FROM posts WHERE id = ?", {Param::of(id)}, &error,
                       nullptr)) {
    return ApiResult::fail(500, error);
  }
  forgetPostCaches(id);
  for (const std::string& url : mediaUrls) removeUploadedFile(url);
  return ApiResult::ok({{"message", "Post deleted"},
                        {"postId", id}});
}

ApiResult AppService::likePost(long long id, long long viewer) {
  std::vector<Row> post;
  if (!ctx_.db.query("SELECT user_id FROM posts WHERE id = ?", {Param::of(id)},
                     &post, nullptr) ||
      post.empty()) {
    return ApiResult::fail(404, "Post not found");
  }
  const long long owner = post[0].integer("user_id");
  if (!canView(owner, viewer)) return ApiResult::fail(403, "Post unavailable");
  std::uint64_t affected = 0;
  std::string error;
  if (!ctx_.db.executeCount("INSERT IGNORE INTO likes (user_id, post_id) VALUES (?, ?)",
                            {Param::of(viewer), Param::of(id)}, &error,
                            &affected)) {
    return ApiResult::fail(500, error);
  }
  const std::string key = interactionKey(viewer, id);
  {
    std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
    ctx_.likedPosts.set(key, true);
  }
  if (affected > 0) {
    notify(owner, viewer, "like", id,
           ctx_.usernameFor(viewer) + " liked your post");
    touchRecentPost(id);
  }
  return ApiResult::ok({{"postId", id},
                        {"liked", true},
                        {"likeCount", countLikes(id)}});
}

ApiResult AppService::unlikePost(long long id, long long viewer) {
  std::vector<Row> post;
  if (!ctx_.db.query("SELECT user_id FROM posts WHERE id = ?", {Param::of(id)},
                     &post, nullptr) ||
      post.empty()) {
    return ApiResult::fail(404, "Post not found");
  }
  std::string error;
  if (!ctx_.db.executeCount("DELETE FROM likes WHERE user_id = ? AND post_id = ?",
                            {Param::of(viewer), Param::of(id)}, &error,
                            nullptr)) {
    return ApiResult::fail(500, error);
  }
  {
    std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
    ctx_.likedPosts.remove(interactionKey(viewer, id));
  }
  return ApiResult::ok({{"postId", id},
                        {"liked", false},
                        {"likeCount", countLikes(id)}});
}

ApiResult AppService::addComment(long long id, long long viewer, const json& payload) {
  const std::string body = trimCopy(payload.value("body", ""));
  if (body.empty() || body.size() > 1000) {
    return ApiResult::fail(400, "Comment must be 1-1000 characters");
  }
  long long owner = 0;
  if (!canViewPost(id, viewer, &owner)) {
    std::vector<Row> exists;
    if (!ctx_.db.query("SELECT user_id FROM posts WHERE id = ?", {Param::of(id)},
                       &exists, nullptr) ||
        exists.empty()) {
      return ApiResult::fail(404, "Post not found");
    }
    return ApiResult::fail(403, "Post unavailable");
  }
  std::uint64_t commentId = 0;
  if (!ctx_.db.execute("INSERT INTO comments (post_id, user_id, body) VALUES (?, ?, ?)",
                       {Param::of(id), Param::of(viewer), Param::of(body)},
                       nullptr, &commentId)) {
    return ApiResult::fail(500, "Could not add comment");
  }
  notify(owner, viewer, "comment", id,
         ctx_.usernameFor(viewer) + " commented: " + body.substr(0, 80));
  touchRecentPost(id);

  std::vector<Row> rows;
  ctx_.db.query("SELECT c.id, c.post_id, c.user_id, c.body, c.created_at, "
                "u.username, u.profile_picture FROM comments c "
                "JOIN users u ON u.id = c.user_id WHERE c.id = ?",
                {Param::of(static_cast<long long>(commentId))}, &rows, nullptr);
  json comment = json::object();
  if (!rows.empty()) {
    const Row& row = rows[0];
    comment = {{"id", row.integer("id")},
               {"postId", row.integer("post_id")},
               {"userId", row.integer("user_id")},
               {"username", row.str("username")},
               {"avatar", row.str("profile_picture")},
               {"body", row.str("body")},
               {"createdAt", row.str("created_at")}};
  }
  return ApiResult::created({{"comment", comment},
                             {"postId", id},
                             {"commentCount", countComments(id)}});
}

ApiResult AppService::getComments(long long id, long long viewer) {
  if (!canViewPost(id, viewer)) {
    std::vector<Row> exists;
    if (!ctx_.db.query("SELECT user_id FROM posts WHERE id = ?", {Param::of(id)},
                       &exists, nullptr) ||
        exists.empty()) {
      return ApiResult::fail(404, "Post not found");
    }
    return ApiResult::fail(403, "Post unavailable");
  }
  std::vector<Row> rows;
  ctx_.db.query("SELECT c.id, c.post_id, c.user_id, c.body, c.created_at, "
                "u.username, u.profile_picture FROM comments c "
                "JOIN users u ON u.id = c.user_id WHERE c.post_id = ? "
                "ORDER BY c.created_at ASC LIMIT 200",
                {Param::of(id)}, &rows, nullptr);
  json comments = json::array();
  for (const Row& row : rows) {
    comments.push_back({{"id", row.integer("id")},
                        {"postId", row.integer("post_id")},
                        {"userId", row.integer("user_id")},
                        {"username", row.str("username")},
                        {"avatar", row.str("profile_picture")},
                        {"body", row.str("body")},
                        {"createdAt", row.str("created_at")}});
  }
  return ApiResult::ok({{"comments", comments},
                        {"postId", id},
                        {"commentCount", countComments(id)}});
}

ApiResult AppService::deleteComment(long long id, long long viewer) {
  std::vector<Row> rows;
  ctx_.db.query("SELECT user_id, post_id FROM comments WHERE id = ?",
                {Param::of(id)}, &rows, nullptr);
  if (rows.empty()) return ApiResult::fail(404, "Comment not found");
  if (rows[0].integer("user_id") != viewer) {
    return ApiResult::fail(403, "Only the author can delete this comment");
  }
  const long long postId = rows[0].integer("post_id");
  ctx_.db.execute("DELETE FROM comments WHERE id = ?", {Param::of(id)}, nullptr);
  touchRecentPost(postId);
  return ApiResult::ok({{"message", "Comment deleted"},
                        {"postId", postId},
                        {"commentCount", countComments(postId)}});
}

ApiResult AppService::savePost(long long id, long long viewer) {
  if (!canViewPost(id, viewer)) {
    std::vector<Row> exists;
    if (!ctx_.db.query("SELECT user_id FROM posts WHERE id = ?", {Param::of(id)},
                       &exists, nullptr) ||
        exists.empty()) {
      return ApiResult::fail(404, "Post not found");
    }
    return ApiResult::fail(403, "Post unavailable");
  }
  std::string error;
  if (!ctx_.db.executeCount("INSERT IGNORE INTO saved_posts (user_id, post_id) "
                            "VALUES (?, ?)",
                            {Param::of(viewer), Param::of(id)}, &error, nullptr)) {
    return ApiResult::fail(500, error);
  }
  {
    std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
    ctx_.savedPosts.set(interactionKey(viewer, id), true);
  }
  return ApiResult::ok({{"postId", id}, {"saved", true}});
}

ApiResult AppService::unsavePost(long long id, long long viewer) {
  std::vector<Row> post;
  if (!ctx_.db.query("SELECT user_id FROM posts WHERE id = ?", {Param::of(id)},
                     &post, nullptr) ||
      post.empty()) {
    return ApiResult::fail(404, "Post not found");
  }
  std::string error;
  if (!ctx_.db.executeCount("DELETE FROM saved_posts WHERE user_id = ? AND post_id = ?",
                            {Param::of(viewer), Param::of(id)}, &error, nullptr)) {
    return ApiResult::fail(500, error);
  }
  {
    std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
    ctx_.savedPosts.remove(interactionKey(viewer, id));
  }
  return ApiResult::ok({{"postId", id}, {"saved", false}});
}

ApiResult AppService::savedPosts(long long viewer) {
  std::vector<Row> rows;
  ctx_.db.query("SELECT p.id, p.user_id, p.caption, p.location, p.created_at, "
                "u.username, u.profile_picture, "
                "(SELECT COUNT(*) FROM likes l WHERE l.post_id = p.id) AS like_count, "
                "(SELECT COUNT(*) FROM comments c WHERE c.post_id = p.id) AS comment_count, "
                "EXISTS(SELECT 1 FROM likes l2 WHERE l2.post_id = p.id AND l2.user_id = ?) AS liked, "
                "1 AS saved FROM saved_posts s JOIN posts p ON p.id = s.post_id "
                "JOIN users u ON u.id = p.user_id "
                 "WHERE s.user_id = ? AND NOT EXISTS (SELECT 1 FROM blocked_users b WHERE "
                 "(b.blocker_id = p.user_id AND b.blocked_id = ?) OR "
                 "(b.blocker_id = ? AND b.blocked_id = p.user_id)) "
                 "AND (p.user_id = ? OR u.is_private = 0 OR p.user_id IN "
                 "(SELECT following_id FROM followers WHERE follower_id = ?)) "
                "ORDER BY s.created_at DESC LIMIT 100",
                {Param::of(viewer), Param::of(viewer), Param::of(viewer),
                  Param::of(viewer), Param::of(viewer), Param::of(viewer)},
                 &rows, nullptr);
  json posts = json::array();
  for (const Row& row : rows) posts.push_back(postToJson(hydratePost(row, viewer)));
  return ApiResult::ok({{"posts", posts}});
}

ApiResult AppService::explore(long long viewer, const std::string& rawTag,
                               int page, int limit) {
  std::string tag = trimCopy(rawTag);
  if (!tag.empty() && tag.front() == '#') tag.erase(tag.begin());
  std::transform(tag.begin(), tag.end(), tag.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  page = std::max(page, 1);
  limit = std::min(std::max(limit, 6), 30);
  std::vector<Row> rows;
  if (!tag.empty()) {
    ctx_.db.query("SELECT p.id, p.user_id, p.caption, p.location, p.created_at, "
                  "u.username, u.profile_picture, "
                  "(SELECT COUNT(*) FROM likes l WHERE l.post_id = p.id) AS like_count, "
                  "(SELECT COUNT(*) FROM comments c WHERE c.post_id = p.id) AS comment_count, "
                  "EXISTS(SELECT 1 FROM likes l2 WHERE l2.post_id = p.id AND l2.user_id = ?) AS liked, "
                  "EXISTS(SELECT 1 FROM saved_posts s WHERE s.post_id = p.id AND s.user_id = ?) AS saved "
                  "FROM post_hashtags ph "
                  "JOIN hashtags h ON h.id = ph.hashtag_id JOIN posts p ON p.id = ph.post_id "
                  "JOIN users u ON u.id = p.user_id "
                  "WHERE NOT EXISTS (SELECT 1 FROM blocked_users b WHERE "
                  "(b.blocker_id = p.user_id AND b.blocked_id = ?) OR "
                  "(b.blocker_id = ? AND b.blocked_id = p.user_id)) "
                  "AND (p.user_id = ? OR u.is_private = 0 OR p.user_id IN "
                  "(SELECT following_id FROM followers WHERE follower_id = ?)) "
                  "AND h.tag = ? ORDER BY p.created_at DESC LIMIT ? OFFSET ?",
                  {Param::of(viewer), Param::of(viewer), Param::of(viewer),
                   Param::of(viewer), Param::of(viewer), Param::of(viewer),
                   Param::of(tag), Param::of(limit),
                   Param::of((page - 1) * limit)},
                  &rows, nullptr);
  } else {
    rows = fetchFeedRows(viewer, "latest");
  }
  json posts = json::array();
  for (const Row& row : rows) posts.push_back(postToJson(hydratePost(row, viewer)));
  return ApiResult::ok({{"posts", posts}, {"tag", tag}});
}

ApiResult AppService::trendingHashtags() {
  std::vector<Row> rows;
  ctx_.db.query(
      "SELECT h.id, h.tag, h.usage_count, "
      "(SELECT COUNT(*) FROM post_hashtags ph JOIN posts p ON p.id = ph.post_id "
      "WHERE ph.hashtag_id = h.id AND "
      "p.created_at >= DATE_SUB(NOW(), INTERVAL 7 DAY)) AS recent_count "
      "FROM hashtags h WHERE h.usage_count > 0",
      {}, &rows, nullptr);

  struct TagMeta {
    std::string tag;
    long long count = 0;
    long long recent = 0;
  };
  std::unordered_map<long long, TagMeta> metadata;
  {
    std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
    ctx_.trendingHashtags.clear();
  }
  for (const Row& row : rows) {
    const long long id = row.integer("id");
    const long long count = row.integer("usage_count");
    const long long recent = row.integer("recent_count");
    metadata[id] = TagMeta{row.str("tag"), count, recent};
    const double score = std::log1p(static_cast<double>(count)) +
                         2.0 * std::log1p(static_cast<double>(recent)) +
                         0.05 * static_cast<double>(recent);
    std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
    ctx_.trendingHashtags.insert(id, score);
  }

  json tags = json::array();
  std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
  for (int i = 0; i < 12 && !ctx_.trendingHashtags.empty(); ++i) {
    const HeapItem item = ctx_.trendingHashtags.pop();
    const auto found = metadata.find(item.id);
    if (found == metadata.end()) continue;
    tags.push_back({{"tag", found->second.tag},
                    {"count", found->second.count},
                    {"recentCount", found->second.recent},
                    {"score", item.score}});
  }
  return ApiResult::ok({{"hashtags", tags}});
}

}  // namespace ig
