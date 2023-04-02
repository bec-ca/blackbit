#include "self_play.hpp"

#include "bee/format_vector.hpp"
#include "bee/span.hpp"
#include "engine.hpp"
#include "experiment_framework.hpp"
#include "generated_game_record.hpp"
#include "rules.hpp"

#include <memory>

using bee::print_line;
using bee::Span;
using std::make_unique;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;

namespace blackbit {
namespace {

struct BotState {
  using ptr = shared_ptr<BotState>;

  static ptr create(
    size_t cache_size,
    Span time_per_move,
    const EngineParams& params,
    int max_depth,
    bool clear_cache_before_move)
  {
    return ptr(new BotState(
      cache_size, time_per_move, params, max_depth, clear_cache_before_move));
  }

  void set_fen(const string& fen) { _board.set_fen(fen); }

  bee::OrError<SearchResultInfo::ptr> move()
  {
    bail(
      result,
      _engine->find_best_move(_board, _max_depth, _time_per_move, nullptr));
    _board.move(result->best_move);
    return std::move(result);
  }

  void user_move(const Move& m) { _board.move(m); }

 private:
  BotState(
    size_t cache_size,
    Span time_per_move,
    const EngineParams& params,
    int max_depth,
    bool clear_cache_before_move)
      : _engine(EngineInProcess::create(
          params.experiment,
          params.eval_params,
          nullptr,
          cache_size,
          clear_cache_before_move)),
        _time_per_move(time_per_move),
        _max_depth(max_depth)
  {}

  unique_ptr<EngineInProcess> _engine;
  Board _board;

  const Span _time_per_move;
  const int _max_depth;
};

struct BotPair {
  BotState::ptr white_bot;
  BotState::ptr black_bot;

  void flip() { swap(white_bot, black_bot); }

  BotState::ptr& get_bot(Color turn)
  {
    switch (turn) {
    case Color::White:
      return white_bot;
    case Color::Black:
      return black_bot;
    case Color::None:
      assert(false);
    }
    assert(false);
  }

  BotState::ptr& playing_bot(Color turn) { return get_bot(turn); }

  BotState::ptr& waiting_bot(Color turn) { return get_bot(oponent(turn)); }

  void set_fen(const string& fen)
  {
    white_bot->set_fen(fen);
    black_bot->set_fen(fen);
  }
};

} // namespace

SelfPlayResult self_play_one_game(const GameParams& game_params)
{
  auto white_bot = BotState::create(
    game_params.hash_size,
    game_params.time_per_move,
    game_params.white_params,
    game_params.max_depth,
    game_params.clear_cache_before_move);
  auto black_bot = BotState::create(
    game_params.hash_size,
    game_params.time_per_move,
    game_params.black_params,
    game_params.max_depth,
    game_params.clear_cache_before_move);

  auto bot_pair = BotPair{.white_bot = white_bot, .black_bot = black_bot};
  auto board = make_unique<Board>();
  board->set_fen(game_params.starting_fen);
  bot_pair.set_fen(game_params.starting_fen);

  vector<gr::MoveInfo> moves;

  int ply = 0;
  auto result = [&]() {
    while (true) {
      auto final_result = Rules::result(*board, Rules::make_scratch(*board));
      if (final_result != GameResult::NotFinished) { return final_result; }

      ply++;
      auto playing_bot = bot_pair.playing_bot(board->turn);
      auto waiting_bot = bot_pair.waiting_bot(board->turn);
      auto result_or_error = playing_bot->move();
      if (result_or_error.is_error()) {
        vector<Move> just_the_moves;
        for (const auto& m : moves) { just_the_moves.push_back(m.move); }
        print_line(
          "Engine was unable to find a move on ply $, current fen $\n played "
          "moves: "
          "$\n$",
          ply,
          board->to_fen(),
          just_the_moves,
          result_or_error.error());
        print_line(board->to_string());
        return GameResult::NotFinished;
      }
      auto result = std::move(result_or_error.value());
      moves.push_back(gr::MoveInfo{
        .move = result->best_move,
        .pv = result->pv,
        .evaluation = result->eval,
        .depth = result->depth,
        .nodes = result->nodes,
        .think_time = result->think_time,
      });
      if (!Rules::is_legal_move(
            *board, Rules::make_scratch(*board), result->best_move)) {
        print_line("Engine proposed an invalid move: $", result->best_move);
        print_line("$", board->to_string());
        return GameResult::NotFinished;
      }
      board->move(result->best_move);
      waiting_bot->user_move(result->best_move);
    }
  }();

  return SelfPlayResult{
    .result = result,
    .moves = moves,
    .game_params = game_params,
    .final_fen = board->to_fen(),
  };
}

} // namespace blackbit
