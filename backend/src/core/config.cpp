#include "core/config.h"

#include <cstdlib>
#include <string>

namespace ig {

static std::string env(const char* name, const std::string& fallback) {
  const char* value = std::getenv(name);
  return (value && *value) ? std::string(value) : fallback;
}

AppConfig AppConfig::fromEnvironment() {
  AppConfig c;
  c.host = env("IG_HOST", c.host);
  c.port = static_cast<std::uint16_t>(std::stoul(env("IG_PORT", "8080")));
  c.mysqlHost = env("IG_DB_HOST", c.mysqlHost);
  c.mysqlPort = static_cast<std::uint16_t>(std::stoul(env("IG_DB_PORT", "3307")));
  c.mysqlUser = env("IG_DB_USER", c.mysqlUser);
  c.mysqlPassword = env("IG_DB_PASSWORD", c.mysqlPassword);
  c.mysqlDatabase = env("IG_DB_NAME", c.mysqlDatabase);
  c.uploadDir = env("IG_UPLOAD_DIR", c.uploadDir);
  c.publicBaseUrl = env("IG_PUBLIC_URL", c.publicBaseUrl);
  return c;
}

}  // namespace ig
