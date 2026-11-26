#include "dsa/dynamic_vector.hpp"

#include <algorithm>
#include <cstring>
#include <new>

namespace ig::dsa {

template <typename T>
DynamicVector<T>::DynamicVector(std::initializer_list<T> init) {
  reserve(init.size());
  for (const T& item : init) pushBack(item);
}

template <typename T>
DynamicVector<T>::~DynamicVector() {
  delete[] data_;
}

template <typename T>
DynamicVector<T>::DynamicVector(const DynamicVector& other) {
  reserve(other.capacity_);
  for (std::size_t i = 0; i < other.size_; ++i) pushBack(other.data_[i]);
}

template <typename T>
DynamicVector<T>::DynamicVector(DynamicVector&& other) noexcept
    : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
  other.data_ = nullptr;
  other.size_ = other.capacity_ = 0;
}

template <typename T>
DynamicVector<T>& DynamicVector<T>::operator=(const DynamicVector& other) {
  if (this == &other) return *this;
  DynamicVector copy(other);
  *this = std::move(copy);
  return *this;
}

template <typename T>
DynamicVector<T>& DynamicVector<T>::operator=(DynamicVector&& other) noexcept {
  if (this == &other) return *this;
  delete[] data_;
  data_ = other.data_;
  size_ = other.size_;
  capacity_ = other.capacity_;
  other.data_ = nullptr;
  other.size_ = other.capacity_ = 0;
  return *this;
}

template <typename T>
void DynamicVector<T>::reserve(std::size_t n) {
  if (n <= capacity_) return;
  T* next = new T[n];
  for (std::size_t i = 0; i < size_; ++i) next[i] = std::move(data_[i]);
  delete[] data_;
  data_ = next;
  capacity_ = n;
}

template <typename T>
void DynamicVector<T>::grow() {
  reserve(capacity_ == 0 ? 4 : capacity_ * 2);
}

template <typename T>
void DynamicVector<T>::pushBack(const T& value) {
  if (size_ == capacity_) grow();
  data_[size_++] = value;
}

template <typename T>
void DynamicVector<T>::pushBack(T&& value) {
  if (size_ == capacity_) grow();
  data_[size_++] = std::move(value);
}

template <typename T>
T DynamicVector<T>::popBack() {
  T value = std::move(data_[size_ - 1]);
  --size_;
  return value;
}

template <typename T>
const T& DynamicVector<T>::at(std::size_t index) const {
  return data_[index];
}

template <typename T>
T& DynamicVector<T>::at(std::size_t index) {
  return data_[index];
}

template <typename T>
bool DynamicVector<T>::eraseAt(std::size_t index) {
  if (index >= size_) return false;
  for (std::size_t i = index; i + 1 < size_; ++i) data_[i] = std::move(data_[i + 1]);
  --size_;
  return true;
}

template <typename T>
bool DynamicVector<T>::eraseValue(const T& value) {
  for (std::size_t i = 0; i < size_; ++i) {
    if (data_[i] == value) return eraseAt(i);
  }
  return false;
}

template class DynamicVector<long long>;
template class DynamicVector<std::string>;

}  // namespace ig::dsa
