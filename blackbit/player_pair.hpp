#pragma once

#include "color.hpp"

namespace blackbit {

template <class T> struct PlayerPair {
 public:
  constexpr PlayerPair(const T& v) : _white(v), _black(v) {}
  constexpr PlayerPair(const T& white, const T& black)
      : _white(white), _black(black)
  {}

  constexpr PlayerPair() {}

  constexpr const T& get(Color color) const
  {
    switch (color) {
    case Color::White:
      return _white;
    case Color::Black:
      return _black;
    case Color::None:
      assert(false);
    }
    assert(false);
  }

  constexpr T& get(Color color)
  {
    switch (color) {
    case Color::White:
      return _white;
    case Color::Black:
      return _black;
    case Color::None:
      assert(false);
    }
    assert(false);
  }

  constexpr const T& white() const { return _white; }
  constexpr T& white() { return _white; }

  constexpr const T& black() const { return _black; }
  constexpr T& black() { return _black; }

 private:
  T _white;
  T _black;
};

} // namespace blackbit
