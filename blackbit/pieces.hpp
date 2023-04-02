#pragma once

#include "bee/to_string.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <string>

namespace blackbit {

// pieces types

enum struct PieceType : std::int8_t {
  CLEAR = 0,
  PAWN = 1,
  KNIGHT = 2,
  BISHOP = 3,
  ROOK = 4,
  QUEEN = 5,
  KING = 6,
};

inline char piece_to_letter(PieceType type)
{
  switch (type) {
  case PieceType::PAWN:
    return 'P';
  case PieceType::KNIGHT:
    return 'N';
  case PieceType::BISHOP:
    return 'B';
  case PieceType::ROOK:
    return 'R';
  case PieceType::QUEEN:
    return 'Q';
  case PieceType::KING:
    return 'K';
  case PieceType::CLEAR:
    return ' ';
  }
  assert(false && "This should not happen");
}

inline PieceType letter_to_piece(char c)
{
  switch (tolower(c)) {
  case 'r':
    return PieceType::ROOK;
  case 'n':
    return PieceType::KNIGHT;
  case 'b':
    return PieceType::BISHOP;
  case 'q':
    return PieceType::QUEEN;
  case 'k':
    return PieceType::KING;
  case 'p':
    return PieceType::PAWN;
  default:
    return PieceType::CLEAR;
  }
}

static constexpr PieceType AllPieces[6] = {
  PieceType::PAWN,
  PieceType::KNIGHT,
  PieceType::BISHOP,
  PieceType::ROOK,
  PieceType::QUEEN,
  PieceType::KING,
};

} // namespace blackbit

namespace bee {

template <> struct to_string<blackbit::PieceType> {
  static std::string convert(const blackbit::PieceType type)
  {
    switch (type) {
    case blackbit::PieceType::PAWN:
      return "pawn";
    case blackbit::PieceType::KNIGHT:
      return "knight";
    case blackbit::PieceType::BISHOP:
      return "bishop";
    case blackbit::PieceType::ROOK:
      return "rook";
    case blackbit::PieceType::QUEEN:
      return "queen";
    case blackbit::PieceType::KING:
      return "king";
    case blackbit::PieceType::CLEAR:
      return "clear";
    }
    assert(false && "This should not happen");
  }
};

} // namespace bee
