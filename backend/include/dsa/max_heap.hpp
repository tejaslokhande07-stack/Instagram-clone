#pragma once
// Custom binary max-heap. Used for feed ranking and trending reels/posts.
// insert O(log n), pop O(log n), peek O(1), arbitrary remove O(log n),
// heapify O(n). Items with equal score are ordered by larger id first.
#include <cstddef>
#include <vector>

namespace ig::dsa {

struct HeapItem {
  long long id = 0;
  double score = 0.0;
};

class MaxHeap {
 public:
  MaxHeap() = default;
  explicit MaxHeap(std::vector<HeapItem> items);

  void insert(long long id, double score);
  HeapItem peek() const;             // undefined behaviour if empty; check empty()
  HeapItem pop();                    // removes and returns max
  bool removeId(long long id);       // delete by value, O(n) search + O(log n) fix
  bool removeAt(std::size_t index);  // delete by heap index, O(log n)
  bool empty() const noexcept { return data_.empty(); }
  std::size_t size() const noexcept { return data_.size(); }
  void heapify();                    // restore heap invariant in O(n)
  void clear() noexcept { data_.clear(); }

  // Traversal in heap array order (not sorted; use repeated pop() for sorted).
  std::vector<HeapItem> items() const { return data_; }
  template <typename F>
  void forEach(F&& fn) const {
    for (const auto& item : data_) fn(item);
  }

 private:
  std::vector<HeapItem> data_;

  static bool higher(const HeapItem& a, const HeapItem& b);
  void siftUp(std::size_t index);
  void siftDown(std::size_t index);
};

}  // namespace ig::dsa
