#pragma once

#include "generated_board_hashes.hpp"
#include "pieces.hpp"

namespace blackbit {

struct CastleFlags {
 public:
  static CastleFlags none() { return CastleFlags(0); }

  static CastleFlags all() { return CastleFlags(WhiteMask | BlackMask); }

  void clear(Color color) { _flags &= ~mask(color); }
  void clear_king(Color color) { _flags &= ~(mask(color) & KingMask); }
  void clear_queen(Color color) { _flags &= ~(mask(color) & QueenMask); }

  void set_king(Color color) { _flags |= mask(color) & KingMask; }
  void set_queen(Color color) { _flags |= mask(color) & QueenMask; }

  bool can_castle(Color color) const { return (mask(color) & _flags) > 0; }
  bool can_castle_king_side(Color color) const
  {
    return (mask(color) & KingMask & _flags) > 0;
  }
  bool can_castle_queen_side(Color color) const
  {
    return (mask(color) & QueenMask & _flags) > 0;
  }

  bool is_clear() const { return _flags == 0; }

  bool operator==(const CastleFlags& other) const
  {
    return _flags == other._flags;
  }
  bool operator!=(const CastleFlags& other) const
  {
    return _flags != other._flags;
  }
  uint64_t hash() const { return castle_hash.at(_flags); }

 private:
  static const uint8_t WhiteKing = 1 << 0;
  static const uint8_t WhiteQueen = 1 << 1;
  static const uint8_t BlackKing = 1 << 2;
  static const uint8_t BlackQueen = 1 << 3;

  static const uint8_t WhiteMask = WhiteKing | WhiteQueen;
  static const uint8_t BlackMask = BlackKing | BlackQueen;

  static const uint8_t KingMask = WhiteKing | BlackKing;
  static const uint8_t QueenMask = WhiteQueen | BlackQueen;

  static uint8_t mask(Color color)
  {
    switch (color) {
    case Color::White:
      return WhiteMask;
    case Color::Black:
      return BlackMask;
    case Color::None:
      assert(false);
    }
    assert(false);
  }

  CastleFlags(uint8_t flags) : _flags(flags) {}

  uint8_t _flags;
};

} // namespace blackbit
