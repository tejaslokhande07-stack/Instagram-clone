#include "services/app_service.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace ig {

static json storyToJson(const Story& story) {
  return {{"id", story.id},
          {"userId", story.userId},
          {"username", story.username},
          {"avatar", story.avatar},
          {"mediaUrl", story.mediaUrl},
          {"mediaType", story.mediaType},
          {"createdAt", story.createdAt},
          {"expiresAt", story.expiresAt},
          {"viewCount", story.viewCount},
          {"viewedByViewer", story.viewedByViewer}};
}

static json reelToJson(const Reel& reel) {
  return {{"id", reel.id},
          {"userId", reel.userId},
          {"username", reel.username},
          {"avatar", reel.avatar},
          {"videoUrl", reel.videoUrl},
          {"thumbnailUrl", reel.thumbnailUrl},
          {"caption", reel.caption},
          {"views", reel.views},
          {"likes", reel.likes},
          {"comments", reel.comments},
          {"likedByViewer", reel.likedByViewer},
          {"savedByViewer", reel.savedByViewer},
          {"createdAt", reel.createdAt},
          {"score", reel.score}};
}

void AppService::pruneExpiredStories() {
  std::vector<Row> expired;
  if (!ctx_.db.query("SELECT id FROM stories WHERE expires_at <= NOW()", {},
                     &expired, nullptr) ||
      expired.empty()) {
    return;
  }
  std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
  for (const Row& row : expired) {
    const long long id = row.integer("id");
    ctx_.activeStories.remove(id);
    ctx_.storyOrder.eraseValue(id);
  }
}

ApiResult AppService::createStory(long long viewer, const json& payload) {
  const std::string data = payload.value("data", "");
  const std::string filename = payload.value("filename", "story.jpg");
  const std::string type = payload.value("type", "image");
  if (data.empty()) return ApiResult::fail(400, "Story media is required");
  const std::string url =
      saveBase64File(data, filename, type == "video" ? "mp4" : "jpg");
  if (url.empty()) return ApiResult::fail(400, "Invalid media upload");

  const int duration = std::clamp(payload.value("duration", 5000), 1000, 30000);
  std::uint64_t id = 0;
  if (!ctx_.db.execute(
          "INSERT INTO stories (user_id, media_url, media_type, duration_ms, "
          "expires_at) VALUES (?, ?, ?, ?, DATE_ADD(NOW(), INTERVAL 24 HOUR))",
          {Param::of(viewer), Param::of(url),
           Param::of(type == "video" ? "video" : "image"), Param::of(duration)},
          nullptr, &id)) {
    return ApiResult::fail(500, "Could not create story");
  }
  {
    std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
    const long long storyId = static_cast<long long>(id);
    if (ctx_.activeStories.insert(storyId)) ctx_.storyOrder.pushBack(storyId);
  }
  return ApiResult::created({{"id", static_cast<long long>(id)}});
}

