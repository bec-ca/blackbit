#include "eval.hpp"

#include "board.hpp"
#include "rules.hpp"

#include "bee/testing.hpp"

using bee::print_line;
using std::string;

namespace blackbit {
namespace {

TEST(king_safety)
{
  auto experiment = Experiment::base();
  auto eval_king = [&](const string& fen) {
    Board board;
    must_unit(board.set_fen(fen));
    print_line(board.to_fen());
    print_line(
      "eval: $",
      Evaluator::eval_king_safety(
        board, Rules::make_scratch(board), board.turn, experiment));
    print_line("--------------------------------");
  };

  eval_king("q7/8/8/8/8/Q7/PPPPPPPP/4K3 w");
  eval_king("q7/8/8/8/8/Q7/PP1PPPPP/4K3 w");
  eval_king("q7/8/8/8/8/Q7/PPP1PPPP/4K3 w");
  eval_king("q7/8/8/8/8/Q7/PPPP1PPP/4K3 w");
  eval_king("q7/8/8/8/8/Q7/PPPPP1PP/4K3 w");
  eval_king("q7/8/8/8/8/Q7/PPPPPP1P/4K3 w");

  print_line("--------------------------------");
  print_line("rough safety for white");
  eval_king("q7/8/8/8/8/8/8/K7 w");
  eval_king("q7/8/8/8/8/8/8/1K6 w");
  eval_king("q7/8/8/8/8/8/8/2K5 w");
  eval_king("q7/8/8/8/8/8/8/3K4 w");
  eval_king("q7/8/8/8/8/8/8/4K3 w");
  eval_king("q7/8/8/8/8/8/8/5K2 w");
  eval_king("q7/8/8/8/8/8/8/6K1 w");
  eval_king("q7/8/8/8/8/8/8/7K w");
  eval_king("8/8/8/8/8/8/8/7K w");

  print_line("rough safety for black");
  eval_king("k7/8/8/8/8/8/8/Q7 b");
  eval_king("1k6/8/8/8/8/8/8/Q7 b");
  eval_king("2k5/8/8/8/8/8/8/Q7 b");
  eval_king("3k4/8/8/8/8/8/8/Q7 b");
  eval_king("4k3/8/8/8/8/8/8/Q7 b");
  eval_king("5k2/8/8/8/8/8/8/Q7 b");
  eval_king("6k1/8/8/8/8/8/8/Q7 b");
  eval_king("7k/8/8/8/8/8/8/Q7 b");
  eval_king("7k/8/8/8/8/8/8/8 b");

  print_line("--------------------------------");
  print_line("rough safety with pawns for white");
  eval_king("q7/8/8/8/8/8/PPP5/K7 w");
  eval_king("q7/8/8/8/8/8/1PP5/K7 w");
  eval_king("q7/8/8/8/8/8/PPP5/1K6 w");
  eval_king("q7/8/8/8/8/8/1PP5/1K6 w");
  eval_king("q7/8/8/8/8/8/PPP5/2K5 w");
  eval_king("q7/8/8/8/8/8/PPP5/3K4 w");

  eval_king("q7/8/8/8/8/8/5PPP5/4K3 w");
  eval_king("q7/8/8/8/8/8/5PPP/5K2 w");
  eval_king("q7/8/8/8/8/8/5PPP/6K1 w");
  eval_king("q7/8/8/8/8/8/5PPP/7K w");
  eval_king("q7/8/8/8/8/7P/5PP1/7K w");
  eval_king("q7/8/8/8/8/6P1/5P1P/7K w");

  print_line("rough safety with pawns for black");
  eval_king("k7/ppp5/8/8/8/8/8/Q7 b");
  eval_king("k7/1pp5/8/8/8/8/8/Q7 b");
  eval_king("1k6/ppp5/8/8/8/8/8/Q7 b");
  eval_king("1k6/1pp5/8/8/8/8/8/Q7 b");
  eval_king("2k5/ppp5/8/8/8/8/8/Q7 b");
  eval_king("3k4/ppp5/8/8/8/8/8/Q7 b");

  eval_king("4k3/5ppp/8/8/8/8/8/Q7 b");
  eval_king("5k2/5ppp/8/8/8/8/8/Q7 b");
  eval_king("6k1/5ppp/8/8/8/8/8/Q7 b");
  eval_king("7k/5ppp/8/8/8/8/8/Q7 b");
  eval_king("7k/5pp1/7p/8/8/8/8/Q7 b");
  eval_king("7k/5p1p/6p1/8/8/8/8/Q7 b");
}

void eval(const Board& board)
{
  print_line(board.to_fen());
  print_line(
    "eval: $",
    Evaluator::eval_for_current_player(
      board,
      Rules::make_scratch(board),
      Experiment::base(),
      EvalParameters::default_params()));
  auto score =
    board.material_score(Color::White) - board.material_score(Color::Black);
  print_line("material: $", score);
  print_line("--------------------------------");
};

TEST(eval)
{
  Board board;

  board.set_initial();
  eval(board);

  auto pp = [&](const string& m) { return Rules::parse_pretty_move(board, m); };

  must(m1, pp("e4"));
  auto mi1 = board.move(m1);
  eval(board);

  must(m2, pp("d5"));
  auto mi2 = board.move(m2);
  eval(board);

  must(m3, pp("exd5"));
  auto mi3 = board.move(m3);
  eval(board);

  must(m4, pp("Qxd5"));
  auto mi4 = board.move(m4);
  eval(board);

  board.undo(m4, mi4);
  eval(board);

  board.undo(m3, mi3);
  eval(board);

  board.undo(m2, mi2);
  eval(board);

  board.undo(m1, mi1);
  eval(board);
}

void eval_pawn(const string& fen)
{
  Board board;
  must_unit(board.set_fen(fen));
  print_line(board.to_fen());
  print_line(
    "eval: $", Evaluator::eval_pawns(board, board.turn, Experiment::base()));
  print_line("--------------------------------");
};

TEST(advanced_pawn)
{
  eval_pawn("8/8/8/8/8/8/8/8 w");

  print_line("-------------------------------------------------------------");
  print_line("White");
  print_line("H file");
  eval_pawn("8/8/8/8/8/8/7P/8 w");
  eval_pawn("8/8/8/8/8/7P/8/8 w");
  eval_pawn("8/8/8/8/7P/8/8/8 w");
  eval_pawn("8/8/8/7P/8/8/8/8 w");
  eval_pawn("8/8/7P/8/8/8/8/8 w");
  eval_pawn("8/7P/8/8/8/8/8/8 w");

  print_line("E file");
  eval_pawn("8/8/8/8/8/8/4P3/8 w");
  eval_pawn("8/8/8/8/8/4P3/8/8 w");
  eval_pawn("8/8/8/8/4P3/8/8/8 w");
  eval_pawn("8/8/8/4P3/8/8/8/8 w");
  eval_pawn("8/8/4P3/8/8/8/8/8 w");
  eval_pawn("8/4P3/8/8/8/8/8/8 w");

  print_line("-------------------------------------------------------------");
  print_line("Black");
  print_line("H file");
  eval_pawn("8/8/8/8/8/8/7p/8 b");
  eval_pawn("8/8/8/8/8/7p/8/8 b");
  eval_pawn("8/8/8/8/7p/8/8/8 b");
  eval_pawn("8/8/8/7p/8/8/8/8 b");
  eval_pawn("8/8/7p/8/8/8/8/8 b");
  eval_pawn("8/7p/8/8/8/8/8/8 b");

  print_line("E file");
  eval_pawn("8/8/8/8/8/8/4p3/8 b");
  eval_pawn("8/8/8/8/8/4p3/8/8 b");
  eval_pawn("8/8/8/8/4p3/8/8/8 b");
  eval_pawn("8/8/8/4p3/8/8/8/8 b");
  eval_pawn("8/8/4p3/8/8/8/8/8 b");
  eval_pawn("8/4p3/8/8/8/8/8/8 b");
}

TEST(rook_on_open_file)
{
  auto exp = Experiment::base();
  auto eval_rook = [&exp](const string& fen) {
    Board board;
    must_unit(board.set_fen(fen));
    print_line(board.to_fen());
    print_line(
      "eval: $", Evaluator::eval_rooks_on_open_file(board, board.turn, exp));
    print_line("--------------------------------");
  };

  eval_rook("8/8/8/8/8/8/8/8 w");

  print_line("-------------------------------------------------------------");
  eval_rook("8/8/8/8/8/8/8/R7 w");
  eval_rook("8/p7/8/8/8/8/8/R7 w");
  eval_rook("8/8/8/8/8/8/8/R7 w");
  eval_rook("8/1p6/8/8/8/8/8/R7 w");

  print_line("-------------------------------------------------------------");
  eval_rook("r7/8/8/8/8/8/8/8 b");
  eval_rook("r7/8/8/8/8/8/P7/8 b");
  eval_rook("r7/8/8/8/8/8/1P6/8 b");
}

TEST(custom_eval)
{
  auto exp = Experiment::base();
  auto default_params = EvalParameters::default_params();
  auto custom_params = EvalParameters::default_params();
  custom_params.custom_eval = [](const Features& features, const Board&) {
    return features.white().current_eval - features.black().current_eval;
  };
  auto eval = [&](const string& fen) {
    Board board;
    must_unit(board.set_fen(fen));
    auto default_eval = Evaluator::eval_for_white(
      board, Rules::make_scratch(board), exp, default_params);
    auto custom_eval = Evaluator::eval_for_white(
      board, Rules::make_scratch(board), exp, custom_params);
    print_line(board.to_fen());
    print_line("default_eval: $", default_eval);
    print_line("custom_eval: $", custom_eval);
    print_line("--------------------------------");
  };

  eval("rn1qkbnr/p2bpppp/8/1p6/P1pN4/4P3/1P3PPP/RNBQKB1R w KQkq - 0 7");

  custom_params.custom_eval = [](const Features& features, const Board&) {
    return (features.white().current_eval - features.black().current_eval) * 2;
  };
  eval("rn1qkbnr/p2bpppp/8/1p6/P1pN4/4P3/1P3PPP/RNBQKB1R w KQkq - 0 7");

  custom_params.custom_eval = [](const Features& features, const Board&) {
    return (features.white().current_eval + features.white().attack_points) -
           (features.black().current_eval + features.black().attack_points);
  };
  eval("2r1r1k1/1b1p1p1p/p2B2p1/1p2PpP1/7P/2n4R/P1P3P1/R6K b - - 0 21");
}

} // namespace

} // namespace blackbit
