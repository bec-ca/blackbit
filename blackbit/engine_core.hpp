#pragma once

#include "board.hpp"
#include "eval.hpp"
#include "move.hpp"
#include "move_history.hpp"
#include "pcp.hpp"
#include "score.hpp"
#include "search_result_info.hpp"
#include "transposition_table.hpp"

#include "bee/nref.hpp"
#include "bee/ref.hpp"

#include <atomic>

namespace blackbit {

struct SearchResultOneDepth {
 public:
  SearchResultOneDepth(
    Score score,
    const std::optional<Move>& m,
    std::vector<Move>&& pv,
    uint64_t nodes);

  Score score() const { return _score; }

  std::vector<Move>& pv() { return _pv; }
  const std::vector<Move>& pv() const { return _pv; }

  uint64_t nodes() const { return _nodes; }
  uint64_t& nodes() { return _nodes; }

  void flip() { _score = -_score; }

  void prepend_move(Move move)
  {
    _move = move;
    _pv.insert(_pv.begin(), move);
  }

  std::string to_string() const;

  std::optional<Move> move() const { return _move; }

 private:
  Score _score;
  std::optional<Move> _move;
  std::vector<Move> _pv;
  uint64_t _nodes;
};

struct SearchResultOneDepthMPV {
 public:
  SearchResultOneDepthMPV(const SearchResultOneDepthMPV&) = delete;
  SearchResultOneDepthMPV(SearchResultOneDepthMPV&&) = default;
  SearchResultOneDepthMPV(std::vector<SearchResultOneDepth>&& results);
  ~SearchResultOneDepthMPV();

  std::string to_string() const;

  std::vector<SearchResultOneDepth> results;

  bee::nref<PV> best_pv() const;

  Score min_score() const;
  Score max_score() const;

  uint64_t nodes() const;
};

struct EngineCore {
 public:
  using ptr = std::unique_ptr<EngineCore>;
  virtual ~EngineCore();

  virtual bee::OrError<std::optional<SearchResultOneDepth>> search_one_depth(
    int depth, Score lower_bound, Score upper_bound) = 0;

  virtual bee::OrError<std::optional<SearchResultOneDepthMPV>>
  search_one_depth_mpv(
    int depth, int max_pvs, Score lower_bound, Score upper_bound) = 0;

  static ptr create(
    const Board& board,
    const std::shared_ptr<TranspositionTable>& hash_table,
    const std::shared_ptr<MoveHistory>& move_history,
    const PCP::ptr& pcp,
    bool allow_partial,
    const std::shared_ptr<std::atomic_bool>& should_stop,
    const Experiment& experiment,
    const EvalParameters& eval_params);
};

} // namespace blackbit
