#include "services/app_service.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <set>

#include "core/crypto.h"

namespace ig {

std::string interactionKey(long long first, long long second) {
  return std::to_string(first) + ":" + std::to_string(second);
}

json postToJson(const Post& post) {
  json media = json::array();
  for (const auto& item : post.media) {
    media.push_back({{"id", item.id},
                     {"url", item.url},
                     {"type", item.type},
                     {"position", item.position}});
  }
  json tags = json::array();
  for (const auto& tag : post.hashtags) tags.push_back(tag);
  return {{"id", post.id},
          {"userId", post.userId},
          {"username", post.username},
          {"avatar", post.avatar},
          {"caption", post.caption},
          {"location", post.location},
          {"createdAt", post.createdAt},
          {"media", media},
          {"hashtags", tags},
          {"likeCount", post.likeCount},
          {"commentCount", post.commentCount},
          {"likedByViewer", post.likedByViewer},
          {"savedByViewer", post.savedByViewer},
          {"score", post.score}};
}

bool AppService::userById(long long id, User* out, std::string* error) const {
  std::vector<Row> rows;
  std::string localError;
  if (!ctx_.db.query(
          "SELECT id, username, email, full_name, bio, website, "
          "profile_picture, is_private, is_verified, created_at "
          "FROM users WHERE id = ?",
          {Param::of(id)}, &rows, &localError)) {
    if (error) *error = localError;
    return false;
  }
  if (rows.empty()) return false;
  const Row& row = rows[0];
  out->id = row.integer("id");
  out->username = row.str("username");
  out->email = row.str("email");
  out->fullName = row.str("full_name");
  out->bio = row.str("bio");
  out->website = row.str("website");
  out->profilePicture = row.str("profile_picture");
  out->isPrivate = row.boolean("is_private");
  out->isVerified = row.boolean("is_verified");
  out->createdAt = row.str("created_at");
  return true;
}

json AppService::publicUser(const User& user) const {
  return {{"id", user.id},
          {"username", user.username},
          {"fullName", user.fullName},
          {"bio", user.bio},
          {"website", user.website},
          {"profilePicture", user.profilePicture},
          {"isPrivate", user.isPrivate},
          {"isVerified", user.isVerified},
          {"createdAt", user.createdAt}};
}

json AppService::publicUserById(long long id) const {
  User user;
  if (!userById(id, &user)) return json::object();
  return publicUser(user);
}

bool AppService::isBlocked(long long a, long long b) const {
  std::vector<Row> rows;
  if (!ctx_.db.query("SELECT 1 AS x FROM blocked_users WHERE "
                     "(blocker_id = ? AND blocked_id = ?) OR "
                     "(blocker_id = ? AND blocked_id = ?) LIMIT 1",
                     {Param::of(a), Param::of(b), Param::of(b), Param::of(a)},
                     &rows, nullptr)) {
    return false;
  }
  return !rows.empty();
}

bool AppService::isFollowing(long long follower, long long target) const {
  std::vector<Row> rows;
  if (!ctx_.db.query("SELECT 1 AS x FROM followers WHERE follower_id = ? AND "
                     "following_id = ?",
                     {Param::of(follower), Param::of(target)}, &rows, nullptr)) {
    return false;
  }
  return !rows.empty();
}

bool AppService::canView(long long owner, long long viewer) const {
  if (owner == viewer) return true;
  if (isBlocked(owner, viewer)) return false;
  User user;
  if (!userById(owner, &user)) return false;
  if (!user.isPrivate) return true;
  return isFollowing(viewer, owner);
}

void AppService::notify(long long recipient, long long actor,
                        const std::string& type, long long entityId,
                        const std::string& body) {
  if (recipient == actor) return;
  std::uint64_t inserted = 0;
  if (ctx_.db.execute("INSERT INTO notifications (user_id, actor_id, type, "
                      "entity_id, body) VALUES (?, ?, ?, ?, ?)",
                      {Param::of(recipient), Param::of(actor), Param::of(type),
                       Param::of(entityId), Param::of(body)},
                      nullptr, &inserted)) {
    std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
    ctx_.notificationQueue.enqueue(static_cast<long long>(inserted));
  }
}

std::string AppService::saveBase64File(const std::string& data,
                                       const std::string& filename,
                                       const std::string& fallbackExt) {
  std::string bytes;
  if (!crypto::base64Decode(data, &bytes)) return {};
  constexpr std::size_t kMaxBytes = 25 * 1024 * 1024;
  if (bytes.empty() || bytes.size() > kMaxBytes) return {};

  std::string ext = fallbackExt;
  const std::size_t dot = filename.rfind('.');
  if (dot != std::string::npos && dot + 1 < filename.size()) {
    ext = filename.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  }
  static const std::set<std::string> allowed = {
      "png", "jpg", "jpeg", "gif", "webp", "mp4", "webm", "mov"};
  if (!allowed.count(ext)) return {};

  std::error_code ec;
  std::filesystem::create_directories(ctx_.config.uploadDir, ec);
  const std::string name = crypto::randomToken(12) + "." + ext;
  const std::filesystem::path target =
      std::filesystem::path(ctx_.config.uploadDir) / name;
  std::ofstream file(target, std::ios::binary);
  if (!file.is_open()) return {};
  file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  file.close();
  return "/uploads/" + name;
}

bool AppService::removeUploadedFile(const std::string& url) {
  const std::string prefix = "/uploads/";
  if (url.compare(0, prefix.size(), prefix) != 0) return false;
  const std::string name = url.substr(prefix.size());
  if (name.empty() || name.find('/') != std::string::npos ||
      name.find('\\') != std::string::npos || name == "." || name == "..") {
    return false;
  }

  std::error_code ec;
  const std::filesystem::path root =
      std::filesystem::weakly_canonical(
          std::filesystem::absolute(ctx_.config.uploadDir), ec);
  if (ec) return false;
  const std::filesystem::path target =
      std::filesystem::weakly_canonical(root / name, ec);
  if (ec) return false;
  const std::filesystem::path relative = target.lexically_relative(root);
  if (relative.empty() || relative.is_absolute() ||
      *relative.begin() == "..") {
    return false;
  }
  return std::filesystem::remove(target, ec) && !ec;
}

void AppService::syncHashtags(long long postId, const std::string& caption) {
  static const std::regex tagPattern("#([A-Za-z0-9_]{1,60})");
  // usage_count counts posts per tag, not raw occurrences in a caption.
  std::set<std::string> seenTags;
  auto begin = std::sregex_iterator(caption.begin(), caption.end(), tagPattern);
  auto end = std::sregex_iterator();
  for (auto it = begin; it != end; ++it) {
    std::string tag = (*it)[1].str();
    std::transform(tag.begin(), tag.end(), tag.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (!seenTags.insert(tag).second) continue;
    std::vector<Row> found;
    ctx_.db.query("SELECT id FROM hashtags WHERE tag = ?", {Param::of(tag)},
                  &found, nullptr);
    long long tagId = 0;
    if (found.empty()) {
      std::uint64_t inserted = 0;
      if (ctx_.db.execute("INSERT INTO hashtags (tag, usage_count) VALUES (?, 1)",
                          {Param::of(tag)}, nullptr, &inserted)) {
        tagId = static_cast<long long>(inserted);
      }
    } else {
      tagId = found[0].integer("id");
      ctx_.db.execute("UPDATE hashtags SET usage_count = usage_count + 1 WHERE id = ?",
                      {Param::of(tagId)}, nullptr);
    }
    if (tagId > 0) {
      ctx_.db.execute("INSERT IGNORE INTO post_hashtags (post_id, hashtag_id) "
                      "VALUES (?, ?)",
                      {Param::of(postId), Param::of(tagId)}, nullptr);
      std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
      ctx_.hashtagTrie.insert(tag);
    }
  }
}

void AppService::replaceHashtags(long long postId, const std::string& caption) {
  std::vector<Row> previous;
  ctx_.db.query("SELECT h.id, h.tag FROM hashtags h JOIN post_hashtags ph "
                "ON ph.hashtag_id = h.id WHERE ph.post_id = ?",
                {Param::of(postId)}, &previous, nullptr);
  ctx_.db.execute("DELETE FROM post_hashtags WHERE post_id = ?",
                  {Param::of(postId)}, nullptr);
  for (const Row& row : previous) {
    const long long tagId = row.integer("id");
    const std::string tag = row.str("tag");
    ctx_.db.execute("UPDATE hashtags SET usage_count = GREATEST(0, usage_count - 1) "
                    "WHERE id = ?",
                    {Param::of(tagId)}, nullptr);
    std::vector<Row> remaining;
    ctx_.db.query("SELECT usage_count FROM hashtags WHERE id = ?",
                  {Param::of(tagId)}, &remaining, nullptr);
    if (remaining.empty() || remaining[0].integer("usage_count") == 0) {
      ctx_.db.execute("DELETE FROM hashtags WHERE id = ? AND usage_count = 0 "
                      "AND NOT EXISTS (SELECT 1 FROM post_hashtags "
                      "WHERE hashtag_id = ?)",
                      {Param::of(tagId), Param::of(tagId)}, nullptr);
      std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
      ctx_.hashtagTrie.remove(tag);
    }
  }
  syncHashtags(postId, caption);
}

bool AppService::canViewPost(long long postId, long long viewer,
                             long long* owner) const {
  std::vector<Row> rows;
  if (!ctx_.db.query("SELECT user_id FROM posts WHERE id = ?",
                     {Param::of(postId)}, &rows, nullptr) ||
      rows.empty()) {
    return false;
  }
  const long long postOwner = rows[0].integer("user_id");
  if (!canView(postOwner, viewer)) return false;
  if (owner) *owner = postOwner;
  return true;
}

long long AppService::countLikes(long long postId) const {
  std::vector<Row> rows;
  ctx_.db.query("SELECT COUNT(*) AS total FROM likes WHERE post_id = ?",
                {Param::of(postId)}, &rows, nullptr);
  return rows.empty() ? 0 : rows[0].integer("total");
}

long long AppService::countComments(long long postId) const {
  std::vector<Row> rows;
  ctx_.db.query("SELECT COUNT(*) AS total FROM comments WHERE post_id = ?",
                {Param::of(postId)}, &rows, nullptr);
  return rows.empty() ? 0 : rows[0].integer("total");
}

void AppService::touchRecentPost(long long postId) {
  std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
  ctx_.recentActivity.eraseValue(postId);
  ctx_.recentActivity.pushFront(postId);
}

void AppService::forgetPostCaches(long long postId) {
  const std::string suffix = ":" + std::to_string(postId);
  std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
  for (const auto& entry : ctx_.likedPosts.entries()) {
    const std::string& key = entry.key;
    if (key.size() >= suffix.size() &&
        key.compare(key.size() - suffix.size(), suffix.size(), suffix) == 0) {
      ctx_.likedPosts.remove(key);
    }
  }
  for (const auto& entry : ctx_.savedPosts.entries()) {
    const std::string& key = entry.key;
    if (key.size() >= suffix.size() &&
        key.compare(key.size() - suffix.size(), suffix.size(), suffix) == 0) {
      ctx_.savedPosts.remove(key);
    }
  }
  ctx_.recentActivity.eraseValue(postId);
  ctx_.feedHeap.removeId(postId);
}

Post AppService::hydratePost(const Row& row, long long viewer) const {
  Post post;
  post.id = row.integer("id");
  post.userId = row.integer("user_id");
  post.username = row.str("username");
  post.avatar = row.str("profile_picture");
  post.caption = row.str("caption");
  post.location = row.str("location");
  post.createdAt = row.str("created_at");
  post.likeCount = row.integer("like_count");
  post.commentCount = row.integer("comment_count");
  {
    // The string-keyed HashTables are a real read cache, not just write-only
    // mirrors of MySQL. They are rebuilt from likes/saved_posts at startup and
    // updated only after successful writes.
    const std::string key = interactionKey(viewer, post.id);
    std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
    bool liked = false;
    bool saved = false;
    post.likedByViewer = ctx_.likedPosts.search(key, &liked) ? liked : false;
    post.savedByViewer = ctx_.savedPosts.search(key, &saved) ? saved : false;
  }

  std::vector<Row> media;
  ctx_.db.query("SELECT id, url, media_type, position FROM post_media "
                "WHERE post_id = ? ORDER BY position, id",
                {Param::of(post.id)}, &media, nullptr);
  for (const Row& item : media) {
    post.media.push_back({item.integer("id"), item.str("url"),
                          item.str("media_type"), item.intValue("position")});
  }

  std::vector<Row> tags;
  ctx_.db.query("SELECT h.tag FROM hashtags h JOIN post_hashtags ph "
                "ON ph.hashtag_id = h.id WHERE ph.post_id = ? ORDER BY h.tag",
                {Param::of(post.id)}, &tags, nullptr);
  for (const Row& tag : tags) post.hashtags.push_back(tag.str("tag"));
  (void)viewer;
  return post;
}

Story AppService::hydrateStory(const Row& row, long long viewer) const {
  Story story;
  story.id = row.integer("id");
  story.userId = row.integer("user_id");
  story.username = row.str("username");
  story.avatar = row.str("profile_picture");
  story.mediaUrl = row.str("media_url");
  story.mediaType = row.str("media_type");
  story.createdAt = row.str("created_at");
  story.expiresAt = row.str("expires_at");
  story.viewCount = row.integer("view_count");
  story.viewedByViewer = row.boolean("viewed");
  (void)viewer;
  return story;
}

Reel AppService::hydrateReel(const Row& row, long long viewer) const {
  Reel reel;
  reel.id = row.integer("id");
  reel.userId = row.integer("user_id");
  reel.username = row.str("username");
  reel.avatar = row.str("profile_picture");
  reel.videoUrl = row.str("video_url");
  reel.thumbnailUrl = row.str("thumbnail_url");
  reel.caption = row.str("caption");
  reel.views = row.integer("views_count");
  reel.likes = row.integer("like_count");
  reel.comments = row.integer("comment_count");
  reel.likedByViewer = row.boolean("liked");
  reel.savedByViewer = row.boolean("saved");
  reel.createdAt = row.str("created_at");
  (void)viewer;
  return reel;
}

void AppService::recordSearch(long long viewer, const std::string& query) {
  if (query.size() < 2) return;
  const std::string value = query.substr(0, 120);
  std::vector<Row> latest;
  ctx_.db.query("SELECT query FROM searches WHERE user_id = ? "
                "ORDER BY created_at DESC, id DESC LIMIT 1",
                {Param::of(viewer)}, &latest, nullptr);
  if (!latest.empty() && latest[0].str("query") == value) return;
  ctx_.db.execute("INSERT INTO searches (user_id, query) VALUES (?, ?)",
                  {Param::of(viewer), Param::of(value)}, nullptr);
}

std::vector<Row> AppService::fetchFeedRows(long long viewer,
                                           const std::string& mode) {
  std::vector<Row> rows;
  std::string where;
  std::vector<Param> ordered{Param::of(viewer), Param::of(viewer),
                             Param::of(viewer), Param::of(viewer)};
  // All feed modes respect privacy: own posts, public posts, and posts from
  // accounts the viewer follows are visible.
  where = "AND (p.user_id = ? OR u.is_private = 0 OR p.user_id IN "
          "(SELECT following_id FROM followers WHERE follower_id = ?))";
  ordered.push_back(Param::of(viewer));
  ordered.push_back(Param::of(viewer));
  if (mode == "following") {
    where += " AND (p.user_id IN (SELECT following_id FROM followers "
             "WHERE follower_id = ?) OR p.user_id = ?)";
    ordered.push_back(Param::of(viewer));
    ordered.push_back(Param::of(viewer));
  }

  const std::string sql =
      "SELECT p.id, p.user_id, p.caption, p.location, p.created_at, "
      "u.username, u.profile_picture, "
      "(SELECT COUNT(*) FROM likes l WHERE l.post_id = p.id) AS like_count, "
      "(SELECT COUNT(*) FROM comments c WHERE c.post_id = p.id) AS comment_count, "
      "EXISTS(SELECT 1 FROM likes l2 WHERE l2.post_id = p.id AND l2.user_id = ?) AS liked, "
      "EXISTS(SELECT 1 FROM saved_posts s WHERE s.post_id = p.id AND s.user_id = ?) AS saved "
      "FROM posts p JOIN users u ON u.id = p.user_id "
      "WHERE NOT EXISTS (SELECT 1 FROM blocked_users b WHERE "
      "(b.blocker_id = p.user_id AND b.blocked_id = ?) OR "
      "(b.blocker_id = ? AND b.blocked_id = p.user_id)) " +
      where + " ORDER BY p.created_at DESC LIMIT 200";

  // Placeholder order: liked, saved, block x2, privacy x2, then following x2.
  ctx_.db.query(sql, ordered, &rows, nullptr);
  return rows;
}

}  // namespace ig
