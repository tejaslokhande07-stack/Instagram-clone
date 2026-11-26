#pragma once
#include <cstdint>
#include <string>

namespace ig {

struct AppConfig {
  std::string host = "127.0.0.1";
  std::uint16_t port = 8080;
  std::string mysqlHost = "127.0.0.1";
  std::uint16_t mysqlPort = 3307;
  std::string mysqlUser = "ig_app";
  std::string mysqlPassword = "ig_app_password";
  std::string mysqlDatabase = "instagram_clone";
  std::string uploadDir = "uploads";
  std::string publicBaseUrl = "http://localhost:8080";

  static AppConfig fromEnvironment();
};

}  // namespace ig
