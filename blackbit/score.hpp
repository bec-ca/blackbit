#pragma once

#include "color.hpp"

#include "yasf/serializer.hpp"

#include <cassert>
#include <compare>
#include <cstdint>
#include <limits>
#include <string>

namespace blackbit {

struct Score {
 public:
  using value_type = int32_t;

  static constexpr int32_t pawn_value = 1000;

  static constexpr int32_t max_score = 1 << 30;
  static constexpr int32_t min_score = -max_score;

  static constexpr int32_t mate_score_per_move = 1 << 20;
  static constexpr int32_t max_mate_moves = 1 << 10;

  constexpr Score(const Score& other) = default;
  constexpr Score(Score&& other) = default;
  constexpr ~Score() = default;

  constexpr Score& operator=(const Score& other) = default;
  constexpr Score& operator=(Score&& other) = default;

  double to_pawns() const;

  int to_xboard() const;

  constexpr int to_centi_pawns() const
  {
    return (_score_milli * 100ll) / pawn_value;
  }

  constexpr static Score of_pawns(double pawns)
  {
    if (is_inf(pawns)) {
      auto ret = of_moves_to_mate(0);
      if (pawns < 0) { ret = -ret; }
      return ret;
    }
    return Score(int32_t(pawns * pawn_value));
  }

  constexpr static Score of_centi_pawns(double centi_pawns)
  {
    return of_milli_pawns(centi_pawns * 10.0);
  }

  constexpr static Score of_milli_pawns(int32_t milli_pawns)
  {
    static_assert(pawn_value == 1000);
    return Score(milli_pawns);
  }

  constexpr int32_t to_milli_pawns() const
  {
    static_assert(pawn_value == 1000);
    return _score_milli;
  }

  constexpr static Score of_moves_to_mate(int moves)
  {
    assert(moves < max_mate_moves);
    return Score(mate_score_per_move * (max_mate_moves - moves));
  }

  constexpr int moves_to_mate() const
  {
    assert(is_mate());
    return max_mate_moves - (abs()._score_milli / mate_score_per_move);
  }

  constexpr bool is_mate() const
  {
    return _score_milli <= -mate_score_per_move ||
           _score_milli >= mate_score_per_move;
  }

  constexpr static inline Score zero() noexcept { return Score(0); }

  constexpr static inline Score one_pawn() noexcept
  {
    return Score(pawn_value);
  }

  constexpr static inline Score max() noexcept { return Score(max_score); }

  constexpr static inline Score min() noexcept { return Score(min_score); }

  constexpr inline Score neg_if(bool v) const
  {
    return v ? this->neg() : *this;
  }

  constexpr inline Score flip_for_color(Color c) const
  {
    return c == Color::Black ? this->neg() : *this;
  }

  constexpr Score next() const { return Score(_score_milli + 1); }
  constexpr Score prev() const { return Score(_score_milli - 1); }

  constexpr Score operator+(const Score& other) const
  {
    return Score(_score_milli + other._score_milli);
  }

  Score& operator+=(const Score& other)
  {
    *this = *this + other;
    return *this;
  }

  constexpr Score operator-(const Score& other) const
  {
    return Score(_score_milli - other._score_milli);
  }

  Score& operator-=(const Score& other)
  {
    *this = *this - other;
    return *this;
  }

  constexpr Score operator-() const { return Score(-_score_milli); }

  constexpr inline Score neg() const { return -(*this); }

  constexpr Score operator*(int m) const { return Score(_score_milli * m); }
  constexpr Score operator*(double m) const { return Score(_score_milli * m); }
  constexpr Score operator/(int m) const { return Score(_score_milli / m); }
  constexpr Score operator/(double m) const { return Score(_score_milli / m); }

  constexpr bool operator<(const Score& other) const
  {
    return _score_milli < other._score_milli;
  }
  constexpr bool operator<=(const Score& other) const
  {
    return _score_milli <= other._score_milli;
  }
  constexpr bool operator>(const Score& other) const
  {
    return _score_milli > other._score_milli;
  }
  constexpr bool operator>=(const Score& other) const
  {
    return _score_milli >= other._score_milli;
  }
  constexpr bool operator==(const Score& other) const
  {
    return _score_milli == other._score_milli;
  }
  constexpr bool operator!=(const Score& other) const
  {
    return _score_milli != other._score_milli;
  }

  constexpr Score& operator*=(int m)
  {
    _score_milli *= m;
    return *this;
  }
  constexpr Score& operator*=(double m)
  {
    _score_milli *= m;
    return *this;
  }

  constexpr Score& operator/=(int m)
  {
    _score_milli /= m;
    return *this;
  }

  constexpr Score& operator/=(double m)
  {
    _score_milli /= m;
    return *this;
  }

  constexpr Score operator*(const Score& m) const
  {
    return Score(((int64_t)_score_milli) * m._score_milli / pawn_value);
  }

  constexpr Score operator/(const Score& m) const
  {
    return Score(((int64_t)_score_milli) * pawn_value / m._score_milli);
  }

  constexpr auto operator<=>(const Score& other) const = default;

  constexpr bool is_positive() const { return _score_milli > 0; }
  constexpr bool is_negative() const { return _score_milli < 0; }

  constexpr Score abs() const { return Score(std::abs(_score_milli)); }

  constexpr Score inc_mate_moves(int moves = 1) const
  {
    if (is_mate()) {
      auto d = Score::of_milli_pawns(mate_score_per_move * moves);
      if (is_positive()) {
        return *this - d;
      } else {
        return *this + d;
      }
    } else {
      return *this;
    }
  }

  constexpr Score dec_mate_moves(int moves = 1) const
  {
    if (is_mate()) {
      auto d = Score::of_milli_pawns(mate_score_per_move * moves);
      if (is_positive()) {
        return *this + d;
      } else {
        return *this - d;
      }
    } else {
      return *this;
    }
  }

  std::string to_string() const;

  yasf::Value::ptr to_yasf_value() const;
  static bee::OrError<Score> of_yasf_value(const yasf::Value::ptr&);

 private:
  constexpr explicit Score(int32_t score_milli) : _score_milli(score_milli) {}

  constexpr static double inf = std::numeric_limits<double>::infinity();

  constexpr static bool is_inf(double value)
  {
    return value == inf || value == -inf;
  }

  constexpr bool is_min() const { return *this <= min(); }
  constexpr bool is_max() const { return *this >= max(); }

  int32_t _score_milli;
};

} // namespace blackbit
