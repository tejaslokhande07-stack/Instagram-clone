#pragma once
// Application service layer: all business logic lives here. Controllers only
// translate HTTP <-> JSON. DSA structures are maintained as caches/indexes and
// are always backed by MySQL rows.
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "core/config.h"
#include "database/database.h"
#include "dsa/deque.hpp"
#include "dsa/dynamic_vector.hpp"
#include "dsa/graph.hpp"
#include "dsa/hash_set.hpp"
#include "dsa/hash_table.hpp"
#include "dsa/max_heap.hpp"
#include "dsa/queue.hpp"
#include "dsa/trie.hpp"
#include "models/models.h"
#include "nlohmann/json.hpp"

namespace ig {

using ig::dsa::DynamicVector;
using ig::dsa::FollowGraph;
using ig::dsa::HashTable;
using ig::dsa::HashSet;
using ig::dsa::HeapItem;
using ig::dsa::IdDeque;
using ig::dsa::IdQueue;
using ig::dsa::MaxHeap;
using ig::dsa::Trie;

using json = nlohmann::json;

json postToJson(const Post& post);
// Collision-safe key for viewer-scoped pair membership in HashTable caches.
std::string interactionKey(long long first, long long second);

struct ApiResult {
  int status = 200;
  json body = json::object();

  static ApiResult ok(json data = json::object());
  static ApiResult created(json data);
  static ApiResult fail(int status, const std::string& message);
};

struct AppContext {
  AppConfig config;
  Database db;

  // Application-level DSA indexes, synchronised with MySQL on every write.
  HashTable<std::string, long long> usernameIndex;
  HashTable<long long, std::string> idIndex;
  FollowGraph graph;                 // adjacency lists: A -> B means A follows B
  Trie usernameTrie;                 // autocomplete
  Trie hashtagTrie;                  // hashtag prefix search
  HashTable<std::string, bool> likedPosts;          // "viewer:post" like membership
  HashTable<std::string, bool> savedPosts;          // "viewer:post" save membership
  HashTable<std::string, bool> unreadConversations; // "viewer:conversation"
  IdDeque recentActivity;            // recently touched post/reel IDs
  IdDeque storyOrder;                // active story IDs, oldest -> newest
  HashSet activeStories;             // O(1) active-story membership for pruning
  IdQueue notificationQueue;         // pending notification events
  MaxHeap feedHeap;                  // scored feed items
  MaxHeap trendingReels;             // scored reel items
  MaxHeap trendingHashtags;          // scored hashtag items
  mutable std::mutex dsaMutex;

  bool initialize(const AppConfig& cfg, std::string* error);
  void rebuildIndexes();
  std::optional<long long> userIdFor(const std::string& username) const;
  std::string usernameFor(long long id) const;
};

class AppService {
 public:
  explicit AppService(AppContext& context) : ctx_(context) {}

  // ---- authentication -----------------------------------------------------
  ApiResult registerUser(const json& payload);
  ApiResult login(const json& payload);
  ApiResult logout(const std::string& token);
  ApiResult requestPasswordReset(const json& payload);
  ApiResult resetPassword(const json& payload);
  std::optional<long long> authenticate(const std::string& header) const;

  // ---- users / social graph ----------------------------------------------
  ApiResult getUser(long long id, long long viewer);
  ApiResult updateUser(long long id, long long viewer, const json& payload);
  ApiResult searchUsers(const std::string& query, long long viewer);
  ApiResult follow(long long target, long long viewer);
  ApiResult unfollow(long long target, long long viewer);
  ApiResult followStatus(long long target, long long viewer);
  ApiResult listFollowers(long long id, long long viewer);
  ApiResult listFollowing(long long id, long long viewer);
  ApiResult listRequests(long long viewer);
  ApiResult respondToRequest(long long requester, long long viewer, bool accept);
  ApiResult block(long long target, long long viewer);
  ApiResult unblock(long long target, long long viewer);
  ApiResult listBlocked(long long viewer);
  ApiResult report(const json& payload, long long viewer);
  ApiResult mutualFollowers(long long other, long long viewer);
  ApiResult suggestedUsers(long long viewer);
  ApiResult suggestedUsers(long long target, long long viewer);