ApiResult AppService::getStories(long long viewer) {
  pruneExpiredStories();
  std::vector<Row> rows;
  ctx_.db.query(
      "SELECT s.id, s.user_id, s.media_url, s.media_type, s.created_at, "
      "s.expires_at, u.username, u.profile_picture, "
      "(SELECT COUNT(*) FROM story_views sv WHERE sv.story_id = s.id) AS view_count, "
      "EXISTS(SELECT 1 FROM story_views sv2 WHERE sv2.story_id = s.id "
      "AND sv2.viewer_id = ?) AS viewed "
      "FROM stories s JOIN users u ON u.id = s.user_id "
      "WHERE s.expires_at > NOW() AND s.user_id != ? "
      "AND s.user_id IN (SELECT following_id FROM followers WHERE follower_id = ?) "
      "AND NOT EXISTS (SELECT 1 FROM blocked_users b WHERE "
      "(b.blocker_id = s.user_id AND b.blocked_id = ?) OR "
      "(b.blocker_id = ? AND b.blocked_id = s.user_id)) "
      "ORDER BY s.created_at ASC, s.id ASC",
      {Param::of(viewer), Param::of(viewer), Param::of(viewer),
       Param::of(viewer), Param::of(viewer)},
      &rows, nullptr);

  std::unordered_map<long long, Story> byId;
  for (const Row& row : rows) {
    Story story = hydrateStory(row, viewer);
    byId[story.id] = std::move(story);
  }

  std::vector<Row> own;
  ctx_.db.query(
      "SELECT s.id, s.user_id, s.media_url, s.media_type, s.created_at, "
      "s.expires_at, u.username, u.profile_picture, "
      "(SELECT COUNT(*) FROM story_views sv WHERE sv.story_id = s.id) AS view_count, "
      "1 AS viewed FROM stories s JOIN users u ON u.id = s.user_id "
      "WHERE s.expires_at > NOW() AND s.user_id = ? ORDER BY s.created_at ASC",
      {Param::of(viewer)}, &own, nullptr);
  for (const Row& row : own) {
    Story story = hydrateStory(row, viewer);
    byId[story.id] = std::move(story);
  }

  // IdDeque provides chronological ordering, HashSet provides O(1) membership,
  // and DynamicVector snapshots the order without copying into std::vector.
  DynamicVector<long long> order;
  {
    std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
    ctx_.storyOrder.forEach([&](long long id) {
      if (ctx_.activeStories.contains(id)) order.pushBack(id);
    });
  }

  json stories = json::array();
  json mine = json::array();
  std::unordered_set<long long> emitted;
  order.forEach([&](long long id) {
    auto it = byId.find(id);
    if (it == byId.end()) return;  // not visible to this viewer
    emitted.insert(id);
    json item = storyToJson(it->second);
    if (it->second.userId == viewer) mine.push_back(std::move(item));
    else stories.push_back(std::move(item));
  });
  // Defensive fallback: if a row is present but missing from the cache order,
  // still return it rather than silently hiding valid story data.
  for (const auto& entry : byId) {
    if (emitted.count(entry.first)) continue;
    json item = storyToJson(entry.second);
    if (entry.second.userId == viewer) mine.push_back(std::move(item));
    else stories.push_back(std::move(item));
  }

  return ApiResult::ok({{"stories", stories}, {"mine", mine}});
}

ApiResult AppService::viewStory(long long id, long long viewer) {
  std::vector<Row> story;
  ctx_.db.query("SELECT user_id FROM stories WHERE id = ? AND expires_at > NOW()",
                {Param::of(id)}, &story, nullptr);
  if (story.empty()) return ApiResult::fail(404, "Story not found or expired");
  std::uint64_t affected = 0;
  ctx_.db.executeCount(
      "INSERT IGNORE INTO story_views (story_id, viewer_id) VALUES (?, ?)",
      {Param::of(id), Param::of(viewer)}, nullptr, &affected);
  // INSERT IGNORE makes repeat views idempotent; notify only on the first view.
  if (affected > 0 && story[0].integer("user_id") != viewer) {
    notify(story[0].integer("user_id"), viewer, "story_view", id,
           ctx_.usernameFor(viewer) + " viewed your story");
  }
  return ApiResult::ok({{"viewed", true}});
}

ApiResult AppService::createReel(long long viewer, const json& payload) {
  const std::string data = payload.value("data", "");
  if (data.empty()) return ApiResult::fail(400, "Video is required");
  const std::string url = saveBase64File(
      data, payload.value("filename", "reel.mp4"), "mp4");
  if (url.empty()) return ApiResult::fail(400, "Invalid video upload");
  std::string thumbnail;
  const std::string thumbData = payload.value("thumbnailData", "");
  if (!thumbData.empty()) {
    thumbnail = saveBase64File(
        thumbData, payload.value("thumbnailFilename", "thumb.jpg"), "jpg");
  }
  std::uint64_t id = 0;
  if (!ctx_.db.execute("INSERT INTO reels (user_id, video_url, thumbnail_url, caption) "
                       "VALUES (?, ?, ?, ?)",
                       {Param::of(viewer), Param::of(url), Param::of(thumbnail),
                        Param::of(payload.value("caption", "").substr(0, 2000))},
                       nullptr, &id)) {
    return ApiResult::fail(500, "Could not create reel");
  }
  return ApiResult::created({{"id", static_cast<long long>(id)}});
}

