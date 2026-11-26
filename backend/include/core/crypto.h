#pragma once
// Security helpers: Argon2id password hashing, BLAKE2b token fingerprints,
// CSPRNG-backed token generation and base64/base64url encoding.
#include <cstddef>
#include <string>

namespace ig::crypto {

std::string randomToken(std::size_t byteCount = 32);
std::string base64Encode(const std::string& input);
std::string base64UrlEncode(const std::string& input);
bool base64Decode(const std::string& input, std::string* output);
std::string fingerprint(const std::string& token);  // BLAKE2b-256 hex

// Argon2id PHC string: $argon2id$v=19$m=...,t=...,p=...$salt$hash
std::string hashPassword(const std::string& password);
bool verifyPassword(const std::string& encodedHash, const std::string& password);

}  // namespace ig::crypto
