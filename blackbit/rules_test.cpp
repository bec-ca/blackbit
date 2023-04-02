#include "bee/testing.hpp"

#include "rules.hpp"

using bee::print_line;
using std::string;

namespace blackbit {
namespace {

void run_tests(const string& fen, PieceType piece_type)
{
  Board board;
  board.set_fen(fen);

  MoveVector moves;
  auto scratch = Rules::make_scratch(board);
  Rules::list_moves(board, scratch, moves);

  int count = 0;
  print_line(fen);
  for (auto& m : moves) {
    if (board[m.o].type != piece_type) continue;
    ++count;
    print_line("$: legal:$", m, Rules::is_legal_move(board, scratch, m));
  }
  print_line("num moves: $", count);
  print_line("---------------------------------");
}

TEST(pawn_moves_from_initial)
{
  auto run = [](const string& fen) { run_tests(fen, PieceType::PAWN); };
  run("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -");
}

TEST(knight_moves_from_initial)
{
  auto run = [](const string& fen) { run_tests(fen, PieceType::KNIGHT); };
  run("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -");
}

TEST(bishop_moves)
{
  auto run = [](const string& fen) { run_tests(fen, PieceType::BISHOP); };
  run("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -");
  run("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq -");
  run("rnbqkbnr/pppppppp/8/8/2B1P3/8/PPPP1PPP/RNBQK1NR w KQkq -");
  run("8/8/8/8/4B3/8/8/8 w KQkq -");
  run("8/8/8/3p4/4B3/8/8/8 w KQkq -");
}

TEST(rook_moves)
{
  auto run = [](const string& fen) { run_tests(fen, PieceType::ROOK); };
  run("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -");
  run("rnbqkbnr/pppppppp/8/8/8/8/1PPPPPP1/RNBQKBNR w KQkq -");
  run("rnbqkbnr/pppppppp/8/8/4R3/8/1PPPPPP1/1NBQKBN1 w KQkq -");
  run("8/8/8/8/3pR3/8/8/8 w KQkq -");
}

TEST(queen_moves)
{
  auto run = [](const string& fen) { run_tests(fen, PieceType::QUEEN); };
  run("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -");
  run("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq -");
  run("8/8/8/8/4Q3/8/8/8 w KQkq -");
  run("8/8/8/8/3pQ3/8/8/8 w KQkq -");
}

TEST(king_moves)
{
  auto run = [](const string& fen) { run_tests(fen, PieceType::KING); };
  run("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -");
  run("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq -");
  run("8/8/8/8/4K3/8/8/8 w KQkq -");
  run("8/8/8/8/8/8/8/4K3 w KQkq -");
  run("8/8/8/8/8/8/8/4K3 w KQ -");
  run("8/8/8/8/8/8/8/4K3 w K -");
  run("8/8/8/8/8/8/8/4K3 w Q -");
  run("8/8/8/8/8/8/8/4K3 w -");
}

void run_combined_tests(const string& fen)
{
  Board board;
  print_line(fen);
  board.set_fen(fen);

  MoveVector moves;

  auto scratch = Rules::make_scratch(board);
  Rules::list_moves(board, scratch, moves);

  print_line("num moves: $", moves.size());
  for (auto& m : moves) {
    bool is_legal = Rules::is_legal_move(board, scratch, m);
    string pretty = Rules::pretty_move(board, m);
    auto mi = board.move(m);
    bool is_mate = Rules::is_mate(board, Rules::make_scratch(board));
    board.undo(m, mi);
    print_line("$: valid:$ mate:$", pretty, is_legal, is_mate);
  }
  print_line("---------------------------------");
}

TEST(castle)
{
  run_combined_tests("k7/8/8/8/8/8/8/4K2R w KQkq -");
  run_combined_tests("k3r3/8/8/8/8/8/8/4K2R w KQkq -");
  run_combined_tests("k4r2/8/8/8/8/8/8/4K2R w KQkq -");
  run_combined_tests("k5r1/8/8/8/8/8/8/4K2R w KQkq -");
  run_combined_tests("k6r/8/8/8/8/8/8/4K2R w KQkq -");
}

TEST(bug)
{
  run_combined_tests(
    "1r3k2/p2n1ppp/b1p2n2/5B2/4N3/P1q5/2P1pPPP/2RQK2R b - - 0 20");
}

TEST(mate_bug)
{
  run_combined_tests("k6/2p5/p2qp3/p6p/2KPb2P/1P3r2/P1R5/R7 b - - 0 42");
}

void run_draw_test(const string& fen)
{
  Board board;
  print_line(fen);
  board.set_fen(fen);
  for (auto color : AllColors) {
    for (auto piece : AllPieces) {
      auto count = board.pieces(color, piece).size();
      if (count > 0) { print_line("$:$:$", color, piece, count); }
    }
  }
  print_line(
    "is_draw: $",
    Rules::result(board, Rules::make_scratch(board)) == GameResult::Draw);
  print_line("---------------------------------");
}

TEST(draw)
{
  run_draw_test("k7/8/K7/8/8/8/8/8 b - - 0 42");
  run_draw_test("k7/8/K7/8/n7/8/8/8 b - - 0 42");
  run_draw_test("k7/8/K7/8/N7/8/8/8 b - - 0 42");
  run_draw_test("k7/8/K7/8/b7/8/8/8 b - - 0 42");
  run_draw_test("k7/8/K7/8/B7/8/8/8 b - - 0 42");
  run_draw_test("k7/8/K7/8/B7/B7/8/8 b - - 0 42");
  run_draw_test("k7/8/K7/8/N7/N7/8/8 b - - 0 42");
  run_draw_test("k7/8/K7/8/8/8/8/r7 b - - 0 42");
  run_draw_test("k7/8/K7/8/8/8/8/R7 b - - 0 42");
}

TEST(pretty_move)
{
  auto run = [](const string& fen, string m) {
    Board b;
    b.set_fen(fen);
    print_line("$", Rules::pretty_move(b, Move::of_string(m).value()));
  };
  run("k7/pppppppp/8/8/8/8/K7/R6R w", "a1c1");
  run("k7/pppppppp/8/8/8/8/K7/R7 w", "a1c1");
  run("k7/pppppppp/8/8/8/8/K7/R6R w", "h1c1");
  run("k7/pppppppp/8/8/R7/8/K7/R7 w", "a4a3");
  run("k7/pppppppp/8/8/R7/8/7K/R7 w", "a4a3");
  run("k7/pppppppp/8/8/R7/8/7K/R7 w", "a1a3");
  run("k6K/pppppppp/2N1N3/8/8/8/8/8 w", "c6d4");
  run("k6K/pppppppp/2N1N3/8/8/8/2N1N3/8 w", "c6d4");
  run("k6K/pppppppp/2N1N3/8/3p4/8/2N1N3/8 w", "c6d4");
}

} // namespace

} // namespace blackbit
