#pragma once

#include "bee/to_string.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <string>

namespace blackbit {

enum struct Color : std::int8_t {
  Black = 0,
  White = 1,
  None = 2,
};

constexpr Color oponent(Color p) { return Color(int(p) ^ 1); }

constexpr std::array<Color, 2> AllColors = {Color::White, Color::Black};

} // namespace blackbit

namespace bee {

template <> struct to_string<blackbit::Color> {
  static std::string convert(const blackbit::Color p);
};

} // namespace bee
