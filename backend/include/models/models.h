#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace ig {

struct User {
  long long id = 0;
  std::string username;
  std::string email;
  std::string fullName;
  std::string bio;
  std::string website;
  std::string profilePicture;
  bool isPrivate = false;
  bool isVerified = false;
  std::string createdAt;
};

struct PostMedia {
  long long id = 0;
  std::string url;
  std::string type = "image";
  int position = 0;
};

struct Comment {
  long long id = 0;
  long long postId = 0;
  long long userId = 0;
  std::string username;
  std::string avatar;
  std::string body;
  std::string createdAt;
};

struct Post {
  long long id = 0;
  long long userId = 0;
  std::string username;
  std::string avatar;
  std::string caption;
  std::string location;
  std::string createdAt;
  std::vector<PostMedia> media;
  std::vector<std::string> hashtags;
  long long likeCount = 0;
  long long commentCount = 0;
  bool likedByViewer = false;
  bool savedByViewer = false;
  double score = 0.0;
};

struct Story {
  long long id = 0;
  long long userId = 0;
  std::string username;
  std::string avatar;
  std::string mediaUrl;
  std::string mediaType = "image";
  std::string createdAt;
  std::string expiresAt;
  long long viewCount = 0;
  bool viewedByViewer = false;
};

struct Reel {
  long long id = 0;
  long long userId = 0;
  std::string username;
  std::string avatar;
  std::string videoUrl;
  std::string thumbnailUrl;
  std::string caption;
  long long views = 0;
  long long likes = 0;
  long long comments = 0;
  bool likedByViewer = false;
  bool savedByViewer = false;
  std::string createdAt;
  double score = 0.0;
};

struct Conversation {
  long long id = 0;
  long long otherId = 0;
  std::string otherUsername;
  std::string otherAvatar;
  bool otherOnline = false;
  std::string lastMessage;
  std::string lastMessageAt;
  long long unreadCount = 0;
};

struct Message {
  long long id = 0;
  long long conversationId = 0;
  long long senderId = 0;
  long long receiverId = 0;
  std::string body;
  std::string mediaUrl;
  bool isRead = false;
  bool typing = false;
  std::string createdAt;
};

struct NotificationItem {
  long long id = 0;
  long long actorId = 0;
  std::string actorUsername;
  std::string actorAvatar;
  std::string type;
  long long entityId = 0;
  std::string body;
  bool isRead = false;
  std::string createdAt;
};

}  // namespace ig
