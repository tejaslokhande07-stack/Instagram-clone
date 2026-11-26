#pragma once
// Circular-buffer double-ended queue used for story ordering and the
// "recently viewed" / "recent activity" lists.
// push/pop at either end: O(1) amortised, random access: O(1).
#include <cstddef>
#include <vector>

namespace ig::dsa {

class IdDeque {
 public:
  explicit IdDeque(std::size_t capacity = 128);

  void pushFront(long long value);
  void pushBack(long long value);
  long long popFront();
  long long popBack();
  long long front() const;
  long long back() const;
  long long at(std::size_t index) const;
  bool eraseValue(long long value);
  bool empty() const noexcept { return size_ == 0; }
  std::size_t size() const noexcept { return size_; }
  void clear() noexcept { head_ = tail_ = size_ = 0; }
  template <typename F>
  void forEach(F&& fn) const {
    for (std::size_t i = 0; i < size_; ++i) {
      fn(buffer_[(head_ + i) % buffer_.size()]);
    }
  }

 private:
  std::vector<long long> buffer_;
  std::size_t head_ = 0;
  std::size_t tail_ = 0;
  std::size_t size_ = 0;

  void grow();
  void writeTail(long long value);
  void writeHead(long long value);
};

}  // namespace ig::dsa
