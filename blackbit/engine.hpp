#pragma once

#include "board.hpp"
#include "engine_core.hpp"
#include "eval.hpp"
#include "experiment_framework.hpp"
#include "move.hpp"
#include "move_history.hpp"
#include "transposition_table.hpp"

#include "bee/alarms.hpp"
#include "bee/queue.hpp"
#include "bee/span.hpp"

#include <atomic>
#include <future>
#include <memory>

namespace blackbit {

struct MultiPVSearchResultInfo {
 public:
  using ptr = std::unique_ptr<MultiPVSearchResultInfo>;

  SearchResultInfo::ptr result;
  bool is_upper_bound;
  ptr clone() const;
};

////////////////////////////////////////////////////////////////////////////////
// FutureResult
//

template <class T> struct FutureResult {
  using ptr = std::shared_ptr<FutureResult>;

  FutureResult(
    std::shared_ptr<std::atomic_bool> should_stop,
    std::future<bee::OrError<T>> result_fut);

  bee::OrError<T> result_now();
  bee::OrError<T> wait();

  bee::OrError<T> wait_at_most(std::optional<bee::Span> span);

  void stop_and_wait()
  {
    stop_and_forget();
    if (!_result_fut.valid()) { return; }
    _result_fut.get();
    return;
  }

  void stop_and_forget() { _should_stop->store(true); }

  std::function<void()> static stop_and_forget_fn(const ptr& self)
  {
    return [self]() { self->stop_and_wait(); };
  }

 private:
  std::shared_ptr<std::atomic_bool> _should_stop;
  std::future<bee::OrError<T>> _result_fut;
};

template <class T>
FutureResult<T>::FutureResult(
  std::shared_ptr<std::atomic_bool> should_stop,
  std::future<bee::OrError<T>> result_fut)
    : _should_stop(std::move(should_stop)), _result_fut(std::move(result_fut))
{}

template <class T> bee::OrError<T> FutureResult<T>::result_now()
{
  return wait_at_most(bee::Span::zero());
}

template <class T> bee::OrError<T> FutureResult<T>::wait()
{
  return wait_at_most(std::nullopt);
}

template <class T>
bee::OrError<T> FutureResult<T>::wait_at_most(std::optional<bee::Span> span)
{
  assert(_result_fut.valid());

  if (span.has_value()) {
    auto status = _result_fut.wait_for(span->to_chrono());
    if (status == std::future_status::timeout) {
      stop_and_forget();
    } else if (status == std::future_status::ready) {
      // nothing to do here
    } else {
      assert(false && "this should not happen");
    }
  }
  return std::move(_result_fut.get());
}

////////////////////////////////////////////////////////////////////////////////
// Engine
//

struct Request;

struct Engine {
 public:
  using ptr = std::unique_ptr<Engine>;

  bee::OrError<SearchResultInfo::ptr> find_best_move(
    const Board& board,
    int depth,
    std::optional<bee::Span> max_time,
    std::function<void(SearchResultInfo::ptr&&)>&& on_update);

  bee::OrError<std::vector<SearchResultInfo::ptr>> find_best_moves_mpv(
    const Board& board,
    const int max_depth,
    const int max_pvs,
    const std::optional<int>& num_workers,
    std::optional<bee::Span> max_time,
    std::function<void(std::vector<SearchResultInfo::ptr>&&)>&& on_update);

  bee::OrError<std::vector<SearchResultInfo::ptr>> find_best_moves_mpv_sp(
    const Board& board,
    const int max_depth,
    const int max_pvs,
    std::optional<bee::Span> max_time,
    std::function<void(std::vector<SearchResultInfo::ptr>&&)>&& on_update);

  FutureResult<SearchResultInfo::ptr>::ptr start_search(
    const Board& board,
    int max_depth,
    std::function<void(SearchResultInfo::ptr&&)>&& on_update);

  FutureResult<std::vector<SearchResultInfo::ptr>>::ptr start_mpv_search(
    const Board& board,
    const int max_depth,
    const int max_pvs,
    const std::optional<int> num_workers,
    std::function<void(std::vector<SearchResultInfo::ptr>&&)>&& on_update);

  FutureResult<std::vector<SearchResultInfo::ptr>>::ptr start_mpv_search_sp(
    const Board& board,
    const int max_depth,
    const int max_pvs,
    std::function<void(std::vector<SearchResultInfo::ptr>&&)>&& on_update);

  static ptr create(
    const Experiment& experiment,
    const EvalParameters& eval_params,
    const PCP::ptr& pcp,
    size_t cache_size,
    bool clear_cache_before_move);

  ~Engine();

 private:
  Engine(
    const Experiment& experiment,
    const EvalParameters& eval_params,
    const PCP::ptr& pcp,
    size_t cache_size,
    bool clear_cache_before_move);

  std::function<void()> _stop_current_computation;

  std::shared_ptr<bee::Queue<Request>> _queue;

  std::thread _worker;

  const Experiment _experiment;
};

////////////////////////////////////////////////////////////////////////////////
// EngineInProcess
//

struct EngineInProcess {
 public:
  using ptr = std::unique_ptr<EngineInProcess>;

  bee::OrError<SearchResultInfo::ptr> find_best_move(
    const Board& board,
    int depth,
    std::optional<bee::Span> max_time,
    std::function<void(SearchResultInfo::ptr&&)>&& on_update);

  static ptr create(
    const Experiment& experiment,
    const EvalParameters& eval_params,
    const PCP::ptr& pcp,
    size_t cache_size,
    bool clear_cache_before_move);

  void set_eval_params(EvalParameters&& eval_params);

  ~EngineInProcess();

 private:
  EngineInProcess(
    const Experiment& experiment,
    const EvalParameters& eval_params,
    const PCP::ptr& pcp,
    size_t cache_size,
    bool clear_cache_before_move);

  const Experiment _experiment;
  EvalParameters _eval_params;

  std::shared_ptr<TranspositionTable> _hash_table;
  std::shared_ptr<MoveHistory> _move_history;

  PCP::ptr _pcp;

  struct WaitTime {
    bee::Span wait;
    std::function<void()> on_done;
  };

  bee::Alarms _alarms;

  const bool _clear_cache_before_move;
};

} // namespace blackbit
