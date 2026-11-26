#pragma once
// Circular-buffer FIFO queue used by the notification/message event processors.
// enqueue / dequeue / front: O(1) amortised.
#include <cstddef>
#include <vector>

namespace ig::dsa {

class IdQueue {
 public:
  explicit IdQueue(std::size_t capacity = 128);

  void enqueue(long long value);
  long long dequeue();
  long long front() const;
  bool empty() const noexcept { return size_ == 0; }
  std::size_t size() const noexcept { return size_; }
  std::size_t capacity() const noexcept { return buffer_.size(); }
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
};

}  // namespace ig::dsa
