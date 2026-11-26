#pragma once
// Custom hash table with separate-chaining collision resolution.
// Used as an application-level index for users (username -> record) that is
// rebuilt from MySQL at startup. Average O(1) insert/search/delete.
#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace ig::dsa {

template <typename K, typename V, typename Hash = std::hash<K>,
          typename KeyEqual = std::equal_to<K>>
class HashTable {
 public:
  struct Entry {
    K key;
    V value;
  };

  explicit HashTable(std::size_t initialBuckets = 16, float maxLoad = 0.75f);
  ~HashTable();
  HashTable(const HashTable& other);
  HashTable(HashTable&& other) noexcept;
  HashTable& operator=(const HashTable& other);
  HashTable& operator=(HashTable&& other) noexcept;

  // Returns false when the key already exists (duplicate prevention).
  bool insert(const K& key, const V& value);
  // Insert or overwrite. Returns true when a new key was created.
  bool set(const K& key, const V& value);
  bool search(const K& key, V* out = nullptr) const;
  bool contains(const K& key) const { return search(key, nullptr); }
  // Returns false when the key was not present.
  bool remove(const K& key, V* removed = nullptr);
  void clear() noexcept;

  std::size_t size() const noexcept { return size_; }
  std::size_t bucketCount() const noexcept { return buckets_.size(); }
  float loadFactor() const noexcept {
    return buckets_.empty() ? 0.0f
                            : static_cast<float>(size_) / buckets_.size();
  }
  void rehash(std::size_t newBucketCount);

  std::vector<Entry> entries() const;

  // Traversal: visits every key/value pair (bucket order, not sorted).
  template <typename F>
  void forEach(F&& fn) const {
    for (const auto* bucket : buckets_) {
      for (const Node* n = bucket; n != nullptr; n = n->next) {
        fn(n->key, n->value);
      }
    }
  }

 private:
  struct Node {
    K key;
    V value;
    Node* next;
    Node(const K& k, const V& v, Node* n) : key(k), value(v), next(n) {}
  };

  std::vector<Node*> buckets_;
  std::size_t size_ = 0;
  float maxLoad_;
  Hash hasher_;
  KeyEqual equal_;

  std::size_t indexFor(const K& key, std::size_t bucketCount) const;
  void destroyBuckets() noexcept;
};

extern template class HashTable<std::string, int>;
extern template class HashTable<std::string, bool>;
extern template class HashTable<int, std::string>;
extern template class HashTable<std::string, long long>;
extern template class HashTable<long long, std::string>;
extern template class HashTable<long long, std::vector<long long>>;

}  // namespace ig::dsa
