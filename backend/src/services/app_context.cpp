#include "services/app_service.h"

#include <filesystem>
#include <iostream>

namespace ig {

bool AppContext::initialize(const AppConfig& cfg, std::string* error) {
  config = cfg;
  if (!db.initialize(cfg, error)) return false;
  std::error_code ec;
  std::filesystem::create_directories(cfg.uploadDir, ec);
  rebuildIndexes();
  return true;
}

void AppContext::rebuildIndexes() {
  std::lock_guard<std::mutex> lock(dsaMutex);
  usernameIndex = HashTable<std::string, long long>{};
  idIndex = HashTable<long long, std::string>{};
  graph = FollowGraph{};
  usernameTrie = Trie{};
  hashtagTrie = Trie{};
  likedPosts.clear();
  savedPosts.clear();
  unreadConversations.clear();
  recentActivity.clear();
  storyOrder.clear();
  activeStories.clear();
  notificationQueue.clear();
  feedHeap.clear();
  trendingReels.clear();
  trendingHashtags.clear();

  std::vector<Row> users;
  std::string error;
  if (db.query("SELECT id, username FROM users", {}, &users, &error)) {
    for (const Row& row : users) {
      long long id = row.integer("id");
      std::string name = row.str("username");
      usernameIndex.set(name, id);
      idIndex.set(id, name);
      usernameTrie.insert(name);
    }
  } else {
    std::cerr << "[DSA] user index rebuild failed: " << error << '\n';
  }

  std::vector<Row> tags;
  if (db.query("SELECT id, tag, usage_count FROM hashtags WHERE usage_count > 0",
               {}, &tags, &error)) {
    for (const Row& row : tags) {
      hashtagTrie.insert(row.str("tag"));
      trendingHashtags.insert(row.integer("id"),
                              static_cast<double>(row.integer("usage_count")));
    }
  } else {
    std::cerr << "[DSA] hashtag index rebuild failed: " << error << '\n';
  }

  std::vector<Row> follows;
  if (db.query("SELECT follower_id, following_id FROM followers", {}, &follows,
               &error)) {
    for (const Row& row : follows) {
      graph.addEdge(row.integer("follower_id"), row.integer("following_id"));
    }
  }

  std::vector<Row> likes;
  if (db.query("SELECT user_id, post_id FROM likes", {}, &likes, &error)) {
    for (const Row& row : likes) {
      likedPosts.set(interactionKey(row.integer("user_id"),
                                    row.integer("post_id")), true);
    }
  }

  std::vector<Row> saves;
  if (db.query("SELECT user_id, post_id FROM saved_posts", {}, &saves, &error)) {
    for (const Row& row : saves) {
      savedPosts.set(interactionKey(row.integer("user_id"),
                                    row.integer("post_id")), true);
    }
  }

  std::vector<Row> unread;
  if (db.query("SELECT DISTINCT receiver_id, conversation_id FROM messages "
               "WHERE is_read = 0 AND typing = 0",
               {}, &unread, &error)) {
    for (const Row& row : unread) {
      unreadConversations.set(interactionKey(row.integer("receiver_id"),
                                             row.integer("conversation_id")), true);
    }
  }

  std::vector<Row> posts;
  if (db.query("SELECT id FROM posts ORDER BY created_at DESC LIMIT 100", {},
               &posts, &error)) {
    for (const Row& row : posts) recentActivity.pushBack(row.integer("id"));
  }

  std::vector<Row> stories;
  if (db.query("SELECT id FROM stories WHERE expires_at > NOW() "
               "ORDER BY created_at ASC, id ASC",
               {}, &stories, &error)) {
    for (const Row& row : stories) {
      const long long id = row.integer("id");
      if (activeStories.insert(id)) storyOrder.pushBack(id);
    }
  }

  std::vector<Row> notifications;
  if (db.query("SELECT id FROM notifications WHERE is_read = 0 "
               "ORDER BY created_at DESC LIMIT 100",
               {}, &notifications, &error)) {
    for (const Row& row : notifications) {
      notificationQueue.enqueue(row.integer("id"));
    }
  }
}

std::optional<long long> AppContext::userIdFor(const std::string& username) const {
  long long id = 0;
  if (usernameIndex.search(username, &id)) return id;
  return std::nullopt;
}

std::string AppContext::usernameFor(long long id) const {
  std::string name;
  idIndex.search(id, &name);
  return name;
}

ApiResult ApiResult::ok(json data) {
  ApiResult result;
  result.status = 200;
  result.body = std::move(data);
  return result;
}

ApiResult ApiResult::created(json data) {
  ApiResult result;
  result.status = 201;
  result.body = std::move(data);
  return result;
}

ApiResult ApiResult::fail(int status, const std::string& message) {
  ApiResult result;
  result.status = status;
  result.body = {{"error", message}};
  return result;
}

}  // namespace ig
