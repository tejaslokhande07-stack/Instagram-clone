-- ============================================================================
-- Instagram Clone - reproducible demo data
-- Run after schema.sql. All demo accounts use password: Pass@123
-- Passwords are stored as Argon2id PHC strings (never plain text).
-- ============================================================================

USE instagram_clone;
SET FOREIGN_KEY_CHECKS = 0;

DELETE FROM searches;
DELETE FROM notifications;
DELETE FROM messages;
DELETE FROM conversations;
DELETE FROM reports;
DELETE FROM blocked_users;
DELETE FROM saved_posts;
DELETE FROM post_hashtags;
DELETE FROM hashtags;
DELETE FROM reel_saves;
DELETE FROM reel_comments;
DELETE FROM reel_likes;
DELETE FROM reel_views;
DELETE FROM reels;
DELETE FROM story_views;
DELETE FROM stories;
DELETE FROM comments;
DELETE FROM likes;
DELETE FROM follow_requests;
DELETE FROM followers;
DELETE FROM post_media;
DELETE FROM posts;
DELETE FROM password_resets;
DELETE FROM sessions;
DELETE FROM user_profiles;
DELETE FROM users;

SET FOREIGN_KEY_CHECKS = 1;

-- ---------------------------------------------------------------------------
-- Demo users (ids are stable so the seed is easy to reason about)
-- ---------------------------------------------------------------------------
INSERT INTO users
  (id, username, email, password_hash, full_name, bio, website,
   profile_picture, is_private, is_verified, last_seen, created_at)
VALUES
  (1, 'alice', 'alice@example.com',
   '$argon2id$v=19$m=32768,t=3,p=2$Qkt0ZkZlaWZBVXBZU1dDdWk0Y3Jndw$0Lev0Khbxjql79s4o+5R81buULuug4E2v3ECeyNSAKI',
   'Alice Smith', 'Photographer. Chasing golden hour.', 'https://alice.example',
   '/uploads/avatar-alice.svg', 0, 1, NOW(), DATE_SUB(NOW(), INTERVAL 90 DAY)),
  (2, 'bob', 'bob@example.com',
   '$argon2id$v=19$m=32768,t=3,p=2$Q0EtcFRsdTE4SWViX3R4SGRkN3BDZw$CoQ/zqeAogq4T97v+zvVVkv+HIcftGjLLwqXpLXFHrA',
   'Bob Chen', 'Runner, coffee enthusiast, occasional photographer.', '',
   '/uploads/avatar-bob.svg', 0, 0, NOW(), DATE_SUB(NOW(), INTERVAL 80 DAY)),
  (3, 'charlie', 'charlie@example.com',
   '$argon2id$v=19$m=32768,t=3,p=2$bmNqLTZTVVM2cGRMX1BiaEhtVGNZdw$09ML2FJ5fI4Ru9Geb2U23/k8OLbY1YbwSQwqpDWW3mU',
   'Charlie Rivera', 'Music producer | making noisy things quietly.', 'https://charlie.example',
   '/uploads/avatar-charlie.svg', 0, 0, DATE_SUB(NOW(), INTERVAL 2 MINUTE), DATE_SUB(NOW(), INTERVAL 70 DAY)),
  (4, 'diana', 'diana@example.com',
   '$argon2id$v=19$m=32768,t=3,p=2$NjVLRTFGSm1WYWRJZ3FXU1dZNUprUQ$MV6DxqKXm/3Y15N0+RsVlyxiJxtwTZAPhiIblAUKczA',
   'Diana Prince', 'Private adventures and small moments.', '',
   '/uploads/avatar-diana.svg', 1, 0, DATE_SUB(NOW(), INTERVAL 10 MINUTE), DATE_SUB(NOW(), INTERVAL 60 DAY)),
  (5, 'admin', 'admin@example.com',
   '$argon2id$v=19$m=32768,t=3,p=2$bHM0UkpQTUZzNlNIMHhOWXAxWU80UQ$Dyg0kcCEVqUdGe3cYUOeM93LK/j05ZxZZQiypRsKWUE',
   'Instagram Admin', 'Welcome to the demo community.', '',
   '/uploads/avatar-admin.svg', 0, 1, NOW(), DATE_SUB(NOW(), INTERVAL 100 DAY));

