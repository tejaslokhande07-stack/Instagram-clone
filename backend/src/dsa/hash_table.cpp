#include "dsa/hash_table.hpp"

#include <algorithm>

namespace ig::dsa {

template <typename K, typename V, typename Hash, typename KeyEqual>
HashTable<K, V, Hash, KeyEqual>::HashTable(std::size_t initialBuckets,
                                           float maxLoad)
    : buckets_(initialBuckets == 0 ? 1 : initialBuckets, nullptr),
      maxLoad_(maxLoad > 0.05f ? maxLoad : 0.75f) {}

template <typename K, typename V, typename Hash, typename KeyEqual>
HashTable<K, V, Hash, KeyEqual>::~HashTable() {
  destroyBuckets();
}

template <typename K, typename V, typename Hash, typename KeyEqual>
HashTable<K, V, Hash, KeyEqual>::HashTable(const HashTable& other)
    : buckets_(other.buckets_.size(), nullptr),
      size_(other.size_),
      maxLoad_(other.maxLoad_),
      hasher_(other.hasher_),
      equal_(other.equal_) {
  other.forEach([this](const K& k, const V& v) { set(k, v); });
  size_ = other.size_;
}

template <typename K, typename V, typename Hash, typename KeyEqual>
HashTable<K, V, Hash, KeyEqual>::HashTable(HashTable&& other) noexcept
    : buckets_(std::move(other.buckets_)),
      size_(other.size_),
      maxLoad_(other.maxLoad_),
      hasher_(std::move(other.hasher_)),
      equal_(std::move(other.equal_)) {
  other.size_ = 0;
  other.buckets_.assign(1, nullptr);
}

template <typename K, typename V, typename Hash, typename KeyEqual>
HashTable<K, V, Hash, KeyEqual>& HashTable<K, V, Hash, KeyEqual>::operator=(
    const HashTable& other) {
  if (this == &other) return *this;
  HashTable copy(other);
  *this = std::move(copy);
  return *this;
}

template <typename K, typename V, typename Hash, typename KeyEqual>
HashTable<K, V, Hash, KeyEqual>& HashTable<K, V, Hash, KeyEqual>::operator=(
    HashTable&& other) noexcept {
  if (this == &other) return *this;
  destroyBuckets();
  buckets_ = std::move(other.buckets_);
  size_ = other.size_;
  maxLoad_ = other.maxLoad_;
  hasher_ = std::move(other.hasher_);
  equal_ = std::move(other.equal_);
  other.buckets_.assign(1, nullptr);
  other.size_ = 0;
  return *this;
}

template <typename K, typename V, typename Hash, typename KeyEqual>
std::size_t HashTable<K, V, Hash, KeyEqual>::indexFor(
    const K& key, std::size_t bucketCount) const {
  // std::hash + modulo; chaining resolves collisions deterministically.
  return hasher_(key) % bucketCount;
}

template <typename K, typename V, typename Hash, typename KeyEqual>
void HashTable<K, V, Hash, KeyEqual>::destroyBuckets() noexcept {
  for (auto* bucket : buckets_) {
    Node* cur = bucket;
    while (cur != nullptr) {
      Node* next = cur->next;
      delete cur;
      cur = next;
    }
    bucket = nullptr;
  }
}

template <typename K, typename V, typename Hash, typename KeyEqual>
bool HashTable<K, V, Hash, KeyEqual>::insert(const K& key, const V& value) {
  if (search(key, nullptr)) return false;  // duplicate keys are rejected
  if (loadFactor() >= maxLoad_) rehash(buckets_.size() * 2);
  const std::size_t idx = indexFor(key, buckets_.size());
  buckets_[idx] = new Node(key, value, buckets_[idx]);
  ++size_;
  return true;
}

template <typename K, typename V, typename Hash, typename KeyEqual>
bool HashTable<K, V, Hash, KeyEqual>::set(const K& key, const V& value) {
  const std::size_t idx = indexFor(key, buckets_.size());
  for (Node* n = buckets_[idx]; n != nullptr; n = n->next) {
    if (equal_(n->key, key)) {
      n->value = value;
      return false;
    }
  }
  return insert(key, value);
}

template <typename K, typename V, typename Hash, typename KeyEqual>
bool HashTable<K, V, Hash, KeyEqual>::search(const K& key, V* out) const {
  const std::size_t idx = indexFor(key, buckets_.size());
  for (const Node* n = buckets_[idx]; n != nullptr; n = n->next) {
    if (equal_(n->key, key)) {
      if (out) *out = n->value;
      return true;
    }
  }
  return false;
}

template <typename K, typename V, typename Hash, typename KeyEqual>
bool HashTable<K, V, Hash, KeyEqual>::remove(const K& key, V* removed) {
  const std::size_t idx = indexFor(key, buckets_.size());
  Node* cur = buckets_[idx];
  Node* prev = nullptr;
  while (cur != nullptr) {
    if (equal_(cur->key, key)) {
      if (removed) *removed = cur->value;
      if (prev) prev->next = cur->next;
      else buckets_[idx] = cur->next;
      delete cur;
      --size_;
      return true;
    }
    prev = cur;
    cur = cur->next;
  }
  return false;
}

template <typename K, typename V, typename Hash, typename KeyEqual>
void HashTable<K, V, Hash, KeyEqual>::clear() noexcept {
  destroyBuckets();
  std::fill(buckets_.begin(), buckets_.end(), nullptr);
  size_ = 0;
}

template <typename K, typename V, typename Hash, typename KeyEqual>
void HashTable<K, V, Hash, KeyEqual>::rehash(std::size_t newBucketCount) {
  if (newBucketCount < 1) newBucketCount = 1;
  std::vector<Node*> old = std::move(buckets_);
  buckets_.assign(newBucketCount, nullptr);
  size_ = 0;
  for (Node* bucket : old) {
    Node* cur = bucket;
    while (cur != nullptr) {
      Node* next = cur->next;
      insert(cur->key, cur->value);
      delete cur;
      cur = next;
    }
  }
}

template <typename K, typename V, typename Hash, typename KeyEqual>
std::vector<typename HashTable<K, V, Hash, KeyEqual>::Entry>
HashTable<K, V, Hash, KeyEqual>::entries() const {
  std::vector<Entry> result;
  result.reserve(size_);
  forEach([&](const K& k, const V& v) { result.push_back({k, v}); });
  return result;
}

// Explicit instantiations keep template definitions available to other TUs.
template class HashTable<std::string, int>;
template class HashTable<std::string, bool>;
template class HashTable<int, std::string>;
template class HashTable<std::string, long long>;
template class HashTable<long long, std::string>;
template class HashTable<long long, std::vector<long long>>;

}  // namespace ig::dsa