ApiResult AppService::getReels(long long viewer, int page, int limit,
                               const std::string& mode) {
  page = std::max(page, 1);
  limit = std::min(std::max(limit, 5), 30);
  const std::string sql =
      "SELECT r.id, r.user_id, r.video_url, r.thumbnail_url, r.caption, "
      "r.views_count, r.created_at, u.username, u.profile_picture, "
      "(SELECT COUNT(*) FROM reel_likes rl WHERE rl.reel_id = r.id) AS like_count, "
      "(SELECT COUNT(*) FROM reel_comments rc WHERE rc.reel_id = r.id) AS comment_count, "
      "EXISTS(SELECT 1 FROM reel_likes rl2 WHERE rl2.reel_id = r.id AND rl2.user_id = ?) AS liked, "
      "EXISTS(SELECT 1 FROM reel_saves rs WHERE rs.reel_id = r.id AND rs.user_id = ?) AS saved "
      "FROM reels r JOIN users u ON u.id = r.user_id "
      "WHERE NOT EXISTS (SELECT 1 FROM blocked_users b WHERE "
      "(b.blocker_id = r.user_id AND b.blocked_id = ?) OR "
      "(b.blocker_id = ? AND b.blocked_id = r.user_id)) "
      "AND (r.user_id = ? OR u.is_private = 0 OR r.user_id IN "
      "(SELECT following_id FROM followers WHERE follower_id = ?)) "
      "ORDER BY r.created_at DESC LIMIT 200";

  std::vector<Row> rows;
  ctx_.db.query(sql, {Param::of(viewer), Param::of(viewer), Param::of(viewer),
                      Param::of(viewer), Param::of(viewer), Param::of(viewer)},
                &rows, nullptr);
  std::vector<Reel> reels;
  {
    std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
    ctx_.trendingReels.clear();
  }
  for (const Row& row : rows) {
    Reel reel = hydrateReel(row, viewer);
    // Trending score = log(views+1)*2 + log(likes+1)*3 + log(comments+1)*5
    //                  + exp(-ageHours/24) [the custom max-heap orders by this].
    std::tm tm{};
    double recency = 0.0;
    if (reel.createdAt.size() >= 19) {
      std::istringstream ss(reel.createdAt.substr(0, 19));
      ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
      if (!ss.fail()) {
        const double ageHours = std::max(
            0.0, static_cast<double>(std::difftime(std::time(nullptr),
                                                   std::mktime(&tm))) / 3600.0);
        recency = std::exp(-ageHours / 24.0);
      }
    }
    reel.score = 2.0 * std::log1p(static_cast<double>(reel.views)) +
                 3.0 * std::log1p(static_cast<double>(reel.likes)) +
                 5.0 * std::log1p(static_cast<double>(reel.comments)) + recency;
    {
      std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
      ctx_.trendingReels.insert(reel.id, reel.score);
    }
    reels.push_back(std::move(reel));
  }
  if (mode == "trending") {
    std::unordered_map<long long, Reel> byId;
    for (const Reel& reel : reels) byId[reel.id] = reel;
    std::vector<Reel> sorted;
    std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
    while (!ctx_.trendingReels.empty()) {
      auto it = byId.find(ctx_.trendingReels.pop().id);
      if (it != byId.end()) sorted.push_back(std::move(it->second));
    }
    reels = std::move(sorted);
  }
  json result = json::array();
  const std::size_t start = (page - 1) * limit;
  for (std::size_t i = start;
       i < reels.size() && result.size() < static_cast<std::size_t>(limit);
       ++i) {
    result.push_back(reelToJson(reels[i]));
  }
  return ApiResult::ok({{"reels", result}, {"hasMore", start + limit < reels.size()}});
}

ApiResult AppService::likeReel(long long id, long long viewer) {
  std::vector<Row> reel;
  ctx_.db.query("SELECT user_id FROM reels WHERE id = ?", {Param::of(id)}, &reel,
                nullptr);
  if (reel.empty()) return ApiResult::fail(404, "Reel not found");
  std::uint64_t affected = 0;
  ctx_.db.executeCount(
      "INSERT IGNORE INTO reel_likes (reel_id, user_id) VALUES (?, ?)",
      {Param::of(id), Param::of(viewer)}, nullptr, &affected);
  if (affected > 0) {
    notify(reel[0].integer("user_id"), viewer, "reel_like", id,
           ctx_.usernameFor(viewer) + " liked your reel");
  }
  return ApiResult::ok({{"liked", true}});
}

ApiResult AppService::unlikeReel(long long id, long long viewer) {
  ctx_.db.execute("DELETE FROM reel_likes WHERE reel_id = ? AND user_id = ?",
                  {Param::of(id), Param::of(viewer)}, nullptr);
  return ApiResult::ok({{"liked", false}});
}

