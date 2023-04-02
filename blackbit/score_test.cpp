#include "score.hpp"

#include "bee/testing.hpp"

using bee::print_line;

namespace blackbit {
namespace {

TEST(constructor)
{
  print_line(Score::of_pawns(10.0).to_centi_pawns());
  print_line(Score::of_pawns(1.53).to_centi_pawns());
  print_line(Score::of_centi_pawns(254).to_centi_pawns());
  print_line("------------------------------------");

  print_line(Score::of_pawns(10.0).to_pawns());
  print_line(Score::of_pawns(1.53).to_pawns());
  print_line(Score::of_centi_pawns(254).to_pawns());
}

TEST(mate)
{
  auto run_test = [](Score score) {
    print_line("score pawns: $", score.to_pawns());
    print_line("is mate: $", score.is_mate());
    if (score.is_mate()) {
      print_line("moves to mate: $", score.moves_to_mate());
    }
    print_line("------------------------------------");
  };
  run_test(Score::of_pawns(4.0));
  run_test(Score::of_pawns(-4.0));
  run_test(Score::of_moves_to_mate(7));
  run_test(Score::of_moves_to_mate(7).neg());
}

TEST(to_string)
{
  print_line(Score::of_pawns(7.123));
  print_line(Score::of_centi_pawns(534));
  print_line(Score::of_pawns(-7.123));
  print_line(Score::of_centi_pawns(-534));
  print_line(Score::of_moves_to_mate(8));
  print_line(Score::of_moves_to_mate(8).neg());
  print_line(Score::of_moves_to_mate(1));
  print_line(Score::of_moves_to_mate(1).neg());
}

TEST(other_stuff)
{
  print_line(sizeof(Score));
  print_line(Score::min());
  print_line(Score::max());
  print_line(Score::zero());
}

TEST(multiply)
{
  PRINT_EXPR(Score::of_pawns(1.0) * 2);
  PRINT_EXPR(Score::of_pawns(1.0) / 2);
  PRINT_EXPR(Score::of_pawns(1.0) / 100);
  PRINT_EXPR(Score::of_pawns(1.0) * Score::of_pawns(1.0));
  PRINT_EXPR(Score::of_pawns(0.5) * Score::of_pawns(0.7));
  PRINT_EXPR(Score::of_pawns(0.5) + Score::of_pawns(0.7));
  PRINT_EXPR(Score::of_pawns(0.5) - Score::of_pawns(0.7));
  PRINT_EXPR(-Score::of_pawns(0.7));
}

TEST(compare)
{
  PRINT_EXPR(Score::of_pawns(1.0) < Score::of_pawns(0.5));
  PRINT_EXPR(Score::of_pawns(1.0) > Score::of_pawns(0.5));
  PRINT_EXPR(Score::of_moves_to_mate(10) > Score::of_moves_to_mate(12));
  PRINT_EXPR(Score::of_moves_to_mate(10) < Score::of_moves_to_mate(12));
  PRINT_EXPR(
    Score::of_moves_to_mate(10).neg() > Score::of_moves_to_mate(12).neg());
  PRINT_EXPR(
    Score::of_moves_to_mate(10).neg() < Score::of_moves_to_mate(12).neg());
}

TEST(xboard)
{
  PRINT_EXPR(Score::of_pawns(1.0).to_xboard());
  PRINT_EXPR(Score::of_moves_to_mate(10).to_xboard());
}

TEST(neg)
{
  PRINT_EXPR(Score::of_pawns(1.0).neg());
  PRINT_EXPR(Score::of_pawns(-1.0).neg());
}

TEST(neg_if)
{
  PRINT_EXPR(Score::of_pawns(1.0).neg_if(true));
  PRINT_EXPR(Score::of_pawns(1.0).neg_if(false));
  PRINT_EXPR(Score::of_pawns(-1.0).neg_if(true));
  PRINT_EXPR(Score::of_pawns(-1.0).neg_if(false));
}

TEST(inc_dec_mate_moves)
{
  auto run_test = [](Score score) {
    print_line(
      "orig:[$] inc:[$] dec:[$] inc2:[$]",
      score,
      score.inc_mate_moves(),
      score.dec_mate_moves(),
      score.inc_mate_moves(2));
  };
  run_test(Score::of_pawns(1.0));
  run_test(Score::of_moves_to_mate(1));
  run_test(Score::of_moves_to_mate(10));
  run_test(Score::of_moves_to_mate(1).neg());
  run_test(Score::of_moves_to_mate(10).neg());
}

} // namespace

} // namespace blackbit
