#include "engine.hpp"

#include "blackbit/move.hpp"
#include "board.hpp"
#include "board_array.hpp"
#include "engine_core.hpp"
#include "eval.hpp"
#include "move_history.hpp"
#include "mpv_search.hpp"
#include "rules.hpp"
#include "transposition_table.hpp"

#include "bee/format_optional.hpp"
#include "bee/format_vector.hpp"
#include "bee/queue.hpp"
#include "bee/ref.hpp"
#include "bee/time.hpp"

#include <atomic>
#include <cmath>
#include <future>
#include <queue>
#include <set>
#include <variant>

using bee::Queue;
using bee::Span;
using bee::Time;
using std::atomic_bool;
using std::function;
using std::make_shared;
using std::make_unique;
using std::nullopt;
using std::optional;
using std::promise;
using std::shared_ptr;
using std::thread;
using std::unique_ptr;
using std::variant;
using std::vector;
using std::visit;

namespace blackbit {

////////////////////////////////////////////////////////////////////////////////
// Experiment flags
//

// auto shorten_on_check =
//   ExperimentFlag::register_flag("shorten_on_check", 1, 1, 2);
//

////////////////////////////////////////////////////////////////////////////////
// Constants
//

constexpr Score search_window = Score::of_milli_pawns(554);

////////////////////////////////////////////////////////////////////////////////
// Request
//

struct RequestSearch {
  shared_ptr<atomic_bool> should_stop;
  shared_ptr<promise<bee::OrError<SearchResultInfo::ptr>>> movep;

  unique_ptr<Board> board;
  int max_depth;
  function<void(SearchResultInfo::ptr&&)> on_update;
};

struct RequestMpvSearch {
  shared_ptr<atomic_bool> should_stop;
  shared_ptr<promise<bee::OrError<vector<SearchResultInfo::ptr>>>> result;

  unique_ptr<Board> board;
  int max_depth;
  int max_pvs;
  optional<int> num_workers;

  function<void(vector<SearchResultInfo::ptr>&&)> on_update;
};

struct RequestMpvSearchSP {
  shared_ptr<atomic_bool> should_stop;
  shared_ptr<promise<bee::OrError<vector<SearchResultInfo::ptr>>>> result;

  unique_ptr<Board> board;
  int max_depth;
  int max_pvs;

  function<void(vector<SearchResultInfo::ptr>&&)> on_update;
};

struct Request {
  template <class T> Request(T&& m) : msg(std::move(m)) {}