ApiResult AppService::commentReel(long long id, long long viewer,
                                  const json& payload) {
  const std::string body = payload.value("body", "");
  if (body.empty() || body.size() > 1000) {
    return ApiResult::fail(400, "Comment must be 1-1000 characters");
  }
  std::vector<Row> reel;
  ctx_.db.query("SELECT user_id FROM reels WHERE id = ?", {Param::of(id)}, &reel,
                nullptr);
  if (reel.empty()) return ApiResult::fail(404, "Reel not found");
  std::uint64_t commentId = 0;
  ctx_.db.execute("INSERT INTO reel_comments (reel_id, user_id, body) VALUES (?, ?, ?)",
                  {Param::of(id), Param::of(viewer), Param::of(body)}, nullptr,
                  &commentId);
  notify(reel[0].integer("user_id"), viewer, "reel_comment", id,
         ctx_.usernameFor(viewer) + " commented on your reel");
  return ApiResult::created({{"id", static_cast<long long>(commentId)}});
}

ApiResult AppService::getReelComments(long long id, long long viewer) {
  std::vector<Row> rows;
  ctx_.db.query("SELECT rc.id, rc.body, rc.created_at, rc.user_id, u.username, "
                "u.profile_picture FROM reel_comments rc JOIN users u ON u.id = rc.user_id "
                "WHERE rc.reel_id = ? ORDER BY rc.created_at ASC LIMIT 200",
                {Param::of(id)}, &rows, nullptr);
  json comments = json::array();
  for (const Row& row : rows) {
    comments.push_back({{"id", row.integer("id")},
                        {"userId", row.integer("user_id")},
                        {"username", row.str("username")},
                        {"avatar", row.str("profile_picture")},
                        {"body", row.str("body")},
                        {"createdAt", row.str("created_at")}});
  }
  (void)viewer;
  return ApiResult::ok({{"comments", comments}});
}

ApiResult AppService::saveReel(long long id, long long viewer) {
  ctx_.db.execute("INSERT IGNORE INTO reel_saves (reel_id, user_id) VALUES (?, ?)",
                  {Param::of(id), Param::of(viewer)}, nullptr);
  return ApiResult::ok({{"saved", true}});
}

ApiResult AppService::unsaveReel(long long id, long long viewer) {
  ctx_.db.execute("DELETE FROM reel_saves WHERE reel_id = ? AND user_id = ?",
                  {Param::of(id), Param::of(viewer)}, nullptr);
  return ApiResult::ok({{"saved", false}});
}

ApiResult AppService::viewReel(long long id, long long viewer) {
  std::uint64_t affected = 0;
  ctx_.db.executeCount(
      "INSERT IGNORE INTO reel_views (reel_id, viewer_id) VALUES (?, ?)",
      {Param::of(id), Param::of(viewer)}, nullptr, &affected);
  if (affected > 0) {
    ctx_.db.execute("UPDATE reels SET views_count = views_count + 1 WHERE id = ?",
                    {Param::of(id)}, nullptr);
  }
  {
    std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
    ctx_.recentActivity.pushFront(id);
  }
  return ApiResult::ok({{"viewed", true}});
}

ApiResult AppService::savedReels(long long viewer) {
  std::vector<Row> rows;
  ctx_.db.query("SELECT r.id, r.user_id, r.video_url, r.thumbnail_url, r.caption, "
                "r.views_count, r.created_at, u.username, u.profile_picture, "
                "(SELECT COUNT(*) FROM reel_likes rl WHERE rl.reel_id = r.id) AS like_count, "
                "(SELECT COUNT(*) FROM reel_comments rc WHERE rc.reel_id = r.id) AS comment_count, "
                "0 AS liked, 1 AS saved FROM reel_saves rs JOIN reels r ON r.id = rs.reel_id "
                "JOIN users u ON u.id = r.user_id WHERE rs.user_id = ? "
                "ORDER BY rs.created_at DESC LIMIT 50",
                {Param::of(viewer)}, &rows, nullptr);
  json reels = json::array();
  for (const Row& row : rows) reels.push_back(reelToJson(hydrateReel(row, viewer)));
  return ApiResult::ok({{"reels", reels}});
}

}  // namespace ig
