#pragma once

#include "bee/format.hpp"

#include <cassert>

namespace blackbit {

template <class K, class V, std::size_t size> struct SpecializedArray {
  V _array[size];

  constexpr V& operator[](K k)
  {
    std::size_t int_k = std::size_t(int(k));
    assert(int_k < size);
    return _array[int_k];
  }
  constexpr const V& operator[](K k) const
  {
    std::size_t int_k = std::size_t(int(k));
    assert(int_k < size);
    return _array[int_k];
  }

  constexpr V* begin() { return &_array[0]; }
  constexpr V* end() { return &_array[size]; }

  constexpr const V* begin() const { return &_array[0]; }
  constexpr const V* end() const { return &_array[size]; }

  constexpr void clear(const V& value)
  {
    for (auto& v : _array) { v = value; }
  }

  std::string to_string(const SpecializedArray<K, V, size>& a)
  {
    std::string output;
    for (const auto& el : a) {
      if (!output.empty()) { output += " "; }
      output += bee::format(el);
    }
    return output;
  }
};

} // namespace blackbit
