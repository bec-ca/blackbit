#include "move.hpp"

#include "yasf/serializer.hpp"

using std::string;

namespace blackbit {
namespace {

bee::OrError<int> file_letter_to_idx(char c)
{
  if (c >= 'a' && c <= 'h') {
    return c - 'a';
  } else {
    return bee::Error::format("Character is not a valid file: $", c);
  }
}

bee::OrError<int> row_number_to_idx(char c)
{
  if (c >= '1' && c <= '8') {
    return c - '1';
  } else {
    return bee::Error::format("Character is not a valid row: $", c);
  }
}

bee::OrError<PieceType> piece_from_promo_letter(char c)
{
  switch (c) {
  case 'n':
    return PieceType::KNIGHT;
  case 'b':
    return PieceType::BISHOP;
  case 'r':
    return PieceType::ROOK;
  case 'q':
    return PieceType::QUEEN;
  default:
    shot("Not a valid promotion letter, $", c);
  }
}

} // namespace

std::string Move::to_string() const
{
  std::string output;
  output += (oc() + 'a');
  output += (ol() + '1');
  output += (dc() + 'a');
  output += (dl() + '1');
  if (_promotion != PieceType::CLEAR) {
    switch (_promotion) {
    case PieceType::PAWN:
      assert(false && "This shouldn't happen");
    case PieceType::KNIGHT:
      output += 'n';
      break;
    case PieceType::BISHOP:
      output += 'b';
      break;
    case PieceType::ROOK:
      output += 'r';
      break;
    case PieceType::QUEEN:
      output += 'q';
      break;
    case PieceType::KING:
      output += 'k';
      break;
    case PieceType::CLEAR:
      break;
    }
  }
  return output;
}

bee::OrError<Move> Move::of_string(const string& move_str)
{
  PieceType promoted = PieceType::CLEAR;
  if (move_str.size() == 5) {
    char letter = tolower(move_str[4]);
    bail_assign(promoted, piece_from_promo_letter(letter));
  } else if (move_str.size() != 4) {
    shot("Move string must have length 4 or be castle, got '$'", move_str);
  }
  bail(from_file_idx, file_letter_to_idx(move_str[0]));
  bail(from_row_idx, row_number_to_idx(move_str[1]));
  bail(to_file_idx, file_letter_to_idx(move_str[2]));
  bail(to_row_idx, row_number_to_idx(move_str[3]));

  return Move(from_row_idx, from_file_idx, to_row_idx, to_file_idx, promoted);
}

yasf::Value::ptr Move::to_yasf_value() const
{
  return yasf::ser<std::string>(to_string());
}

bee::OrError<Move> Move::of_yasf_value(const yasf::Value::ptr& value)
{
  bail(str, yasf::des<std::string>(value));
  return Move::of_string(str);
}

} // namespace blackbit
