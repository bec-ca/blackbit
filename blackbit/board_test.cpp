#include "bee/testing.hpp"
#include "blackbit/game_result.hpp"

#include "board.hpp"
#include "rules.hpp"

#include "bee/error.hpp"

#include <iostream>

using bee::print_line;
using std::string;

namespace blackbit {
namespace {

TEST(sizeof_board) { print_line(sizeof(Board)); }

string pp(const Board& b, Move m) { return Rules::pretty_move(b, m); }

TEST(castle_movement)
{
  Board board;
  board.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w K");

  print_line(board.to_fen());
  print_line(board.hash_key());

  must(m, board.parse_xboard_move_string("O-O"));

  print_line(pp(board, m));

  auto mi = board.move(m);
  print_line(board.to_fen());
  print_line(board.hash_key());
  board.undo(m, mi);
  print_line(board.to_fen());
  print_line(board.hash_key());
}

TEST(FEN)
{
  auto run = [&](const string& fen) {
    Board board;
    board.set_fen(fen);
    string computed_fen = board.to_fen();

    print_line(fen);
    print_line(computed_fen);
    print_line(fen == computed_fen);
  };

  run("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBKR w");
  run("8/8/8/8/8/8/8/8 w");
}

TEST(HASH)
{
  Board board;
  board.set_fen("2rq1k1r/1p3ppp/p2bbp2/3p4/Q4P1P/1PNBP3/P1PK2P1/R6R b");

  print_line(board.hash_key());

  must(move, Rules::parse_pretty_move(board, "Ke7"));

  print_line(board.hash_key());

  auto mi = board.move(move);

  print_line(board.hash_key());

  board.undo(move, mi);

  print_line(board.hash_key());

  board.move(move);

  print_line(board.hash_key());
}

TEST(blockers)
{
  auto run_test = [](const string& fen) {
    Board board;
    board.set_fen(fen);
    print_line(board.to_fen());
    print_line(board.bb_blockers[Color::White].to_string());
    print_line(board.bb_blockers[Color::Black].to_string());
    print_line("--------------------------------");
  };

  run_test("8/8/8/8/8/8/8/8 w");
  run_test("8/8/8/3R4/8/8/8/8 w");
  run_test("8/8/8/3R4/8/3p4/8/8 w");
  run_test("8/8/8/3R4/8/3n4/8/8 w");
  run_test("8/2n5/8/3R4/8/3n4/8/8 w");
  run_test("8/3n4/8/3R4/8/3n4/8/8 w");
  run_test("8/8/8/3N4/8/8/8/8 w");
  run_test("8/8/8/3N4/8/4p3/8/8 w");
  run_test("8/4b3/8/3N4/8/4n3/8/8 w");
  run_test("8/8/8/3B4/8/8/8/8 w");
  run_test("8/8/4n3/3B4/8/8/8/8 w");
  run_test("8/8/4n3/3B4/4n3/8/8/8 w");
  run_test("8/8/2n1n3/3B4/2n1n3/8/8/8 w");
  run_test("8/8/8/3Q4/8/8/8/8 w");
  run_test("8/8/2n1n3/3Q4/2n1n3/8/8/8 w");
  run_test("8/8/2nnn3/2nQn3/2nnn3/8/8/8 w");
  run_test("8/8/2nNn3/2nQn3/2nnN3/8/8/8 w");
}

} // namespace
} // namespace blackbit
