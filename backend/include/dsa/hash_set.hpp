#pragma once
// Custom hash set (separate chaining) used for unique relationship membership:
// post likes, saved posts, unread conversations, visited BFS vertices.
// Insert / search / delete: O(1) average, O(n) worst case.
#include <cstddef>
#include <functional>
#include <vector>

namespace ig::dsa {

class HashSet {
 public:
  explicit HashSet(std::size_t initialBuckets = 16, float maxLoad = 0.75f);

  bool insert(long long value);   // false if already present
  bool contains(long long value) const;
  bool remove(long long value);   // false if not present
  void clear();
  std::size_t size() const noexcept { return size_; }
  std::vector<long long> entries() const;

  template <typename F>
  void forEach(F&& fn) const {
    for (const auto& bucket : buckets_) {
      for (long long v : bucket) fn(v);
    }
  }

 private:
  std::vector<std::vector<long long>> buckets_;
  std::size_t size_ = 0;
  float maxLoad_;

  std::size_t indexFor(long long value, std::size_t count) const;
  void grow();
};

}  // namespace ig::dsa
