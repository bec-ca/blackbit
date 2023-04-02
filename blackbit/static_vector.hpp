#pragma once

#include <cassert>
#include <cstdint>
#include <vector>

namespace blackbit {

template <class T, int cap> struct StaticVector {
 public:
  bool full() const { return _size >= cap; }

  bool empty() const { return _size == 0; }

  void clear() { _size = 0; }

  void push_back(const T& value)
  {
    // assert(!full());
    _array[_size++] = value;
  }

  void pop_back()
  {
    // assert(!empty());
    _size--;
  }

  const T& operator[](int idx) const
  {
    // assert(idx < _size && idx >= 0);
    return _array[idx];
  }

  T& operator[](int idx)
  {
    // assert(idx < _size && idx >= 0);
    return _array[idx];
  }

  T* begin() { return &_array[0]; }
  T* end() { return &_array[_size]; }

  const T* begin() const { return &_array[0]; }
  const T* end() const { return &_array[_size]; }

  int size() const { return _size; }

  std::vector<T> to_vector() const { return std::vector<T>(begin(), end()); }

 private:
  T _array[cap];
  int _size = 0;
};

} // namespace blackbit
