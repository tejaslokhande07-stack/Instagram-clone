#include "dsa/queue.hpp"

namespace ig::dsa {

IdQueue::IdQueue(std::size_t capacity) : buffer_(capacity == 0 ? 4 : capacity) {}

void IdQueue::grow() {
  std::vector<long long> next(buffer_.size() * 2);
  std::size_t i = 0;
  forEach([&](long long v) { next[i++] = v; });
  buffer_ = std::move(next);
  head_ = 0;
  tail_ = size_;
}

void IdQueue::enqueue(long long value) {
  if (size_ == buffer_.size()) grow();
  buffer_[tail_] = value;
  tail_ = (tail_ + 1) % buffer_.size();
  ++size_;
}

long long IdQueue::dequeue() {
  long long value = buffer_[head_];
  head_ = (head_ + 1) % buffer_.size();
  --size_;
  return value;
}

long long IdQueue::front() const { return buffer_[head_]; }

}  // namespace ig::dsa