INSERT INTO user_profiles (user_id, phone, gender, location, show_email)
VALUES
  (1, '', 'female', 'San Francisco', 0),
  (2, '', 'male', 'Seattle', 0),
  (3, '', 'male', 'Austin', 1),
  (4, '', 'female', 'New York', 0),
  (5, '', '', 'Internet', 1);

-- ---------------------------------------------------------------------------
-- Directed follow graph: follower_id -> following_id
-- ---------------------------------------------------------------------------
INSERT INTO followers (follower_id, following_id, created_at) VALUES
  (1, 2, DATE_SUB(NOW(), INTERVAL 40 DAY)),
  (1, 3, DATE_SUB(NOW(), INTERVAL 35 DAY)),
  (2, 1, DATE_SUB(NOW(), INTERVAL 38 DAY)),
  (2, 3, DATE_SUB(NOW(), INTERVAL 30 DAY)),
  (3, 1, DATE_SUB(NOW(), INTERVAL 33 DAY)),
  (3, 2, DATE_SUB(NOW(), INTERVAL 29 DAY)),
  (4, 1, DATE_SUB(NOW(), INTERVAL 25 DAY)),
  (4, 2, DATE_SUB(NOW(), INTERVAL 20 DAY)),
  (5, 1, DATE_SUB(NOW(), INTERVAL 15 DAY)),
  (5, 2, DATE_SUB(NOW(), INTERVAL 14 DAY)),
  (5, 3, DATE_SUB(NOW(), INTERVAL 13 DAY)),
  (2, 5, DATE_SUB(NOW(), INTERVAL 12 DAY));

INSERT INTO follow_requests (requester_id, target_id, status, created_at) VALUES
  (1, 4, 'pending', DATE_SUB(NOW(), INTERVAL 2 HOUR));

-- ---------------------------------------------------------------------------
-- Posts and media
-- ---------------------------------------------------------------------------
INSERT INTO posts (id, user_id, caption, location, created_at) VALUES
  (1, 1, 'Golden hour on the coast. Still thinking about this light. #travel #sunset',
   'Big Sur, CA', DATE_SUB(NOW(), INTERVAL 3 HOUR)),
  (2, 2, 'Weekend reset: 10 km, good coffee, no notifications. #weekend #life',
   'Seattle, WA', DATE_SUB(NOW(), INTERVAL 8 HOUR)),
  (3, 3, 'New studio setup is finally alive. Turn it up. #night #city #music',
   'Austin, TX', DATE_SUB(NOW(), INTERVAL 26 HOUR)),
  (4, 4, 'Morning run before the city wakes up. #fitness #morning',
   'Central Park, NY', DATE_SUB(NOW(), INTERVAL 50 HOUR));

INSERT INTO post_media (post_id, url, media_type, position) VALUES
  (1, '/uploads/post1.svg', 'image', 0),
  (2, '/uploads/post2.svg', 'image', 0),
  (3, '/uploads/post3.svg', 'image', 0),
  (4, '/uploads/post4.svg', 'image', 0);

INSERT INTO hashtags (id, tag, usage_count) VALUES
  (1, 'travel', 1), (2, 'sunset', 1), (3, 'weekend', 1), (4, 'life', 1),
  (5, 'night', 1), (6, 'city', 1), (7, 'music', 1),
  (8, 'fitness', 1), (9, 'morning', 1), (10, 'reels', 2), (11, 'nature', 1);

INSERT INTO post_hashtags (post_id, hashtag_id) VALUES
  (1, 1), (1, 2), (2, 3), (2, 4), (3, 5), (3, 6), (3, 7), (4, 8), (4, 9);

INSERT INTO likes (user_id, post_id, created_at) VALUES
  (2, 1, DATE_SUB(NOW(), INTERVAL 2 HOUR)),
  (3, 1, DATE_SUB(NOW(), INTERVAL 1 HOUR)),
  (5, 1, DATE_SUB(NOW(), INTERVAL 30 MINUTE)),
  (1, 2, DATE_SUB(NOW(), INTERVAL 6 HOUR)),
  (3, 2, DATE_SUB(NOW(), INTERVAL 5 HOUR)),
  (1, 3, DATE_SUB(NOW(), INTERVAL 20 HOUR)),
  (2, 3, DATE_SUB(NOW(), INTERVAL 18 HOUR)),
  (5, 3, DATE_SUB(NOW(), INTERVAL 12 HOUR)),
  (1, 4, DATE_SUB(NOW(), INTERVAL 40 HOUR));

