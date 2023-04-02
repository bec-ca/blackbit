#pragma once

#include "engine_interface.hpp"
#include "game_result.hpp"
#include "generated_game_record.hpp"
#include "move.hpp"

#include "async/task.hpp"

namespace blackbit {

namespace gr = generated_game_record;

enum class GameEndReason {
  EndedNormally,
  EngineFailed,
};

struct SelfPlayResultAsync {
  GameResult result;
  GameEndReason end_reason;
  std::vector<gr::MoveInfo> moves;
  std::string final_fen;
};

using EngineFactory = std::function<bee::OrError<EngineInterface::ptr>()>;

async::Task<bee::OrError<SelfPlayResultAsync>> self_play_one_game(
  const std::string starting_fen,
  const bee::Span time_per_move,
  const EngineFactory white_bot,
  const EngineFactory black_bot);

} // namespace blackbit
