#include "services/app_service.h"

#include <algorithm>
#include <cctype>
#include <regex>

#include "core/crypto.h"

namespace ig {

static std::string lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

static std::string trimmed(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return "";
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

static bool validUsername(const std::string& value) {
  static const std::regex pattern("^[A-Za-z0-9_.]{3,30}$");
  return std::regex_match(value, pattern);
}

static bool validEmail(const std::string& value) {
  static const std::regex pattern("^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}$");
  return value.size() <= 255 && std::regex_match(value, pattern);
}

ApiResult AppService::registerUser(const json& payload) {
  const std::string username = trimmed(payload.value("username", ""));
  const std::string email = lower(trimmed(payload.value("email", "")));
  const std::string password = payload.value("password", "");
  const std::string fullName = trimmed(payload.value("fullName", username));

  if (!validUsername(username)) {
    return ApiResult::fail(400, "Username must be 3-30 letters, digits, '.' or '_'");
  }
  if (!validEmail(email)) return ApiResult::fail(400, "Invalid email address");
  if (password.size() < 8) {
    return ApiResult::fail(400, "Password must be at least 8 characters");
  }

  std::vector<Row> existing;
  ctx_.db.query("SELECT id FROM users WHERE username = ? OR email = ?",
                {Param::of(username), Param::of(email)}, &existing, nullptr);
  if (!existing.empty()) {
    return ApiResult::fail(409, "Username or email is already registered");
  }

  const std::string hash = crypto::hashPassword(password);
  if (hash.empty()) return ApiResult::fail(500, "Password hashing failed");

  std::uint64_t inserted = 0;
  std::string error;
  if (!ctx_.db.execute(
          "INSERT INTO users (username, email, password_hash, full_name) "
          "VALUES (?, ?, ?, ?)",
          {Param::of(username), Param::of(email), Param::of(hash),
           Param::of(fullName)},
          &error, &inserted)) {
    return ApiResult::fail(409, "Username or email is already registered");
  }
  const long long userId = static_cast<long long>(inserted);
  ctx_.db.execute("INSERT INTO user_profiles (user_id) VALUES (?)",
                  {Param::of(userId)}, nullptr);

  {
    std::lock_guard<std::mutex> lock(ctx_.dsaMutex);
    ctx_.usernameIndex.set(username, userId);
    ctx_.idIndex.set(userId, username);
    ctx_.usernameTrie.insert(username);
  }

  const std::string token = crypto::randomToken(32);
  ctx_.db.execute("INSERT INTO sessions (user_id, token_hash, expires_at) "
                  "VALUES (?, ?, DATE_ADD(NOW(), INTERVAL 30 DAY))",
                  {Param::of(userId), Param::of(crypto::fingerprint(token))},
                  nullptr);

  User user;
  userById(userId, &user);
  json response;
  response["token"] = token;
  response["user"] = publicUser(user);
  return ApiResult::created(response);
}

ApiResult AppService::login(const json& payload) {
  const std::string identity = trimmed(payload.value("username", ""));
  const std::string password = payload.value("password", "");
  if (identity.empty() || password.empty()) {
    return ApiResult::fail(400, "Username and password are required");
  }

  std::vector<Row> rows;
  if (!ctx_.db.query("SELECT id, username, password_hash FROM users "
                     "WHERE username = ? OR email = ? LIMIT 1",
                     {Param::of(identity), Param::of(lower(identity))}, &rows,
                     nullptr)) {
    return ApiResult::fail(500, "Database error");
  }
  if (rows.empty()) return ApiResult::fail(401, "Invalid credentials");

  const long long userId = rows[0].integer("id");
  if (!crypto::verifyPassword(rows[0].str("password_hash"), password)) {
    return ApiResult::fail(401, "Invalid credentials");
  }

  ctx_.db.execute("UPDATE users SET last_seen = NOW() WHERE id = ?",
                  {Param::of(userId)}, nullptr);
  const std::string token = crypto::randomToken(32);
  ctx_.db.execute("INSERT INTO sessions (user_id, token_hash, expires_at) "
                  "VALUES (?, ?, DATE_ADD(NOW(), INTERVAL 30 DAY))",
                  {Param::of(userId), Param::of(crypto::fingerprint(token))},
                  nullptr);

  User user;
  userById(userId, &user);
  json response;
  response["token"] = token;
  response["user"] = publicUser(user);
  return ApiResult::ok(response);
}

ApiResult AppService::logout(const std::string& token) {
  if (token.empty()) return ApiResult::fail(400, "Token required");
  ctx_.db.execute("DELETE FROM sessions WHERE token_hash = ?",
                  {Param::of(crypto::fingerprint(token))}, nullptr);
  return ApiResult::ok({{"message", "Logged out"}});
}

std::optional<long long> AppService::authenticate(const std::string& header) const {
  std::string token = trimmed(header);
  const std::string prefix = "Bearer ";
  if (token.size() >= prefix.size() &&
      token.compare(0, prefix.size(), prefix) == 0) {
    token = trimmed(token.substr(prefix.size()));
  }
  if (token.empty()) return std::nullopt;
  std::vector<Row> rows;
  if (!ctx_.db.query("SELECT user_id FROM sessions WHERE token_hash = ? "
                     "AND expires_at > NOW() LIMIT 1",
                     {Param::of(crypto::fingerprint(token))}, &rows, nullptr)) {
    return std::nullopt;
  }
  if (rows.empty()) return std::nullopt;
  return rows[0].integer("user_id");
}

ApiResult AppService::requestPasswordReset(const json& payload) {
  const std::string email = lower(trimmed(payload.value("email", "")));
  std::vector<Row> rows;
  ctx_.db.query("SELECT id FROM users WHERE email = ?", {Param::of(email)},
                &rows, nullptr);
  if (rows.empty()) {
    // Do not disclose whether the account exists.
    return ApiResult::ok({{"message", "If the account exists, a reset token was created"}});
  }
  const long long userId = rows[0].integer("id");
  const std::string token = crypto::randomToken(24);
  ctx_.db.execute("INSERT INTO password_resets (user_id, token_hash, expires_at) "
                  "VALUES (?, ?, DATE_ADD(NOW(), INTERVAL 1 HOUR))",
                  {Param::of(userId), Param::of(crypto::fingerprint(token))},
                  nullptr);
  json response;
  response["message"] = "If the account exists, a reset token was created";
  // Local development delivers the token in the response (no email service).
  response["devToken"] = token;
  return ApiResult::ok(response);
}

ApiResult AppService::resetPassword(const json& payload) {
  const std::string token = trimmed(payload.value("token", ""));
  const std::string password = payload.value("password", "");
  if (password.size() < 8) {
    return ApiResult::fail(400, "Password must be at least 8 characters");
  }
  std::vector<Row> rows;
  ctx_.db.query("SELECT id, user_id FROM password_resets WHERE token_hash = ? "
                "AND used = 0 AND expires_at > NOW() LIMIT 1",
                {Param::of(crypto::fingerprint(token))}, &rows, nullptr);
  if (rows.empty()) return ApiResult::fail(400, "Invalid or expired reset token");

  const std::string hash = crypto::hashPassword(password);
  if (hash.empty()) return ApiResult::fail(500, "Password hashing failed");
  ctx_.db.execute("UPDATE users SET password_hash = ? WHERE id = ?",
                  {Param::of(hash), Param::of(rows[0].integer("user_id"))},
                  nullptr);
  ctx_.db.execute("UPDATE password_resets SET used = 1 WHERE id = ?",
                  {Param::of(rows[0].integer("id"))}, nullptr);
  ctx_.db.execute("DELETE FROM sessions WHERE user_id = ?",
                  {Param::of(rows[0].integer("user_id"))}, nullptr);
  return ApiResult::ok({{"message", "Password reset successful"}});
}

}  // namespace ig
