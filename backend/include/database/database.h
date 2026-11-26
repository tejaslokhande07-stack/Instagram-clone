#pragma once
// Thread-safe MySQL connection pool. Every query uses a prepared statement;
// the database remains the permanent source of truth for all application data.
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <mysql.h>

#include "core/config.h"

namespace ig {

struct Row {
  std::unordered_map<std::string, std::string> columns;

  std::string str(const std::string& key,
                  const std::string& fallback = "") const;
  long long integer(const std::string& key, long long fallback = 0) const;
  int intValue(const std::string& key, int fallback = 0) const;
  bool boolean(const std::string& key, bool fallback = false) const;
};

struct Param {
  enum class Type { Null, Integer, Double, Text } type = Type::Null;
  long long integer = 0;
  double real = 0.0;
  std::string text;

  static Param null();
  static Param of(long long value);
  static Param of(int value);
  static Param of(double value);
  static Param of(const std::string& value);
  static Param of(const char* value);
};

class Statement {
 public:
  Statement(MYSQL* connection, const std::string& sql);
  ~Statement();
  Statement(const Statement&) = delete;
  Statement& operator=(const Statement&) = delete;

  bool prepare(std::string* error);
  void bind(const std::vector<Param>& params);
  bool execute(std::string* error, std::uint64_t* affected = nullptr,
               std::uint64_t* insertedId = nullptr);
  std::vector<Row> fetchAll(std::string* error);

 private:
  MYSQL* connection_;
  MYSQL_STMT* stmt_ = nullptr;
  std::vector<MYSQL_BIND> bindings_;
  std::vector<std::string> textStorage_;
  std::vector<long long> intStorage_;
  std::vector<double> doubleStorage_;
};

class Database {
 public:
  Database() = default;
  ~Database();
  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;

  bool initialize(const AppConfig& config, std::string* error);
  bool execute(const std::string& sql, const std::vector<Param>& params,
               std::string* error, std::uint64_t* insertedId = nullptr);
  // Same as execute(), but also reports affected rows. Needed for INSERT IGNORE
  // idempotency checks (like/save) where insert_id is always zero.
  bool executeCount(const std::string& sql, const std::vector<Param>& params,
                    std::string* error, std::uint64_t* affected,
                    std::uint64_t* insertedId = nullptr);
  bool query(const std::string& sql, const std::vector<Param>& params,
             std::vector<Row>* rows, std::string* error);
  std::string lastError() const;

 private:
  struct ConnectionGuard {
    Database* owner = nullptr;
    MYSQL* connection = nullptr;
    ConnectionGuard() = default;
    ConnectionGuard(const ConnectionGuard&) = delete;
    ConnectionGuard& operator=(const ConnectionGuard&) = delete;
    ConnectionGuard(ConnectionGuard&& other) noexcept;
    ConnectionGuard& operator=(ConnectionGuard&& other) noexcept;
    ~ConnectionGuard();
  };

  ConnectionGuard acquire();

  std::vector<MYSQL*> connections_;
  std::mutex mutex_;
  std::condition_variable available_;
  std::string initError_;
};

}  // namespace ig