INSERT INTO comments (id, post_id, user_id, body, created_at) VALUES
  (1, 1, 2, 'This is unreal. What time did you get there?', DATE_SUB(NOW(), INTERVAL 2 HOUR)),
  (2, 1, 3, 'The colours are perfect 🔥', DATE_SUB(NOW(), INTERVAL 90 MINUTE)),
  (3, 2, 1, 'Need to try that route!', DATE_SUB(NOW(), INTERVAL 5 HOUR)),
  (4, 3, 5, 'Welcome to the neighbourhood.', DATE_SUB(NOW(), INTERVAL 10 HOUR)),
  (5, 4, 2, 'Strong work, early bird 🐦', DATE_SUB(NOW(), INTERVAL 30 HOUR));

INSERT INTO saved_posts (user_id, post_id, created_at) VALUES
  (1, 3, DATE_SUB(NOW(), INTERVAL 19 HOUR)),
  (2, 1, DATE_SUB(NOW(), INTERVAL 100 MINUTE)),
  (5, 2, DATE_SUB(NOW(), INTERVAL 4 HOUR));

-- ---------------------------------------------------------------------------
-- Stories (expire after 24h in the application)
-- ---------------------------------------------------------------------------
INSERT INTO stories (id, user_id, media_url, media_type, duration_ms, created_at, expires_at) VALUES
  (1, 1, '/uploads/story1.svg', 'image', 5000, DATE_SUB(NOW(), INTERVAL 1 HOUR), DATE_ADD(NOW(), INTERVAL 23 HOUR)),
  (2, 2, '/uploads/story2.svg', 'image', 6000, DATE_SUB(NOW(), INTERVAL 3 HOUR), DATE_ADD(NOW(), INTERVAL 21 HOUR)),
  (3, 3, '/uploads/story3.svg', 'image', 7000, DATE_SUB(NOW(), INTERVAL 5 HOUR), DATE_ADD(NOW(), INTERVAL 19 HOUR));

INSERT INTO story_views (story_id, viewer_id, viewed_at) VALUES
  (2, 1, DATE_SUB(NOW(), INTERVAL 2 HOUR)),
  (3, 1, DATE_SUB(NOW(), INTERVAL 4 HOUR)),
  (3, 2, DATE_SUB(NOW(), INTERVAL 4 HOUR));

-- ---------------------------------------------------------------------------
-- Reels
-- ---------------------------------------------------------------------------
INSERT INTO reels (id, user_id, video_url, thumbnail_url, caption, views_count, created_at) VALUES
  (1, 4, '/uploads/demo-reel.mp4', '/uploads/reel-thumb.svg',
   'A tiny moment of calm from the garden. #nature #reels', 1240, DATE_SUB(NOW(), INTERVAL 6 HOUR)),
  (2, 3, '/uploads/demo-reel.mp4', '/uploads/reel-thumb.svg',
   'Loop experiments for the new track. #music #reels', 860, DATE_SUB(NOW(), INTERVAL 28 HOUR));

INSERT INTO reel_views (reel_id, viewer_id) VALUES
  (1, 1), (1, 2), (1, 3), (1, 5), (2, 1), (2, 2), (2, 5);
UPDATE reels SET views_count = 1240 WHERE id = 1;
UPDATE reels SET views_count = 860 WHERE id = 2;

INSERT INTO reel_likes (reel_id, user_id) VALUES
  (1, 1), (1, 2), (1, 3), (2, 1), (2, 5);

INSERT INTO reel_comments (id, reel_id, user_id, body, created_at) VALUES
  (1, 1, 2, 'This loop is so satisfying.', DATE_SUB(NOW(), INTERVAL 4 HOUR)),
  (2, 1, 3, 'Nature wins again 🌿', DATE_SUB(NOW(), INTERVAL 3 HOUR)),
  (3, 2, 1, 'The beat drops perfectly.', DATE_SUB(NOW(), INTERVAL 20 HOUR));

