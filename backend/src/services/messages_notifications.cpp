#include "services/app_service.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <unordered_set>

namespace ig {

static std::string normalizeSearchQuery(const std::string& raw) {
  std::size_t first = 0;
  while (first < raw.size() &&
         std::isspace(static_cast<unsigned char>(raw[first]))) {
    ++first;
  }
  std::size_t last = raw.size();
  while (last > first &&
         std::isspace(static_cast<unsigned char>(raw[last - 1]))) {
    --last;
  }
  std::string value = raw.substr(first, last - first);
  if (!value.empty() && value.front() == '#') value.erase(value.begin());
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

static json messageToJson(const Row& row) {
  return {{"id", row.integer("id")},
          {"conversationId", row.integer("conversation_id")},
          {"senderId", row.integer("sender_id")},
          {"receiverId", row.integer("receiver_id")},
          {"body", row.str("body")},
          {"mediaUrl", row.str("media_url")},
          {"isRead", row.boolean("is_read")},
          {"createdAt", row.str("created_at")}};
}

void AppService::rebuildNotificationQueue() {
  std::vector<Row> rows;
  if (!ctx_.db.query("SELECT id FROM notifications WHERE is_read = 0 "
                     "ORDER BY created_at DESC LIMIT 100",
                     {}, &rows, nullptr)) {
    return;
  }
  std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
  ctx_.notificationQueue.clear();
  for (const Row& row : rows) ctx_.notificationQueue.enqueue(row.integer("id"));
}

void AppService::rebuildUnreadConversations() {
  std::vector<Row> rows;
  if (!ctx_.db.query("SELECT DISTINCT receiver_id, conversation_id FROM messages "
                     "WHERE is_read = 0 AND typing = 0",
                     {}, &rows, nullptr)) {
    return;
  }
  std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
  ctx_.unreadConversations.clear();
  for (const Row& row : rows) {
    ctx_.unreadConversations.set(
        interactionKey(row.integer("receiver_id"),
                       row.integer("conversation_id")),
        true);
  }
}

ApiResult AppService::listConversations(long long viewer) {
  const std::string sql =
      "SELECT c.id, c.participant1, c.participant2, u.id AS other_id, "
      "u.username AS other_username, u.profile_picture AS other_avatar, "
      "u.last_seen, "
      "(SELECT CASE WHEN m.body = '' THEN '[Photo]' ELSE m.body END "
      "FROM messages m WHERE m.conversation_id = c.id "
      "AND m.typing = 0 ORDER BY m.id DESC LIMIT 1) AS last_message, "
      "(SELECT MAX(m.created_at) FROM messages m WHERE m.conversation_id = c.id "
      "AND m.typing = 0) AS last_at, "
      "(SELECT COUNT(*) FROM messages m2 WHERE m2.conversation_id = c.id "
      "AND m2.receiver_id = ? AND m2.is_read = 0 AND m2.typing = 0) AS unread "
      "FROM conversations c JOIN users u ON u.id = "
      "CASE WHEN c.participant1 = ? THEN c.participant2 ELSE c.participant1 END "
      "WHERE c.participant1 = ? OR c.participant2 = ? "
      "ORDER BY COALESCE(last_at, c.created_at) DESC LIMIT 100";
  std::vector<Row> rows;
  ctx_.db.query(sql, {Param::of(viewer), Param::of(viewer), Param::of(viewer),
                      Param::of(viewer)},
                &rows, nullptr);

  json result = json::array();
  for (const Row& row : rows) {
    std::time_t lastSeen = 0;
    const std::string seen = row.str("last_seen");
    if (seen.size() >= 19) {
      std::tm tm{};
      std::istringstream ss(seen.substr(0, 19));
      ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
      if (!ss.fail()) lastSeen = std::mktime(&tm);
    }
    const bool online = lastSeen != 0 &&
                        std::difftime(std::time(nullptr), lastSeen) < 300;
    result.push_back({{"id", row.integer("id")},
                      {"otherId", row.integer("other_id")},
                      {"otherUsername", row.str("other_username")},
                      {"otherAvatar", row.str("other_avatar")},
                      {"online", online},
                      {"lastMessage", row.str("last_message")},
                      {"lastMessageAt", row.str("last_at")},
                      {"unreadCount", row.integer("unread")}});
  }
  return ApiResult::ok({{"conversations", result}});
}

ApiResult AppService::searchConversations(long long viewer,
                                          const std::string& query) {
  auto result = listConversations(viewer);
  if (!result.body.contains("conversations") || query.empty()) return result;
  std::string needle = query;
  std::transform(needle.begin(), needle.end(), needle.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  json filtered = json::array();
  for (const auto& item : result.body["conversations"]) {
    std::string name = item.value("otherUsername", "");
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (name.find(needle) != std::string::npos) filtered.push_back(item);
  }
  result.body["conversations"] = filtered;
  return result;
}

ApiResult AppService::getOrCreateConversation(long long other, long long viewer) {
  if (other == viewer) return ApiResult::fail(400, "Cannot message yourself");
  std::vector<Row> user;
  ctx_.db.query("SELECT id FROM users WHERE id = ?", {Param::of(other)}, &user,
                nullptr);
  if (user.empty()) return ApiResult::fail(404, "User not found");
  if (isBlocked(other, viewer)) return ApiResult::fail(403, "Blocked user");

  std::vector<Row> rows;
  ctx_.db.query("SELECT id FROM conversations WHERE "
                "(participant1 = ? AND participant2 = ?) OR "
                "(participant1 = ? AND participant2 = ?) LIMIT 1",
                {Param::of(viewer), Param::of(other), Param::of(other),
                 Param::of(viewer)},
                &rows, nullptr);
  std::uint64_t id = 0;
  if (rows.empty()) {
    if (!ctx_.db.execute("INSERT INTO conversations (participant1, participant2) "
                         "VALUES (?, ?)",
                         {Param::of(std::min(viewer, other)),
                          Param::of(std::max(viewer, other))},
                         nullptr, &id)) {
      return ApiResult::fail(500, "Could not create conversation");
    }
  } else {
    id = static_cast<std::uint64_t>(rows[0].integer("id"));
  }
  return ApiResult::ok({{"conversationId", static_cast<long long>(id)}});
}

ApiResult AppService::getMessages(long long conversationId, long long viewer,
                                  int before, int limit) {
  std::vector<Row> participants;
  ctx_.db.query("SELECT participant1, participant2 FROM conversations WHERE id = ?",
                {Param::of(conversationId)}, &participants, nullptr);
  if (participants.empty()) return ApiResult::fail(404, "Conversation not found");
  if (participants[0].integer("participant1") != viewer &&
      participants[0].integer("participant2") != viewer) {
    return ApiResult::fail(403, "Not a participant");
  }
  limit = std::min(std::max(limit, 10), 100);
  std::string sql =
      "SELECT id, conversation_id, sender_id, receiver_id, body, media_url, "
      "is_read, created_at FROM messages WHERE conversation_id = ? "
      "AND typing = 0 ";
  std::vector<Param> params{Param::of(conversationId)};
  if (before > 0) {
    sql += "AND id < ? ";
    params.push_back(Param::of(before));
  }
  sql += "ORDER BY id DESC LIMIT ?";
  params.push_back(Param::of(limit));
  std::vector<Row> rows;
  ctx_.db.query(sql, params, &rows, nullptr);
  std::reverse(rows.begin(), rows.end());

  json messages = json::array();
  for (const Row& row : rows) messages.push_back(messageToJson(row));

  std::vector<Row> typingRows;
  ctx_.db.query("SELECT sender_id FROM messages WHERE conversation_id = ? "
                "AND typing = 1 AND sender_id != ? "
                "AND created_at > DATE_SUB(NOW(), INTERVAL 20 SECOND) "
                "ORDER BY id DESC LIMIT 1",
                {Param::of(conversationId), Param::of(viewer)}, &typingRows,
                nullptr);
  return ApiResult::ok({{"messages", messages},
                        {"typing", !typingRows.empty()}});
}

ApiResult AppService::sendMessage(long long viewer, const json& payload) {
  long long conversationId = payload.value("conversationId", 0LL);
  const long long recipientId = payload.value("recipientId", 0LL);
  if (conversationId == 0 && recipientId != 0) {
    auto conversation = getOrCreateConversation(recipientId, viewer);
    if (conversation.status != 200) return conversation;
    conversationId = conversation.body.value("conversationId", 0LL);
  }
  if (conversationId == 0) return ApiResult::fail(400, "Conversation required");

  std::string body = payload.value("body", "");
  std::string mediaUrl;
  if (payload.contains("mediaData") && !payload.value("mediaData", "").empty()) {
    mediaUrl = saveBase64File(payload.value("mediaData", ""),
                              payload.value("filename", "image.jpg"), "jpg");
  }
  if (body.empty() && mediaUrl.empty()) {
    return ApiResult::fail(400, "Message cannot be empty");
  }
  std::vector<Row> participants;
  ctx_.db.query("SELECT participant1, participant2 FROM conversations WHERE id = ?",
                {Param::of(conversationId)}, &participants, nullptr);
  if (participants.empty()) return ApiResult::fail(404, "Conversation not found");
  const long long other = participants[0].integer("participant1") == viewer
                              ? participants[0].integer("participant2")
                              : participants[0].integer("participant1");

  std::uint64_t id = 0;
  if (!ctx_.db.execute("INSERT INTO messages (conversation_id, sender_id, "
                       "receiver_id, body, media_url) VALUES (?, ?, ?, ?, ?)",
                       {Param::of(conversationId), Param::of(viewer),
                        Param::of(other), Param::of(body.substr(0, 4000)),
                        Param::of(mediaUrl)},
                       nullptr, &id)) {
    return ApiResult::fail(500, "Could not send message");
  }
  ctx_.db.execute("UPDATE conversations SET updated_at = NOW() WHERE id = ?",
                  {Param::of(conversationId)}, nullptr);
  // Sending a real message supersedes any stale typing indicator from this sender.
  ctx_.db.execute("DELETE FROM messages WHERE conversation_id = ? "
                  "AND sender_id = ? AND typing = 1",
                  {Param::of(conversationId), Param::of(viewer)}, nullptr);
  {
    std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
    ctx_.unreadConversations.set(interactionKey(other, conversationId), true);
  }
  notify(other, viewer, "message", conversationId,
         ctx_.usernameFor(viewer) + " sent you a message");

  std::vector<Row> rows;
  ctx_.db.query("SELECT id, conversation_id, sender_id, receiver_id, body, "
                "media_url, is_read, created_at FROM messages WHERE id = ?",
                {Param::of(static_cast<long long>(id))}, &rows, nullptr);
  return ApiResult::created({{"message", rows.empty() ? json::object()
                                                     : messageToJson(rows[0])}});
}

ApiResult AppService::markConversationRead(long long conversationId,
                                           long long viewer) {
  ctx_.db.execute("UPDATE messages SET is_read = 1 WHERE conversation_id = ? "
                  "AND receiver_id = ? AND is_read = 0",
                  {Param::of(conversationId), Param::of(viewer)}, nullptr);
  {
    std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
    ctx_.unreadConversations.remove(interactionKey(viewer, conversationId));
  }
  return ApiResult::ok({{"read", true}});
}

ApiResult AppService::setTyping(long long conversationId, long long viewer,
                                bool typing) {
  std::vector<Row> participants;
  ctx_.db.query("SELECT participant1, participant2 FROM conversations WHERE id = ?",
                {Param::of(conversationId)}, &participants, nullptr);
  if (participants.empty()) return ApiResult::fail(404, "Conversation not found");
  const long long p1 = participants[0].integer("participant1");
  const long long p2 = participants[0].integer("participant2");
  if (p1 != viewer && p2 != viewer) {
    return ApiResult::fail(403, "Not a participant");
  }
  if (!typing) {
    ctx_.db.execute("DELETE FROM messages WHERE conversation_id = ? "
                    "AND sender_id = ? AND typing = 1",
                    {Param::of(conversationId), Param::of(viewer)}, nullptr);
    return ApiResult::ok({{"typing", false}});
  }
  const long long other = (p1 == viewer) ? p2 : p1;
  ctx_.db.execute("DELETE FROM messages WHERE conversation_id = ? "
                  "AND sender_id = ? AND typing = 1",
                  {Param::of(conversationId), Param::of(viewer)}, nullptr);
  ctx_.db.execute("INSERT INTO messages (conversation_id, sender_id, receiver_id, "
                  "body, typing) VALUES (?, ?, ?, '', 1)",
                  {Param::of(conversationId), Param::of(viewer), Param::of(other)},
                  nullptr);
  return ApiResult::ok({{"typing", true}});
}

ApiResult AppService::unreadConversationCount(long long viewer) {
  // Rebuild from MySQL first so the cache is authoritative after restarts or
  // external changes, then consume the cache to answer the request.
  rebuildUnreadConversations();
  const std::string prefix = std::to_string(viewer) + ":";
  int count = 0;
  {
    std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
    ctx_.unreadConversations.forEach([&](const std::string& key, bool present) {
      if (present && key.rfind(prefix, 0) == 0) ++count;
    });
  }
  return ApiResult::ok({{"count", count}});
}

ApiResult AppService::getNotifications(long long viewer) {
  std::vector<Row> rows;
  ctx_.db.query("SELECT n.id, n.actor_id, n.type, n.entity_id, n.body, "
                "n.is_read, n.created_at, u.username, u.profile_picture "
                "FROM notifications n JOIN users u ON u.id = n.actor_id "
                "WHERE n.user_id = ? ORDER BY n.created_at DESC LIMIT 100",
                {Param::of(viewer)}, &rows, nullptr);
  json result = json::array();
  for (const Row& row : rows) {
    result.push_back({{"id", row.integer("id")},
                      {"actorId", row.integer("actor_id")},
                      {"actorUsername", row.str("username")},
                      {"actorAvatar", row.str("profile_picture")},
                      {"type", row.str("type")},
                      {"entityId", row.integer("entity_id")},
                      {"body", row.str("body")},
                      {"isRead", row.boolean("is_read")},
                      {"createdAt", row.str("created_at")}});
  }
  return ApiResult::ok({{"notifications", result}});
}

ApiResult AppService::unreadNotifications(long long viewer) {
  rebuildNotificationQueue();
  std::vector<Row> rows;
  ctx_.db.query("SELECT id FROM notifications WHERE user_id = ? AND is_read = 0",
                {Param::of(viewer)}, &rows, nullptr);
  std::unordered_set<long long> viewerIds;
  for (const Row& row : rows) viewerIds.insert(row.integer("id"));
  int count = 0;
  {
    std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
    ctx_.notificationQueue.forEach([&](long long id) {
      if (viewerIds.count(id)) ++count;
    });
  }
  return ApiResult::ok({{"count", count}});
}

ApiResult AppService::markNotificationRead(long long id, long long viewer) {
  std::uint64_t affected = 0;
  ctx_.db.executeCount("UPDATE notifications SET is_read = 1 "
                       "WHERE id = ? AND user_id = ?",
                       {Param::of(id), Param::of(viewer)}, nullptr, &affected);
  if (affected == 0) return ApiResult::fail(404, "Notification not found");
  rebuildNotificationQueue();
  return ApiResult::ok({{"read", true}});
}

ApiResult AppService::markAllNotificationsRead(long long viewer) {
  ctx_.db.execute("UPDATE notifications SET is_read = 1 WHERE user_id = ?",
                  {Param::of(viewer)}, nullptr);
  rebuildNotificationQueue();
  return ApiResult::ok({{"read", true}});
}

ApiResult AppService::search(const std::string& rawQuery, long long viewer) {
  const std::string query = normalizeSearchQuery(rawQuery);
  if (query.empty()) {
    return ApiResult::ok({{"users", json::array()},
                          {"hashtags", json::array()},
                          {"posts", json::array()}});
  }
  recordSearch(viewer, query);
  auto usersResult = searchUsers(query, viewer);
  // Hashtag suggestions come from the custom Trie; MySQL supplies the live
  // usage counts and filters out tags whose last post was removed.
  std::vector<std::string> candidates;
  {
    std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
    candidates = ctx_.hashtagTrie.startsWith(query, 20);
  }
  json hashtags = json::array();
  if (!candidates.empty()) {
    std::string placeholders;
    std::vector<Param> tagParams;
    for (std::size_t i = 0; i < candidates.size(); ++i) {
      if (i != 0) placeholders += ",";
      placeholders += "?";
      tagParams.push_back(Param::of(candidates[i]));
    }
    std::vector<Row> tagRows;
    ctx_.db.query("SELECT tag, usage_count FROM hashtags WHERE usage_count > 0 "
                  "AND tag IN (" + placeholders + ") "
                  "ORDER BY usage_count DESC, tag ASC LIMIT 10",
                  tagParams, &tagRows, nullptr);
    for (const Row& row : tagRows) {
      hashtags.push_back({{"tag", row.str("tag")},
                          {"count", row.integer("usage_count")}});
    }
  }

  json posts = json::array();
  std::vector<Row> postRows;
  ctx_.db.query(
      "SELECT p.id, p.user_id, p.caption, p.location, p.created_at, "
      "u.username, u.profile_picture, "
      "(SELECT COUNT(*) FROM likes l WHERE l.post_id = p.id) AS like_count, "
      "(SELECT COUNT(*) FROM comments c WHERE c.post_id = p.id) AS comment_count, "
      "EXISTS(SELECT 1 FROM likes l2 WHERE l2.post_id = p.id AND l2.user_id = ?) AS liked, "
      "EXISTS(SELECT 1 FROM saved_posts s WHERE s.post_id = p.id AND s.user_id = ?) AS saved "
      "FROM posts p JOIN users u ON u.id = p.user_id "
      "WHERE (p.caption LIKE ? OR EXISTS (SELECT 1 FROM post_hashtags ph "
      "JOIN hashtags h ON h.id = ph.hashtag_id "
      "WHERE ph.post_id = p.id AND h.tag = ?)) "
      "AND NOT EXISTS (SELECT 1 FROM blocked_users b WHERE "
      "(b.blocker_id = p.user_id AND b.blocked_id = ?) OR "
      "(b.blocker_id = ? AND b.blocked_id = p.user_id)) "
      "AND (p.user_id = ? OR u.is_private = 0 OR p.user_id IN "
      "(SELECT following_id FROM followers WHERE follower_id = ?)) "
      "ORDER BY p.created_at DESC LIMIT 12",
      {Param::of(viewer), Param::of(viewer), Param::of("%" + query + "%"),
       Param::of(query), Param::of(viewer), Param::of(viewer),
       Param::of(viewer), Param::of(viewer)},
      &postRows, nullptr);
  for (const Row& row : postRows) {
    posts.push_back(postToJson(hydratePost(row, viewer)));
  }
  return ApiResult::ok({{"users", usersResult.body.value("results", json::array())},
                        {"hashtags", hashtags},
                        {"posts", posts}});
}

ApiResult AppService::recentSearches(long long viewer) {
  std::vector<Row> rows;
  ctx_.db.query("SELECT query, MAX(created_at) AS last_at FROM searches "
                "WHERE user_id = ? GROUP BY query "
                "ORDER BY last_at DESC, query ASC LIMIT 10",
                {Param::of(viewer)}, &rows, nullptr);
  json result = json::array();
  for (const Row& row : rows) result.push_back(row.str("query"));
  return ApiResult::ok({{"queries", result}});
}

ApiResult AppService::uploadMedia(long long viewer, const json& payload) {
  (void)viewer;
  const std::string url = saveBase64File(
      payload.value("data", ""), payload.value("filename", "image.jpg"), "jpg");
  if (url.empty()) return ApiResult::fail(400, "Invalid upload");
  return ApiResult::created({{"url", url}});
}

}  // namespace ig
