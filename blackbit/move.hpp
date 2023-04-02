#pragma once

#include "pieces.hpp"
#include "place.hpp"
#include "yasf/value.hpp"

#include <cstdio>
#include <iostream>

namespace blackbit {

struct __attribute__((__packed__)) Move {
  inline Move(int8_t ol, int8_t oc, int8_t dl, int8_t dc, PieceType promotion)
      : o(Place::of_line_of_col(ol, oc)),
        d(Place::of_line_of_col(dl, dc)),
        _promotion(promotion)
  {}
  inline Move(Place o, Place d, PieceType promotion)
      : o(o), d(d), _promotion(promotion)
  {}

  inline Move() {}

  inline Move(const Move& other) = default;
  inline Move(Move&& other) = default;

  Move& operator=(const Move& other) = default;
  Move& operator=(Move&& other) = default;

  inline bool operator<(const Move other) const
  {
    if (o != other.o) { return o < other.o; }
    return d < other.d;
  }

  inline PieceType promotion() const { return _promotion; }
  inline void set_promotion(PieceType p) { _promotion = p; }

  inline bool is_promotion() const { return promotion() != PieceType::CLEAR; }

  inline int8_t ol() const { return o.line(); }
  inline int8_t oc() const { return o.col(); }
  inline int8_t dl() const { return d.line(); }
  inline int8_t dc() const { return d.col(); }
  inline bool operator==(const Move m) const { return o == m.o and d == m.d; }
  inline bool operator!=(const Move m) const { return o != m.o or d != m.d; }
  inline bool is_valid() const
  {
    return o.is_valid() && d.is_valid() && (o != d);
  }

  static inline Move invalid() { return Move(0, 0, 0, 0, PieceType::CLEAR); }

  Place o, d;

  std::string to_string() const;
  static bee::OrError<Move> of_string(const std::string&);

  yasf::Value::ptr to_yasf_value() const;
  static bee::OrError<Move> of_yasf_value(const yasf::Value::ptr& value);

 private:
  PieceType _promotion;
};

} // namespace blackbit
