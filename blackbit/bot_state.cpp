#include "bot_state.hpp"

#include "communication.hpp"
#include "engine.hpp"
#include "game_result.hpp"
#include "pcp.hpp"
#include "rules.hpp"
#include "search_result_info.hpp"

#include "bee/format_vector.hpp"
#include "bee/span.hpp"

#include <string>

using bee::Span;
using std::shared_ptr;
using std::string;

namespace blackbit {
namespace {

void send_result(
  XboardWriter& writer, const SearchResultInfo& result, const Board& board)
{
  writer.send(
    "$ $ $ $\t$",
    result.depth,
    result.eval.to_xboard(),
    result.think_time.to_millis() / 10,
    result.nodes,
    result.make_pretty_moves(board));
}

struct BotStateImpl : public BotState,
                      public std::enable_shared_from_this<BotStateImpl> {
 public:
  BotStateImpl(
    const XboardWriter::ptr& writer,
    const Experiment& experiment,
    const EvalParameters& eval_params,
    bool use_mpv,
    int cache_size,
    PCP::ptr&& pcp)
      : _engine(Engine::create(
          experiment, eval_params, nullptr, 1ull << cache_size, false)),
        _experiment(experiment),
        _logger(writer->logger()),
        _writer(writer),
        _use_mpv(use_mpv),
        _pcp(std::move(pcp))
  {
    _board.set_initial();
  }

  ~BotStateImpl() { assert(_torn_down); }

  virtual void tear_down() override
  {
    assert(!_torn_down);
    _stop_current_search();
    if (_post_thread.has_value()) { _post_thread->join(); }
    _torn_down = true;
  }

  virtual void set_ponder(bool ponder) override
  {
    _ponder = ponder;
    _maybe_restart_search();
  }

  virtual void set_post(bool post) override
  {
    _post = post;
    _maybe_restart_search();
  }

  virtual void reset() override
  {
    _board.set_initial();
    _ponder = false;
    _post = false;
    _maybe_restart_search();
  }

  virtual void set_max_depth(int max_depth) override
  {
    _max_depth = max_depth;
    _maybe_restart_search();
  }

  virtual void set_fen(const string& fen) override
  {
    _board.set_fen(fen);
    _moves.clear();
    _maybe_restart_search();
  }

  virtual string get_fen() const override { return _board.to_fen(); }

  virtual void set_time_control(int mps, Span base, Span inc) override
  {
    _mps = mps;
    _base = base;
    _inc = inc;
  }

  virtual void set_max_time(Span max_time) override { _max_time = max_time; }

  virtual void set_time_remaining(Span time_remaining) override
  {
    _time_remaining = time_remaining;
  }

  virtual Span get_time_remaining() const override { return _time_remaining; }

  Span _get_think_time() const
  {
    const Span absolute_minimum_time_to_think = Span::of_millis(50);
    const Span time_buffer = Span::of_millis(10);
    Span think_time = Span::of_seconds(1);
    if (_mps > 0 && _inc == Span::zero()) {
      think_time = _base / _mps;
    } else if (_mps == 0 && _base > Span::zero()) {
      think_time =
        std::max(std::min(_time_remaining, _inc), (_time_remaining / 40));
    } else if (_max_time > Span::zero()) {
      think_time = _max_time;
    }
    if ((_time_remaining > Span::zero()) && think_time > _time_remaining) {
      think_time = _time_remaining;
    }
    think_time =
      std::max(absolute_minimum_time_to_think, think_time - time_buffer);
    assert(think_time > Span::zero() && "Why?");
    return think_time;
  }

  auto make_post_callback()
  {
    return [post = _post,
            writer = _writer,
            board = _board,
            last_result =
              shared_ptr<SearchResultInfo>(nullptr)](auto&& result) mutable {
      if (post) {
        if (last_result == nullptr || *result != *last_result) {
          send_result(*writer, *result, board);
          last_result = result->clone();
        }
      }
    };
  }

  auto make_mpv_post_callback()
  {
    return [cb = make_post_callback()](auto&& results) mutable {
      if (!results.empty()) { cb(results[0]); }
    };
  }

  bee::OrError<Move> _find_best_move(Span think_time)
  {
    if (_pcp != nullptr) {
      auto entry = _pcp->lookup(_board.to_fen());
      if (!entry.is_error() && entry->has_value()) {
        auto& e = **entry;
        send_result(*_writer, *e, _board);
        return e->best_move;
      }
    }
    if (_use_mpv) {
      bail(
        result,
        _engine->find_best_moves_mpv(
          _board, _max_depth, 1, 16, think_time, make_mpv_post_callback()));
      return result[0]->best_move;
    } else {
      auto weak_ptr = weak_from_this();
      _current_search_id++;
      bail(
        res,
        _engine->find_best_move(
          _board, _max_depth, think_time, make_post_callback()));
      return res->best_move;
    }
  }

  virtual bee::OrError<Move> move() override
  {
    Span think_time = _get_think_time();

    _logger->log_line("Going to think for $", think_time.to_string());
    auto m_or_error = _find_best_move(think_time);
    bail(m, std::move(m_or_error));
    if (!Rules::is_legal_move(_board, Rules::make_scratch(_board), m)) {
      bee::print_line("Got invalid move: $\n$", m, _board.to_string());
      return bee::Error::format("Engine returned an invalid move: $", m);
    }
    auto mi = _board.move(m);
    _moves.emplace_back(m, mi);
    _logger->log_line("$", _board.to_fen());
    _maybe_restart_search();
    return m;
  }

  virtual Color get_turn() const override { return _board.turn; }

  virtual void print_board() const override
  {
    bee::print_line(_board.to_string());
  }

  virtual bool get_ponder() const override { return _ponder; }

  virtual bee::OrError<bee::Unit> user_move(const Move& m) override
  {
    if (!Rules::is_legal_move(_board, Rules::make_scratch(_board), m)) {
      return bee::Error::format("Illegal move $", m);
    } else {
      auto mi = _board.move(m);
      _moves.emplace_back(m, mi);
      _logger->log_line("$", _board.to_fen());
    }
    _maybe_restart_search();
    return bee::unit;
  }

  virtual bee::OrError<bee::Unit> user_move(const string& move_str) override
  {
    bail(
      m,
      _board.parse_xboard_move_string(move_str),
      "Parsing move: '$'",
      move_str);
    return user_move(m);
  }

  virtual bool is_over() const override
  {
    return Rules::result(_board, Rules::make_scratch(_board)) !=
           GameResult::NotFinished;
  }

  void _stop_current_search()
  {
    if (_move_future != nullptr) {
      _logger->log_line("Stopping search");
      _move_future->stop_and_wait();
      _move_future = nullptr;
    }
  }

  void _maybe_restart_search()
  {
    _stop_current_search();
    if (_ponder) {
      _logger->log_line("Starting search");
      _move_future = _engine->start_search(_board, 50, make_post_callback());
    }
  }

  virtual void undo() override
  {
    if (_moves.empty()) return;
    auto back = _moves.back();
    _board.undo(back.first, back.second);
    _moves.pop_back();
    _maybe_restart_search();
  }

 private:
  Board _board;
  Engine::ptr _engine;
  FutureResult<SearchResultInfo::ptr>::ptr _move_future;
  bool _ponder = false;
  bool _post = false;
  int _max_depth = 50;

  Span _time_remaining = Span::zero();

  int _mps = 0;
  Span _base = Span::zero();
  Span _inc = Span::zero();

  Span _max_time = Span::zero();

  Experiment _experiment;

  std::vector<std::pair<Move, MoveInfo>> _moves;

  Logger::ptr _logger;
  XboardWriter::ptr _writer;

  std::optional<std::thread> _post_thread;

  bool _use_mpv;

  bool _torn_down = false;

  int _current_search_id = 0;

  PCP::ptr _pcp;
};

} // namespace

BotState::~BotState() {}

shared_ptr<BotState> BotState::create(
  const XboardWriter::ptr& writer,
  const Experiment& experiment,
  const EvalParameters& eval_params,
  bool use_mpv,
  int cache_size,
  PCP::ptr&& pcp)
{
  return make_shared<BotStateImpl>(
    writer, experiment, eval_params, use_mpv, cache_size, std::move(pcp));
}

} // namespace blackbit
