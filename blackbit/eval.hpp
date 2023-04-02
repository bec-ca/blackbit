#pragma once

#include "board.hpp"
#include "eval_scratch.hpp"
#include "score.hpp"

#include <functional>

namespace blackbit {

struct PlayerFeatures {
  Score current_eval;

  Score material_points;

  Score attack_points;

  Score mobility_points;

  Score pawn_points;

  Score rooks_on_open_file_points;

  Score bishop_pair_points;

  // king safety features
  Score king_safe_from_queen_points;
  Score king_rough_safe_from_queen_points;
  Score king_rough_safe_from_queen_with_pawns_points;
  Score king_is_being_attacked_points;

  Score king_threat_from_pieces;
};

using Features = PlayerPair<PlayerFeatures>;

struct EvalParameters {
  static EvalParameters default_params();
  static EvalParameters default_test_params();

  std::function<Score(const Features&, const Board&)> custom_eval;
};

struct Multipliers {
  static double attack_multiplier();
  static double king_safety_from_queen_score();
  static double king_rough_safety_from_queen_score();
  static double king_rough_safety_from_queen_with_pawns_score();
  static double mobility_multiplier();
};

struct Evaluator {
  static Score eval_for_white(
    const Board& board,
    const EvalScratch& scratch,
    const Experiment& experiment,
    const EvalParameters& params);

  static Score eval_for_current_player(
    const Board& board,
    const EvalScratch& scratch,
    const Experiment& experiment,
    const EvalParameters& params);

  static Features features(
    const Board& board,
    const EvalScratch& scratch,
    const Experiment& experiment);

  // for test stuff
  static Score eval_king_safety(
    const Board& board,
    const EvalScratch& scratch,
    Color color,
    const Experiment& experiment);

  static Score eval_pawns(
    const Board& board, Color color, const Experiment& experiment);

  static Score eval_rooks_on_open_file(
    const Board& board, Color color, const Experiment& experiment);
};

} // namespace blackbit
