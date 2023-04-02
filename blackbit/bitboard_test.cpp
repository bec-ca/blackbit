#include "bitboard.hpp"

#include "bee/testing.hpp"
#include "pieces.hpp"

using bee::print_line;

namespace blackbit {
namespace {

TEST(passed_pawn_mask)
{
  auto print_masks = [](Color player) {
    print_line("--------------------------------");
    print_line("Player: $", player);
    for (auto place : PlaceIterator()) {
      print_line(place);
      auto mask = BitBoard::get_passed_pawn_mask(player, place);
      print_line(mask.to_string());
    }
  };
  print_masks(Color::White);
  print_masks(Color::Black);
}

TEST(passed_column_ahead)
{
  auto print_masks = [](Color player) {
    print_line("--------------------------------");
    print_line("Player: $", player);
    for (auto place : PlaceIterator()) {
      print_line(place);
      auto mask = BitBoard::column_ahead(player, place);
      print_line(mask.to_string());
    }
  };
  print_masks(Color::White);
  print_masks(Color::Black);
}

TEST(mirrored_pair)
{
  BitBoard board(0x0102040810204080u);
  auto pair = BitBoard::mirrored_pair(board);
  print_line(pair[Color::White].to_string());
  print_line(pair[Color::Black].to_string());
}

} // namespace
} // namespace blackbit
