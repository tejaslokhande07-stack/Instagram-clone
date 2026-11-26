#include "dsa/hash_set.hpp"

#include <cstdint>

namespace ig::dsa {

HashSet::HashSet(std::size_t initialBuckets, float maxLoad)
    : buckets_(initialBuckets == 0 ? 1 : initialBuckets),
      maxLoad_(maxLoad > 0.05f ? maxLoad : 0.75f) {}

std::size_t HashSet::indexFor(long long value, std::size_t count) const {
  // 64-bit mixing so sequential integer IDs do not cluster in one bucket.
  std::uint64_t x = static_cast<std::uint64_t>(value);
  x ^= x >> 30;
  x *= 0xbf58476d1ce4e5b9ULL;
  x ^= x >> 27;
  x *= 0x94d049bb133111ebULL;
  x ^= x >> 31;
  return static_cast<std::size_t>(x) % count;
}

bool HashSet::insert(long long value) {
  if (contains(value)) return false;
  if (static_cast<float>(size_ + 1) / buckets_.size() >= maxLoad_) grow();
  buckets_[indexFor(value, buckets_.size())].push_back(value);
  ++size_;
  return true;
}

bool HashSet::contains(long long value) const {
  const auto& bucket = buckets_[indexFor(value, buckets_.size())];
  for (long long v : bucket) {
    if (v == value) return true;
  }
  return false;
}

bool HashSet::remove(long long value) {
  auto& bucket = buckets_[indexFor(value, buckets_.size())];
  for (auto it = bucket.begin(); it != bucket.end(); ++it) {
    if (*it == value) {
      bucket.erase(it);
      --size_;
      return true;
    }
  }
  return false;
}

void HashSet::clear() {
  for (auto& bucket : buckets_) bucket.clear();
  size_ = 0;
}

std::vector<long long> HashSet::entries() const {
  std::vector<long long> result;
  result.reserve(size_);
  forEach([&](long long v) { result.push_back(v); });
  return result;
}

void HashSet::grow() {
  std::vector<std::vector<long long>> old = std::move(buckets_);
  buckets_.assign(old.size() * 2, {});
  size_ = 0;
  for (const auto& bucket : old) {
    for (long long v : bucket) insert(v);
  }
}

}  // namespace ig::dsa
