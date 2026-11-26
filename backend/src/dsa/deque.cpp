#include "dsa/deque.hpp"

namespace ig::dsa {

IdDeque::IdDeque(std::size_t capacity) : buffer_(capacity == 0 ? 4 : capacity) {}

void IdDeque::grow() {
  std::vector<long long> next(buffer_.size() * 2);
  std::size_t i = 0;
  forEach([&](long long v) { next[i++] = v; });
  buffer_ = std::move(next);
  head_ = 0;
  tail_ = size_;
}

void IdDeque::writeTail(long long value) {
  buffer_[tail_] = value;
  tail_ = (tail_ + 1) % buffer_.size();
  ++size_;
}

void IdDeque::writeHead(long long value) {
  head_ = (head_ + buffer_.size() - 1) % buffer_.size();
  buffer_[head_] = value;
  ++size_;
}

void IdDeque::pushBack(long long value) {
  if (size_ == buffer_.size()) grow();
  writeTail(value);
}

void IdDeque::pushFront(long long value) {
  if (size_ == buffer_.size()) grow();
  writeHead(value);
}

long long IdDeque::popFront() {
  long long v = buffer_[head_];
  head_ = (head_ + 1) % buffer_.size();
  --size_;
  return v;
}

long long IdDeque::popBack() {
  tail_ = (tail_ + buffer_.size() - 1) % buffer_.size();
  long long v = buffer_[tail_];
  --size_;
  return v;
}

long long IdDeque::front() const { return buffer_[head_]; }
long long IdDeque::back() const {
  return buffer_[(tail_ + buffer_.size() - 1) % buffer_.size()];
}
long long IdDeque::at(std::size_t index) const {
  return buffer_[(head_ + index) % buffer_.size()];
}

bool IdDeque::eraseValue(long long value) {
  std::vector<long long> kept;
  kept.reserve(size_);
  forEach([&](long long v) {
    if (v != value) kept.push_back(v);
  });
  if (kept.size() == size_) return false;
  clear();
  for (long long v : kept) pushBack(v);
  return true;
}

}  // namespace ig::dsa
