#pragma once
// Custom dynamic array (vector) with amortised O(1) append and O(1) indexing.
// Used for post collections, comment threads and media lists in service caches.
#include <cstddef>
#include <initializer_list>
#include <string>
#include <utility>

namespace ig::dsa {

template <typename T>
class DynamicVector {
 public:
  DynamicVector() = default;
  DynamicVector(std::initializer_list<T> init);

  void pushBack(const T& value);
  void pushBack(T&& value);
  T popBack();
  const T& at(std::size_t index) const;
  T& at(std::size_t index);
  bool eraseAt(std::size_t index);
  bool eraseValue(const T& value);
  void reserve(std::size_t n);
  void clear() noexcept { size_ = 0; }
  std::size_t size() const noexcept { return size_; }
  std::size_t capacity() const noexcept { return capacity_; }
  bool empty() const noexcept { return size_ == 0; }
  T* data() noexcept { return data_; }
  const T* data() const noexcept { return data_; }

  template <typename F>
  void forEach(F&& fn) const {
    for (std::size_t i = 0; i < size_; ++i) fn(data_[i]);
  }

  T* begin() noexcept { return data_; }
  T* end() noexcept { return data_ + size_; }
  const T* begin() const noexcept { return data_; }
  const T* end() const noexcept { return data_ + size_; }

  ~DynamicVector();
  DynamicVector(const DynamicVector& other);
  DynamicVector(DynamicVector&& other) noexcept;
  DynamicVector& operator=(const DynamicVector& other);
  DynamicVector& operator=(DynamicVector&& other) noexcept;

 private:
  T* data_ = nullptr;
  std::size_t size_ = 0;
  std::size_t capacity_ = 0;

  void grow();
};

extern template class DynamicVector<long long>;
extern template class DynamicVector<std::string>;

}  // namespace ig::dsa
