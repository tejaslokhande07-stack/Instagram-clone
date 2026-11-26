#include "database/database.h"

#include <algorithm>
#include <cstring>

namespace ig {

std::string Row::str(const std::string& key, const std::string& fallback) const {
  auto it = columns.find(key);
  return it == columns.end() ? fallback : it->second;
}

long long Row::integer(const std::string& key, long long fallback) const {
  auto it = columns.find(key);
  if (it == columns.end() || it->second.empty()) return fallback;
  try {
    return std::stoll(it->second);
  } catch (...) {
    return fallback;
  }
}

int Row::intValue(const std::string& key, int fallback) const {
  return static_cast<int>(integer(key, fallback));
}

bool Row::boolean(const std::string& key, bool fallback) const {
  auto it = columns.find(key);
  if (it == columns.end()) return fallback;
  return it->second == "1" || it->second == "true";
}

Param Param::null() { return Param{}; }
Param Param::of(long long value) {
  Param p;
  p.type = Param::Type::Integer;
  p.integer = value;
  return p;
}
Param Param::of(int value) { return of(static_cast<long long>(value)); }
Param Param::of(double value) {
  Param p;
  p.type = Param::Type::Double;
  p.real = value;
  return p;
}
Param Param::of(const std::string& value) {
  Param p;
  p.type = Param::Type::Text;
  p.text = value;
  return p;
}
Param Param::of(const char* value) { return of(std::string(value)); }

Statement::Statement(MYSQL* connection, const std::string& sql)
    : connection_(connection) {
  stmt_ = mysql_stmt_init(connection_);
  if (stmt_) {
    mysql_stmt_prepare(stmt_, sql.c_str(), static_cast<unsigned long>(sql.size()));
  }
}

Statement::~Statement() {
  if (stmt_) mysql_stmt_close(stmt_);
}

bool Statement::prepare(std::string* error) {
  if (!stmt_) {
    if (error) *error = mysql_error(connection_);
    return false;
  }
  const char* err = mysql_stmt_error(stmt_);
  if (err && *err) {
    if (error) *error = err;
    return false;
  }
  return true;
}

void Statement::bind(const std::vector<Param>& params) {
  bindings_.assign(params.size(), MYSQL_BIND{});
  textStorage_.clear();
  intStorage_.clear();
  doubleStorage_.clear();
  textStorage_.reserve(params.size());
  intStorage_.reserve(params.size());
  doubleStorage_.reserve(params.size());

  for (std::size_t i = 0; i < params.size(); ++i) {
    MYSQL_BIND& b = bindings_[i];
    const Param& p = params[i];
    switch (p.type) {
      case Param::Type::Null:
        b.buffer_type = MYSQL_TYPE_NULL;
        break;
      case Param::Type::Integer:
        intStorage_.push_back(p.integer);
        b.buffer_type = MYSQL_TYPE_LONGLONG;
        b.buffer = &intStorage_.back();
        b.is_unsigned = false;
        break;
      case Param::Type::Double:
        doubleStorage_.push_back(p.real);
        b.buffer_type = MYSQL_TYPE_DOUBLE;
        b.buffer = &doubleStorage_.back();
        break;
      case Param::Type::Text: {
        textStorage_.push_back(p.text);
        std::string& s = textStorage_.back();
        b.buffer_type = MYSQL_TYPE_STRING;
        b.buffer = const_cast<char*>(s.data());
        b.buffer_length = static_cast<unsigned long>(s.size());
        b.length = nullptr;
        break;
      }
    }
  }
  if (!bindings_.empty()) {
    mysql_stmt_bind_param(stmt_, bindings_.data());
  }
}

bool Statement::execute(std::string* error, std::uint64_t* affected,
                        std::uint64_t* insertedId) {
  if (mysql_stmt_execute(stmt_) != 0) {
    if (error) *error = mysql_stmt_error(stmt_);
    return false;
  }
  if (affected) *affected = mysql_stmt_affected_rows(stmt_);
  if (insertedId) *insertedId = mysql_stmt_insert_id(stmt_);
  return true;
}

std::vector<Row> Statement::fetchAll(std::string* error) {
  std::vector<Row> result;
  MYSQL_RES* metadata = mysql_stmt_result_metadata(stmt_);
  if (!metadata) return result;  // statements without result sets

  const unsigned int columnCount = mysql_num_fields(metadata);
  MYSQL_FIELD* fields = mysql_fetch_fields(metadata);
  std::vector<MYSQL_BIND> out(columnCount, MYSQL_BIND{});
  std::vector<std::string> buffers(columnCount);
  std::vector<unsigned long> lengths(columnCount, 0);
  std::unique_ptr<bool[]> isNull(new bool[columnCount]());
  constexpr std::size_t kColumnCapacity = 65536;

  for (unsigned int i = 0; i < columnCount; ++i) {
    buffers[i].resize(kColumnCapacity);
    out[i].buffer_type = MYSQL_TYPE_STRING;
    out[i].buffer = buffers[i].data();
    out[i].buffer_length = kColumnCapacity;
    out[i].length = &lengths[i];
    out[i].is_null = isNull.get() + i;
  }

  if (mysql_stmt_bind_result(stmt_, out.data()) != 0) {
    if (error) *error = mysql_stmt_error(stmt_);
    mysql_free_result(metadata);
    return result;
  }
  mysql_stmt_store_result(stmt_);

  int status = 0;
  while ((status = mysql_stmt_fetch(stmt_)) == 0 ||
         status == MYSQL_DATA_TRUNCATED) {
    Row row;
    for (unsigned int i = 0; i < columnCount; ++i) {
      if (isNull[i]) {
        row.columns[fields[i].name] = "";
      } else {
        const std::size_t n = std::min<std::size_t>(lengths[i], kColumnCapacity - 1);
        row.columns[fields[i].name] = std::string(buffers[i].data(), n);
      }
      lengths[i] = 0;
      isNull[i] = false;
    }
    result.push_back(std::move(row));
    if (status == MYSQL_DATA_TRUNCATED) continue;
  }
  mysql_free_result(metadata);
  return result;
}

Database::~Database() {
  for (MYSQL* connection : connections_) {
    if (connection) mysql_close(connection);
  }
  connections_.clear();
  mysql_library_end();
}

bool Database::initialize(const AppConfig& config, std::string* error) {
  if (mysql_library_init(0, nullptr, nullptr) != 0) {
    initError_ = "mysql_library_init failed";
    if (error) *error = initError_;
    return false;
  }
  for (int i = 0; i < 8; ++i) {
    MYSQL* connection = mysql_init(nullptr);
    if (!connection) {
      initError_ = "mysql_init failed";
      if (error) *error = initError_;
      return false;
    }
    unsigned int timeout = 5;
    mysql_options(connection, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    if (!mysql_real_connect(connection, config.mysqlHost.c_str(),
                            config.mysqlUser.c_str(), config.mysqlPassword.c_str(),
                            config.mysqlDatabase.c_str(), config.mysqlPort,
                            nullptr, 0)) {
      initError_ = mysql_error(connection);
      if (error) *error = "MySQL connect failed: " + initError_;
      mysql_close(connection);
      return false;
    }
    mysql_set_character_set(connection, "utf8mb4");
    connections_.push_back(connection);
  }
  return true;
}

Database::ConnectionGuard::ConnectionGuard(ConnectionGuard&& other) noexcept
    : owner(other.owner), connection(other.connection) {
  other.owner = nullptr;
  other.connection = nullptr;
}

Database::ConnectionGuard& Database::ConnectionGuard::operator=(
    ConnectionGuard&& other) noexcept {
  if (this != &other) {
    if (owner && connection) {
      std::lock_guard<std::mutex> lock(owner->mutex_);
      owner->connections_.push_back(connection);
      owner->available_.notify_one();
    }
    owner = other.owner;
    connection = other.connection;
    other.owner = nullptr;
    other.connection = nullptr;
  }
  return *this;
}

Database::ConnectionGuard::~ConnectionGuard() {
  if (owner && connection) {
    {
      std::lock_guard<std::mutex> lock(owner->mutex_);
      owner->connections_.push_back(connection);
    }
    owner->available_.notify_one();
    connection = nullptr;
  }
}

Database::ConnectionGuard Database::acquire() {
  std::unique_lock<std::mutex> lock(mutex_);
  available_.wait(lock, [this] { return !connections_.empty(); });
  ConnectionGuard guard;
  guard.owner = this;
  guard.connection = connections_.back();
  connections_.pop_back();
  return guard;
}

bool Database::execute(const std::string& sql, const std::vector<Param>& params,
                       std::string* error, std::uint64_t* insertedId) {
  ConnectionGuard connection = acquire();
  Statement stmt(connection.connection, sql);
  if (!stmt.prepare(error)) return false;
  stmt.bind(params);
  return stmt.execute(error, nullptr, insertedId);
}

bool Database::executeCount(const std::string& sql,
                            const std::vector<Param>& params,
                            std::string* error, std::uint64_t* affected,
                            std::uint64_t* insertedId) {
  ConnectionGuard connection = acquire();
  Statement stmt(connection.connection, sql);
  if (!stmt.prepare(error)) return false;
  stmt.bind(params);
  return stmt.execute(error, affected, insertedId);
}

bool Database::query(const std::string& sql, const std::vector<Param>& params,
                     std::vector<Row>* rows, std::string* error) {
  ConnectionGuard connection = acquire();
  Statement stmt(connection.connection, sql);
  if (!stmt.prepare(error)) return false;
  stmt.bind(params);
  if (!stmt.execute(error)) return false;
  if (rows) *rows = stmt.fetchAll(error);
  return true;
}

std::string Database::lastError() const { return initError_; }

}  // namespace ig
