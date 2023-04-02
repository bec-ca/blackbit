#pragma once

#include "blackbit/eval_scratch.hpp"
#include "board.hpp"
#include "game_result.hpp"

namespace blackbit {

struct Rules {
  static void list_moves(
    const Board& board, const EvalScratch& scratch, MoveVector& moves);

  static void list_takes(const Board& board, MoveVector& moves);

  static bool is_legal_move(
    const Board& board, const EvalScratch& scratch, const Move& m);

  static bool is_king_under_attack(
    const Board& board, const EvalScratch& scratch, Color color);

  static bool is_check(const Board& board, const EvalScratch& scratch);

  static bool is_mate(const Board& board, const EvalScratch& scratch);

  static bool is_draw_without_stalemate(const Board& board);

  static GameResult result(const Board& board, const EvalScratch& scratch);

  static GameResult result_slow(const Board& board);

  static BitBoard attacks_bb(const Board& board, Color color);

  static std::string pretty_move(const Board& board, Move m);

  static bee::OrError<Move> parse_pretty_move(
    const Board& board, const std::string& m);

  static EvalScratch make_scratch(const Board& b);

  static bool is_game_over_slow(const Board& b);
};

} // namespace blackbit