  variant<RequestSearch, RequestMpvSearch, RequestMpvSearchSP> msg;
};

namespace {
bee::OrError<SearchResultInfo::ptr> pv_search(
  const Board& board,
  int max_depth,
  const shared_ptr<TranspositionTable>& hash_table,
  const shared_ptr<MoveHistory>& move_history,
  const PCP::ptr& pcp,
  const shared_ptr<atomic_bool>& should_stop,
  const Experiment& experiment,
  const EvalParameters& eval_params,
  function<void(SearchResultInfo::ptr&&)>&& on_update)
{
  assert(max_depth >= 1);
  SearchResultInfo::ptr result = nullptr;

  auto start = Time::monotonic();

  uint64_t node_count = 0;

  auto make_result =
    [&](Span ellapsed, Move m, Score score, int depth, vector<Move>&& pv) {
      return SearchResultInfo::create(
        m, std::move(pv), score, node_count, depth, ellapsed);
    };

  auto core = EngineCore::create(
    board,
    hash_table,
    move_history,
    pcp,
    true,
    should_stop,
    experiment,
    eval_params);

  for (int d = 1; d <= max_depth; ++d) {
    auto search_once = [&](const Score lower_bound, const Score upper_bound) {
      return core->search_one_depth(d, lower_bound, upper_bound);
    };

    auto do_search = [&]() -> bee::OrError<optional<SearchResultOneDepth>> {
      Score lower_bound = Score::min();
      Score upper_bound = Score::max();
      if (result != nullptr) {
        if (result->eval.is_mate()) {
          lower_bound = result->eval.dec_mate_moves(2);
          upper_bound = result->eval.inc_mate_moves(2);
        } else {
          lower_bound = result->eval - search_window;
          upper_bound = result->eval + search_window;
        }
      }
      bail(r, search_once(lower_bound, upper_bound));
      if (!r.has_value()) { return nullopt; }
      if ((r->score() > lower_bound && r->score() < upper_bound)) {
        return std::move(r);
      } else {
        return search_once(Score::min(), Score::max());
      }
    };

    bail(r_opt, do_search());
    if (!r_opt.has_value()) { break; }
    auto& r = *r_opt;
    auto m = r.move();
    if (!m.has_value()) {
      return bee::Error("Engine returned result without move");
    }

    node_count += r.nodes();
    auto checkpoint = Time::monotonic();
    Span ellapsed = checkpoint.diff(start);
    result = make_result(ellapsed, *m, r.score(), d, std::move(r.pv()));
    if (on_update != nullptr) {
      auto r = result->clone();
      r->flip(board.turn);
      on_update(std::move(r));
    }
    if (should_stop->load()) { break; }
  }

  if (result == nullptr) { return bee::Error("Failed to find a move"); }
  result->flip(board.turn);

  return result;
}

bee::OrError<vector<SearchResultInfo::ptr>> mpv_search_sp(
  const Board& board,
  int max_depth,
  int max_pvs,
  const shared_ptr<TranspositionTable>& hash_table,
  const shared_ptr<MoveHistory>& move_history,
  const PCP::ptr& pcp,
  const shared_ptr<atomic_bool>& should_stop,
  const Experiment& experiment,
  const EvalParameters& eval_params,
  function<void(vector<SearchResultInfo::ptr>&&)>&& on_update)
{
  vector<SearchResultInfo::ptr> results;

  auto start = Time::monotonic();

  uint64_t node_count = 0;

  auto make_result =
    [&](Span ellapsed, Move m, Score score, int depth, vector<Move>&& pv) {
      return SearchResultInfo::create(
        m, std::move(pv), score, node_count, depth, ellapsed);
    };

  auto core = EngineCore::create(
    board,
    hash_table,
    move_history,
    pcp,
    false,
    should_stop,
    experiment,
    eval_params);

  for (int d = 1; d <= max_depth; ++d) {
    auto search_once = [&](Score lower_bound, Score upper_bound)
      -> bee::OrError<optional<SearchResultOneDepthMPV>> {
      return core->search_one_depth_mpv(d, max_pvs, lower_bound, upper_bound);
    };

    auto do_search = [&]() -> bee::OrError<optional<SearchResultOneDepthMPV>> {
      Score lower_bound = Score::min();
      Score upper_bound = Score::max();
      return search_once(lower_bound, upper_bound);
    };

    bail(r_opt, do_search());
    if (!r_opt.has_value()) { break; }
    auto& r = *r_opt;

    node_count += r.nodes();
    auto checkpoint = Time::monotonic();
    Span ellapsed = checkpoint.diff(start);
    results.clear();
    for (auto& res : r.results) {
      auto m = res.move();
      if (!m.has_value()) {
        return bee::Error("Engine returned result without moves");
      }
      results.push_back(
        make_result(ellapsed, *m, res.score(), d, std::move(res.pv())));
    }
    if (on_update != nullptr) {
      vector<SearchResultInfo::ptr> clone;
      for (auto& r : results) {
        auto x = r->clone();
        x->flip(board.turn);
        clone.push_back(std::move(x));
      }
      on_update(std::move(clone));
    }
    if (should_stop->load()) { break; }
  }

  for (auto& r : results) { r->flip(board.turn); }

  return results;
}

void run_background_engine(
  Queue<Request>* queue,
  const Experiment& experiment,
  EvalParameters eval_params,
  const PCP::ptr& pcp,
  size_t cache_size,
  bool clear_cache_before_move)
{
  auto _hash_table = make_shared<TranspositionTable>(cache_size);
  auto _move_history = make_shared<MoveHistory>();

  while (true) {
    auto r_opt = queue->pop();
    if (!r_opt.has_value()) break;

    visit(
      [&]<class T>(T& msg) {
        if (clear_cache_before_move) {
          _hash_table->clear();
          _move_history->clear();
        }
        if constexpr (std::is_same_v<T, RequestSearch>) {
          msg.movep->set_value(pv_search(
            *msg.board,
            msg.max_depth,
            _hash_table,
            _move_history,
            pcp,
            msg.should_stop,
            experiment,
            eval_params,
            std::move(msg.on_update)));
        } else if constexpr (std::is_same_v<T, RequestMpvSearch>) {
          msg.result->set_value(MpvSearch::search(
            std::move(msg.board),
            msg.max_depth,
            msg.max_pvs,
            msg.num_workers,
            _hash_table,
            _move_history,
            msg.should_stop,
            experiment,
            eval_params,
            std::move(msg.on_update)));
        } else if constexpr (std::is_same_v<T, RequestMpvSearchSP>) {
          msg.result->set_value(mpv_search_sp(
            *msg.board,
            msg.max_depth,
            msg.max_pvs,
            _hash_table,
            _move_history,
            pcp,
            msg.should_stop,
            experiment,
            eval_params,
            std::move(msg.on_update)));
        }
      },
      r_opt->msg);
  }
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
// Engine
//

Engine::Engine(
  const Experiment& experiment,
  const EvalParameters& eval_params,
  const PCP::ptr& pcp,
  size_t cache_size,
  bool clear_cache_before_move)
    : _queue(make_shared<Queue<Request>>()), _experiment(experiment)
{
  Queue<Request>* q = &*_queue;
  _worker = thread(
    [q, experiment, eval_params, pcp, cache_size, clear_cache_before_move]() {
      run_background_engine(
        q, experiment, eval_params, pcp, cache_size, clear_cache_before_move);
    });
}

Engine::~Engine()
{
  if (_stop_current_computation != nullptr) { _stop_current_computation(); }
  _queue->close();
  _worker.join();
}

Engine::ptr Engine::create(
  const Experiment& experiment,
  const EvalParameters& eval_params,
  const PCP::ptr& pcp,
  size_t cache_size,
  bool clear_cache_before_move)
{
  auto engine = unique_ptr<Engine>(new Engine(
    experiment, eval_params, pcp, cache_size, clear_cache_before_move));

  return engine;
}

bee::OrError<SearchResultInfo::ptr> Engine::find_best_move(
  const Board& board,
  int depth,
  optional<Span> max_time,
  function<void(SearchResultInfo::ptr&&)>&& on_update)
{
  auto movef = start_search(board, depth, std::move(on_update));
  return movef->wait_at_most(max_time);
}

bee::OrError<vector<SearchResultInfo::ptr>> Engine::find_best_moves_mpv_sp(
  const Board& board,
  const int max_depth,
  const int max_pvs,
  std::optional<Span> max_time,
  std::function<void(std::vector<SearchResultInfo::ptr>&&)>&& on_update)
{
  auto movef =
    start_mpv_search_sp(board, max_depth, max_pvs, std::move(on_update));
  return movef->wait_at_most(max_time);
}

bee::OrError<vector<SearchResultInfo::ptr>> Engine::find_best_moves_mpv(
  const Board& board,
  const int max_depth,
  const int max_pvs,
  const std::optional<int>& num_workers,
  std::optional<Span> max_time,
  std::function<void(std::vector<SearchResultInfo::ptr>&&)>&& on_update)
{
  auto movef = start_mpv_search(
    board, max_depth, max_pvs, num_workers, std::move(on_update));
  return movef->wait_at_most(max_time);
}

FutureResult<SearchResultInfo::ptr>::ptr Engine::start_search(
  const Board& board,
  int max_depth,
  std::function<void(SearchResultInfo::ptr&&)>&& on_update)
{
  if (_stop_current_computation != nullptr) { _stop_current_computation(); }
  auto should_stop = make_shared<atomic_bool>(false);
  auto movep = make_shared<promise<bee::OrError<SearchResultInfo::ptr>>>();
  auto future = make_shared<FutureResult<SearchResultInfo::ptr>>(
    should_stop, movep->get_future());

  _stop_current_computation =
    FutureResult<SearchResultInfo::ptr>::stop_and_forget_fn(future);

  _queue->push(RequestSearch{
    .should_stop = should_stop,
    .movep = movep,
    .board = make_unique<Board>(board),
    .max_depth = max_depth,
    .on_update = std::move(on_update),
  });

  return future;
}

FutureResult<vector<SearchResultInfo::ptr>>::ptr Engine::start_mpv_search(
  const Board& board,
  const int max_depth,
  const int max_pvs,
  optional<int> num_workers,
  function<void(vector<SearchResultInfo::ptr>&&)>&& on_update)
{
  if (_stop_current_computation != nullptr) { _stop_current_computation(); }
  auto should_stop = make_shared<atomic_bool>(false);
  auto result =
    make_shared<promise<bee::OrError<vector<SearchResultInfo::ptr>>>>();
  auto future = make_shared<FutureResult<vector<SearchResultInfo::ptr>>>(
    should_stop, result->get_future());

  _stop_current_computation =
    FutureResult<vector<SearchResultInfo::ptr>>::stop_and_forget_fn(future);

  _queue->push(RequestMpvSearch{
    .should_stop = should_stop,
    .result = result,
    .board = make_unique<Board>(board),
    .max_depth = max_depth,
    .max_pvs = max_pvs,
    .num_workers = num_workers,
    .on_update = std::move(on_update),
  });

  return future;
}

FutureResult<vector<SearchResultInfo::ptr>>::ptr Engine::start_mpv_search_sp(
  const Board& board,
  const int max_depth,
  const int max_pvs,
  function<void(vector<SearchResultInfo::ptr>&&)>&& on_update)
{
  if (_stop_current_computation != nullptr) { _stop_current_computation(); }
  auto should_stop = make_shared<atomic_bool>(false);
  auto result =
    make_shared<promise<bee::OrError<vector<SearchResultInfo::ptr>>>>();
  auto future = make_shared<FutureResult<vector<SearchResultInfo::ptr>>>(
    should_stop, result->get_future());

  _stop_current_computation =
    FutureResult<vector<SearchResultInfo::ptr>>::stop_and_forget_fn(future);

  _queue->push(RequestMpvSearchSP{
    .should_stop = should_stop,
    .result = result,
    .board = make_unique<Board>(board),
    .max_depth = max_depth,
    .max_pvs = max_pvs,
    .on_update = std::move(on_update),
  });

  return future;
}

////////////////////////////////////////////////////////////////////////////////
// EngineInProcess
//

EngineInProcess::EngineInProcess(
  const Experiment& experiment,
  const EvalParameters& eval_params,
  const PCP::ptr& pcp,
  size_t cache_size,
  bool clear_cache_before_move)
    : _experiment(experiment),
      _eval_params(eval_params),
      _hash_table(make_shared<TranspositionTable>(cache_size)),
      _move_history(make_shared<MoveHistory>()),
      _pcp(pcp),
      _clear_cache_before_move(clear_cache_before_move)
{}

EngineInProcess::~EngineInProcess() {}

EngineInProcess::ptr EngineInProcess::create(
  const Experiment& experiment,
  const EvalParameters& eval_params,
  const PCP::ptr& pcp,
  size_t cache_size,
  bool clear_cache_before_move)
{
  return unique_ptr<EngineInProcess>(new EngineInProcess(
    experiment, eval_params, pcp, cache_size, clear_cache_before_move));
}

void EngineInProcess::set_eval_params(EvalParameters&& eval_params)
{
  _eval_params = std::move(eval_params);

  _hash_table->clear();
  // _move_history->clear();
}

bee::OrError<SearchResultInfo::ptr> EngineInProcess::find_best_move(
  const Board& board,
  int max_depth,
  optional<Span> max_time,
  function<void(SearchResultInfo::ptr&&)>&& on_update)
{
  auto should_stop = make_shared<atomic_bool>(false);

  if (max_time.has_value()) {
    _alarms.add_alarm(*max_time, [=]() { should_stop->store(true); });
  };

  if (_clear_cache_before_move) {
    _hash_table->clear();
    // _move_history->clear();
  }

  return pv_search(
    board,
    max_depth,
    _hash_table,
    _move_history,
    _pcp,
    should_stop,
    _experiment,
    _eval_params,
    std::move(on_update));
}

} // namespace blackbit
