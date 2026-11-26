#include "dsa/max_heap.hpp"

#include <algorithm>
#include <utility>

namespace ig::dsa {

MaxHeap::MaxHeap(std::vector<HeapItem> items) : data_(std::move(items)) {
  heapify();
}

bool MaxHeap::higher(const HeapItem& a, const HeapItem& b) {
  if (a.score == b.score) return a.id > b.id;  // deterministic tie-break
  return a.score > b.score;
}

void MaxHeap::siftUp(std::size_t index) {
  while (index > 0) {
    std::size_t parent = (index - 1) / 2;
    if (!higher(data_[index], data_[parent])) break;
    std::swap(data_[index], data_[parent]);
    index = parent;
  }
}

void MaxHeap::siftDown(std::size_t index) {
  const std::size_t n = data_.size();
  while (true) {
    std::size_t left = 2 * index + 1;
    std::size_t right = left + 1;
    std::size_t best = index;
    if (left < n && higher(data_[left], data_[best])) best = left;
    if (right < n && higher(data_[right], data_[best])) best = right;
    if (best == index) break;
    std::swap(data_[index], data_[best]);
    index = best;
  }
}

void MaxHeap::insert(long long id, double score) {
  data_.push_back({id, score});
  siftUp(data_.size() - 1);
}

HeapItem MaxHeap::peek() const { return data_.front(); }

HeapItem MaxHeap::pop() {
  HeapItem top = data_.front();
  std::swap(data_.front(), data_.back());
  data_.pop_back();
  if (!data_.empty()) siftDown(0);
  return top;
}

bool MaxHeap::removeAt(std::size_t index) {
  if (index >= data_.size()) return false;
  std::swap(data_[index], data_.back());
  data_.pop_back();
  if (index < data_.size()) {
    // The replacement may need to move in either direction.
    siftUp(index);
    siftDown(index);
  }
  return true;
}

bool MaxHeap::removeId(long long id) {
  for (std::size_t i = 0; i < data_.size(); ++i) {
    if (data_[i].id == id) return removeAt(i);
  }
  return false;
}

void MaxHeap::heapify() {
  if (data_.empty()) return;
  for (std::size_t i = data_.size() / 2; i-- > 0;) siftDown(i);
}

}  // namespace ig::dsa
