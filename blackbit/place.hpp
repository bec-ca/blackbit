#pragma once

#include "bee/error.hpp"
#include "color.hpp"

#include <cassert>
#include <cstdint>

namespace blackbit {

enum class Direction {
  Up,
  Right,
  Down,
  Left,
};

struct __attribute__((__packed__)) Place {
 public:
  constexpr inline Place() : _place(64) {}
  /* not BB */
  constexpr inline int8_t line() const { return _place >> 3; }

  /* not BB */
  constexpr inline int8_t col() const { return _place & 7; }

  /* not BB */
  constexpr static inline Place of_line_of_col(int8_t lin, int8_t col)
  {
    return Place(col | (lin << 3));
  }

  inline Place down() const { return Place(_place - 8); }

  inline Place up() const { return Place(_place + 8); }

  inline Place right() const { return Place(_place + 1); }

  inline Place left() const { return Place(_place - 1); }

  inline Place move(Direction dir)
  {
    switch (dir) {
    case Direction::Up:
      return up();
    case Direction::Right:
      return right();
    case Direction::Down:
      return down();
    case Direction::Left:
      return left();
    }
    assert(false);
  }

  inline constexpr Place mirror() const
  {
    return of_line_of_col(7 - line(), col());
  }

  inline constexpr Place player_view(Color color) const
  {
    switch (color) {
    case Color::White:
      return *this;
    case Color::Black:
      return mirror();
    case Color::None:
      assert(false);
    }
  }

  static inline Place of_int(int8_t place) { return Place(place); }

  constexpr inline int8_t to_int() const { return _place; }

  inline bool operator==(const Place& other) const
  {
    return _place == other._place;
  }
  inline bool operator!=(const Place& other) const
  {
    return _place != other._place;
  }

  inline bool operator<(const Place other) const
  {
    return _place < other._place;
  }

  inline bool is_valid() const { return _place >= 0 && _place < 64; }

  static inline Place invalid() { return of_int(64); }

  inline explicit operator int() const { return _place; }

  std::string to_string() const
  {
    std::string str;
    str += ('a' + col());
    str += ('1' + line());
    return str;
  }

  static bee::OrError<Place> of_string(const std::string& str)
  {
    if (str.size() != 2) { return bee::Error("Invalid place string"); }
    int col = str[0] - 'a';
    int line = str[1] - '1';
    if (col < 0 || col > 7 || line < 0 || line > 7) {
      return bee::Error("Invalid place string");
    }
    return of_line_of_col(line, col);
  }

 private:
  int8_t _place;

  constexpr inline Place(int8_t place) : _place(place) {}
};

struct PlaceIterator {
 public:
  struct Wrapper {
   public:
    Wrapper(const Place& p) : p(p) {}

    inline Wrapper operator++(int)
    {
      auto ret = *this;
      p = p.right();
      return ret;
    }

    inline Wrapper& operator++()
    {
      p = p.right();
      return *this;
    }

    inline bool operator==(const Wrapper& other) const { return p == other.p; }
    inline bool operator!=(const Wrapper& other) const { return p != other.p; }

    const Place& operator*() const { return p; }

   private:
    Place p;
  };

  Wrapper begin() const { return Place::of_int(0); }

  Wrapper end() const { return Place::of_int(64); }
};

} // namespace blackbit
