#pragma once

#include "board.hpp"
#include "eval.hpp"
#include "experiment_framework.hpp"
#include "generated_game_record.hpp"

namespace gr = generated_game_record;

namespace blackbit {

struct EngineParams {
  Experiment experiment;
  EvalParameters eval_params;
};

struct GameParams {
  std::string starting_fen;
  EngineParams white_params;
  EngineParams black_params;
  bee::Span time_per_move;
  const size_t hash_size;
  const int max_depth;
  const bool clear_cache_before_move;
};

struct SelfPlayResult {
  GameResult result;
  std::vector<gr::MoveInfo> moves;
  GameParams game_params;
  std::string final_fen;
};

SelfPlayResult self_play_one_game(const GameParams& game_params);

} // namespace blackbit
