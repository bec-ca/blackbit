#include "self_play_async.hpp"

#include "async/async.hpp"
#include "bee/format_vector.hpp"
#include "blackbit/game_result.hpp"
#include "blackbit/pieces.hpp"
#include "generated_game_record.hpp"
#include "rules.hpp"

#include <memory>

using namespace async;

using bee::print_err_line;
using bee::Span;
using std::make_unique;
using std::nullopt;
using std::optional;
using std::string;
using std::unique_ptr;
using std::vector;

namespace blackbit {
namespace {

struct GameRunner : public std::enable_shared_from_this<GameRunner> {
 public:
  GameRunner(
    const string& fen,
    const Span time_per_move,
    EngineInterface::ptr&& white_engine,
    EngineInterface::ptr&& black_engine)
      : _white_engine(std::move(white_engine)),
        _black_engine(std::move(black_engine)),
        _fen(fen),
        _time_per_move(time_per_move),
        _board(make_unique<Board>())
  {}

  ~GameRunner()
  {
    if (!_closed) {
      print_err_line(
        "GameRunner destructing before closing, closed:$, closing:$",
        _closed,
        _closing);
      assert(false);
    }
  }

  Task<bee::OrError<SelfPlayResultAsync>> start()
  {
    auto ret = co_await [&]() -> Task<bee::OrError<SelfPlayResultAsync>> {
      _board->set_fen(_fen);
      co_bail_unit(_white_engine->set_fen(_fen));
      co_bail_unit(_black_engine->set_fen(_fen));
      co_bail_unit(_white_engine->set_time_per_move(_time_per_move));
      co_bail_unit(_black_engine->set_time_per_move(_time_per_move));
      while (true) {
        auto m = co_await get_playing_engine()->find_move();
        auto ret = _handle_move(std::move(m));
        if (ret.has_value()) { co_return std::move(*ret); }
      }
    }();
    co_await _close();
    co_return ret;
  }

 private:
  Task<bee::Unit> _close()
  {
    _closing = true;
    co_await _white_engine->close();
    co_await _black_engine->close();

    _white_engine = nullptr;
    _black_engine = nullptr;
    _closed = true;

    co_return bee::unit;
  }

  const EngineInterface::ptr& get_playing_engine() const
  {
    if (_board->turn == Color::White) {
      return _white_engine;
    } else {
      return _black_engine;
    }
  }

  optional<bee::OrError<SelfPlayResultAsync>> _handle_move(
    bee::OrError<Move>&& move_or_error)
  {
    if (move_or_error.is_error()) {
      print_err_line("Engine failed: $", move_or_error.error());
      return create_result(
        game_result_from_winner(oponent(_board->turn)),
        GameEndReason::EngineFailed);
    }
    bail(m, move_or_error);
    _board->move(m);
    _moves.push_back({.move = m});
    get_playing_engine()->send_move(m);
    auto result = Rules::result(*_board, Rules::make_scratch(*_board));
    if (result != GameResult::NotFinished) {
      return create_result(result, GameEndReason::EndedNormally);
    } else {
      return nullopt;
    }
  };

  SelfPlayResultAsync create_result(GameResult result, GameEndReason end_reason)
  {
    return SelfPlayResultAsync{
      .result = result,
      .end_reason = end_reason,
      .moves = std::move(_moves),
      .final_fen = _board->to_fen(),
    };
  }

  EngineInterface::ptr _white_engine;
  EngineInterface::ptr _black_engine;

  string _fen;
  Span _time_per_move;
  unique_ptr<Board> _board;

  vector<gr::MoveInfo> _moves;
  bool _closed = false;
  bool _closing = false;
};

} // namespace

////////////////////////////////////////////////////////////////////////////////
// self_play_one_game
//

Task<bee::OrError<SelfPlayResultAsync>> self_play_one_game(
  const string starting_fen,
  const Span time_per_move,
  const EngineFactory white_factory,
  const EngineFactory black_factory)
{
  co_bail(white_engine, white_factory());
  co_bail(black_engine, black_factory());
  auto game_runner = make_shared<GameRunner>(
    starting_fen,
    time_per_move,
    std::move(white_engine),
    std::move(black_engine));
  co_return co_await game_runner->start();
}

} // namespace blackbit