  // ---- posts / feed / explore --------------------------------------------
  ApiResult createPost(long long viewer, const json& payload);
  ApiResult getFeed(long long viewer, int page, int limit,
                    const std::string& mode);
  ApiResult getPost(long long id, long long viewer);
  ApiResult getPostsByUser(long long id, long long viewer, int page, int limit);
  ApiResult updatePost(long long id, long long viewer, const json& payload);
  ApiResult deletePost(long long id, long long viewer);
  ApiResult likePost(long long id, long long viewer);
  ApiResult unlikePost(long long id, long long viewer);
  ApiResult addComment(long long id, long long viewer, const json& payload);
  ApiResult getComments(long long id, long long viewer);
  ApiResult deleteComment(long long id, long long viewer);
  ApiResult savePost(long long id, long long viewer);
  ApiResult unsavePost(long long id, long long viewer);
  ApiResult savedPosts(long long viewer);
  ApiResult explore(long long viewer, const std::string& tag, int page,
                    int limit);
  ApiResult trendingHashtags();

  // ---- stories ------------------------------------------------------------
  ApiResult createStory(long long viewer, const json& payload);
  ApiResult getStories(long long viewer);
  ApiResult viewStory(long long id, long long viewer);

  // ---- reels --------------------------------------------------------------
  ApiResult createReel(long long viewer, const json& payload);
  ApiResult getReels(long long viewer, int page, int limit,
                     const std::string& mode);
  ApiResult likeReel(long long id, long long viewer);
  ApiResult unlikeReel(long long id, long long viewer);
  ApiResult commentReel(long long id, long long viewer, const json& payload);
  ApiResult getReelComments(long long id, long long viewer);
  ApiResult saveReel(long long id, long long viewer);
  ApiResult unsaveReel(long long id, long long viewer);
  ApiResult viewReel(long long id, long long viewer);
  ApiResult savedReels(long long viewer);

  // ---- messaging ----------------------------------------------------------
  ApiResult listConversations(long long viewer);
  ApiResult searchConversations(long long viewer, const std::string& query);
  ApiResult getOrCreateConversation(long long other, long long viewer);
  ApiResult getMessages(long long conversationId, long long viewer, int before,
                        int limit);
  ApiResult sendMessage(long long viewer, const json& payload);
  ApiResult markConversationRead(long long conversationId, long long viewer);
  ApiResult setTyping(long long conversationId, long long viewer, bool typing);
  ApiResult unreadConversationCount(long long viewer);

  // ---- notifications / search --------------------------------------------
  ApiResult getNotifications(long long viewer);
  ApiResult unreadNotifications(long long viewer);
  ApiResult markNotificationRead(long long id, long long viewer);
  ApiResult markAllNotificationsRead(long long viewer);
  ApiResult search(const std::string& query, long long viewer);
  ApiResult recentSearches(long long viewer);

  // ---- media --------------------------------------------------------------
  ApiResult uploadMedia(long long viewer, const json& payload);

 private:
  AppContext& ctx_;

  bool userById(long long id, User* out, std::string* error = nullptr) const;
  json publicUser(const User& user) const;
  json publicUserById(long long id) const;
  bool isBlocked(long long a, long long b) const;
  bool canView(long long owner, long long viewer) const;
  bool isFollowing(long long follower, long long target) const;
  bool canViewPost(long long postId, long long viewer, long long* owner = nullptr) const;
  long long countLikes(long long postId) const;
  long long countComments(long long postId) const;
  void touchRecentPost(long long postId);
  void forgetPostCaches(long long postId);
  void pruneExpiredStories();
  void notify(long long recipient, long long actor, const std::string& type,
              long long entityId, const std::string& body);
  void rebuildNotificationQueue();
  void rebuildUnreadConversations();
  std::string saveBase64File(const std::string& data, const std::string& filename,
                             const std::string& fallbackExt);
  bool removeUploadedFile(const std::string& url);
  void syncHashtags(long long postId, const std::string& caption);
  void replaceHashtags(long long postId, const std::string& caption);
  Post hydratePost(const Row& row, long long viewer) const;
  Story hydrateStory(const Row& row, long long viewer) const;
  Reel hydrateReel(const Row& row, long long viewer) const;
  void recordSearch(long long viewer, const std::string& query);
  std::vector<Row> fetchFeedRows(long long viewer, const std::string& mode);
};

}  // namespace ig
