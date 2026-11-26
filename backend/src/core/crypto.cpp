#include "core/crypto.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <random>
#include <sstream>

#include "argon2.h"
#include "blake2/blake2.h"

namespace ig::crypto {

static const char* kBase64 =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64Encode(const std::string& input) {
  std::string out;
  out.reserve(((input.size() + 2) / 3) * 4);
  for (std::size_t i = 0; i < input.size(); i += 3) {
    const unsigned char a = static_cast<unsigned char>(input[i]);
    const unsigned char b = i + 1 < input.size()
                                ? static_cast<unsigned char>(input[i + 1])
                                : 0;
    const unsigned char c = i + 2 < input.size()
                                ? static_cast<unsigned char>(input[i + 2])
                                : 0;
    out.push_back(kBase64[a >> 2]);
    out.push_back(kBase64[((a & 0x03) << 4) | (b >> 4)]);
    out.push_back(i + 1 < input.size() ? kBase64[((b & 0x0f) << 2) | (c >> 6)]
                                       : '=');
    out.push_back(i + 2 < input.size() ? kBase64[c & 0x3f] : '=');
  }
  return out;
}

std::string base64UrlEncode(const std::string& input) {
  std::string out = base64Encode(input);
  std::replace(out.begin(), out.end(), '+', '-');
  std::replace(out.begin(), out.end(), '/', '_');
  while (!out.empty() && out.back() == '=') out.pop_back();
  return out;
}

bool base64Decode(const std::string& input, std::string* output) {
  if (!output) return false;
  output->clear();
  std::string normalized;
  for (char c : input) {
    if (std::isspace(static_cast<unsigned char>(c))) continue;
    if (c == '-') c = '+';
    if (c == '_') c = '/';
    normalized.push_back(c);
  }
  while (normalized.size() % 4 != 0) normalized.push_back('=');
  std::array<int, 256> table{};
  table.fill(-1);
  for (int i = 0; i < 64; ++i) {
    table[static_cast<unsigned char>(kBase64[i])] = i;
  }
  for (std::size_t i = 0; i < normalized.size(); i += 4) {
    int v[4];
    for (int j = 0; j < 4; ++j) {
      char c = normalized[i + j];
      if (c == '=') {
        v[j] = 0;
      } else {
        v[j] = table[static_cast<unsigned char>(c)];
        if (v[j] < 0) return false;
      }
    }
    unsigned char a = static_cast<unsigned char>((v[0] << 2) | (v[1] >> 4));
    unsigned char b = static_cast<unsigned char>(((v[1] & 0x0f) << 4) | (v[2] >> 2));
    unsigned char c = static_cast<unsigned char>(((v[2] & 0x03) << 6) | v[3]);
    output->push_back(static_cast<char>(a));
    if (normalized[i + 2] != '=') output->push_back(static_cast<char>(b));
    if (normalized[i + 3] != '=') output->push_back(static_cast<char>(c));
  }
  return true;
}

std::string randomToken(std::size_t byteCount) {
  std::random_device rd;
  std::mt19937_64 generator(
      (static_cast<std::uint64_t>(rd()) << 32) ^ static_cast<std::uint64_t>(rd()));
  std::string bytes;
  bytes.reserve(byteCount);
  for (std::size_t i = 0; i < byteCount; ++i) {
    bytes.push_back(static_cast<char>(generator() & 0xff));
  }
  return base64UrlEncode(bytes);
}

std::string fingerprint(const std::string& token) {
  unsigned char digest[32];
  blake2b(digest, 32, token.data(), token.size(), nullptr, 0);
  std::ostringstream hex;
  for (unsigned char byte : digest) {
    hex << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<int>(byte);
  }
  return hex.str();
}

std::string hashPassword(const std::string& password) {
  constexpr std::uint32_t kTimeCost = 3;
  constexpr std::uint32_t kMemoryCost = 32768;  // 32 MiB
  constexpr std::uint32_t kParallelism = 2;
  constexpr std::size_t kSaltLength = 16;
  constexpr std::size_t kHashLength = 32;
  constexpr std::size_t kEncodedLength = 128;

  std::string salt = randomToken(kSaltLength);
  std::string encoded(kEncodedLength, '\0');
  int result = argon2id_hash_encoded(
      kTimeCost, kMemoryCost, kParallelism, password.data(), password.size(),
      salt.data(), salt.size(), kHashLength, encoded.data(), encoded.size());
  if (result != ARGON2_OK) return {};
  encoded.resize(std::strlen(encoded.c_str()));
  return encoded;
}

bool verifyPassword(const std::string& encodedHash, const std::string& password) {
  if (encodedHash.empty()) return false;
  return argon2id_verify(encodedHash.c_str(), password.data(), password.size()) ==
         ARGON2_OK;
}

}  // namespace ig::crypto