INSERT INTO reel_saves (reel_id, user_id) VALUES (1, 1), (2, 5);

-- ---------------------------------------------------------------------------
-- Conversations and messages
-- ---------------------------------------------------------------------------
INSERT INTO conversations (id, participant1, participant2, created_at, updated_at) VALUES
  (1, 1, 2, DATE_SUB(NOW(), INTERVAL 3 DAY), DATE_SUB(NOW(), INTERVAL 20 MINUTE)),
  (2, 1, 3, DATE_SUB(NOW(), INTERVAL 2 DAY), DATE_SUB(NOW(), INTERVAL 5 HOUR)),
  (3, 2, 4, DATE_SUB(NOW(), INTERVAL 1 DAY), DATE_SUB(NOW(), INTERVAL 1 DAY));

INSERT INTO messages (id, conversation_id, sender_id, receiver_id, body, is_read, created_at) VALUES
  (1, 1, 1, 2, 'Hey! Loved your weekend post.', 1, DATE_SUB(NOW(), INTERVAL 3 DAY)),
  (2, 1, 2, 1, 'Thanks! That trail was worth the early start.', 1, DATE_SUB(NOW(), INTERVAL 3 DAY)),
  (3, 1, 1, 2, 'Are we still on for coffee this week?', 1, DATE_SUB(NOW(), INTERVAL 2 HOUR)),
  (4, 1, 2, 1, 'Absolutely. Thursday morning works for me ☕', 0, DATE_SUB(NOW(), INTERVAL 20 MINUTE)),
  (5, 2, 3, 1, 'Sent you the rough mix.', 1, DATE_SUB(NOW(), INTERVAL 6 HOUR)),
  (6, 2, 1, 3, 'Listening now!', 1, DATE_SUB(NOW(), INTERVAL 5 HOUR)),
  (7, 2, 3, 1, 'No rush — tell me what you think.', 0, DATE_SUB(NOW(), INTERVAL 5 HOUR)),
  (8, 3, 4, 2, 'Thanks for the recommendation!', 1, DATE_SUB(NOW(), INTERVAL 1 DAY));

-- ---------------------------------------------------------------------------
-- Notifications and search history
-- ---------------------------------------------------------------------------
INSERT INTO notifications (id, user_id, actor_id, type, entity_id, body, is_read, created_at) VALUES
  (1, 1, 2, 'like', 1, 'bob liked your post', 0, DATE_SUB(NOW(), INTERVAL 2 HOUR)),
  (2, 1, 3, 'comment', 1, 'charlie commented: The colours are perfect 🔥', 0, DATE_SUB(NOW(), INTERVAL 90 MINUTE)),
  (3, 1, 2, 'message', 1, 'bob sent you a message', 0, DATE_SUB(NOW(), INTERVAL 20 MINUTE)),
  (4, 2, 1, 'follow', 2, 'alice started following you', 1, DATE_SUB(NOW(), INTERVAL 40 DAY)),
  (5, 3, 1, 'like', 3, 'alice liked your post', 0, DATE_SUB(NOW(), INTERVAL 20 HOUR)),
  (6, 4, 1, 'follow_request', 1, 'alice requested to follow you', 0, DATE_SUB(NOW(), INTERVAL 2 HOUR));

INSERT INTO searches (user_id, query, created_at) VALUES
  (1, 'charlie', DATE_SUB(NOW(), INTERVAL 1 HOUR)),
  (1, 'travel', DATE_SUB(NOW(), INTERVAL 3 HOUR)),
  (2, 'sunset', DATE_SUB(NOW(), INTERVAL 6 HOUR));

ALTER TABLE users AUTO_INCREMENT = 6;
ALTER TABLE posts AUTO_INCREMENT = 5;
ALTER TABLE stories AUTO_INCREMENT = 4;
ALTER TABLE reels AUTO_INCREMENT = 3;
ALTER TABLE notifications AUTO_INCREMENT = 7;
ALTER TABLE messages AUTO_INCREMENT = 9;
ALTER TABLE comments AUTO_INCREMENT = 6;
ALTER TABLE reel_comments AUTO_INCREMENT = 4;
ALTER TABLE hashtags AUTO_INCREMENT = 12;
ALTER TABLE conversations AUTO_INCREMENT = 4;
