#include "engine_core.hpp"

#include "board.hpp"
#include "eval.hpp"
#include "experiment_framework.hpp"
#include "move_history.hpp"
#include "pcp.hpp"
#include "rules.hpp"
#include "transposition_table.hpp"

#include "bee/format_map.hpp"
#include "bee/format_memory.hpp"
#include "bee/format_vector.hpp"
#include "bee/nref.hpp"

#include <atomic>
#include <cstdint>
#include <memory>

using bee::nref;
using std::array;
using std::atomic_bool;
using std::max;
using std::nullopt;
using std::optional;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;

namespace blackbit {

// auto turn_score_flag =
//   ExperimentFlag::register_flag("turn_score", -500, 500, 0);

namespace {

constexpr Score threshold_per_depth = Score::of_milli_pawns(1100);

struct SearchInterruptRequested {};

struct SearchResult {
 public:
  SearchResult(SearchResult&& other) = default;

  SearchResult() : _score(Score::min()) {}

  explicit SearchResult(Score score) : _score(score), _pv(nullptr) {}

  SearchResult(Score score, std::unique_ptr<PV>&& pv)
      : _score(score), _pv(std::move(pv))
  {}

  static SearchResult combine(Move move, SearchResult&& res)
  {
    auto pv = std::make_unique<PV>(move, std::move(res._pv));
    return SearchResult(res._score, std::move(pv));
  }

  static SearchResult of_single_move(Move move, Score score)
  {
    auto pv = std::make_unique<PV>(move, nullptr);
    return SearchResult(score, std::move(pv));
  }

  bool operator<(const SearchResult& other) const
  {
    return _score < other._score;
  }

  bool operator>(const SearchResult& other) const
  {
    return _score > other._score;
  }

  void set_score(Score score)
  {
    _score = score;
    _pv = nullptr;
  }

  SearchResult& operator=(SearchResult&& other) = default;

  SearchResult clone() const { return SearchResult(_score, _pv->clone()); }

  void flip() { _score = -_score; }

  void backout_move() { _score = _score.inc_mate_moves(); }

  bool is_min() const { return _score == Score::min() && _pv == nullptr; }

  Score score() const { return _score; }

  Score min_score() const { return _score; }
  Score max_score() const { return _score; }

  const nref<PV> best_pv() const { return _pv; }
  nref<PV> best_pv() { return _pv; }

  void update_max(Move m, SearchResult&& cand)
  {
    if (cand.score() > score()) {
      _score = cand.score();
      _pv = std::make_unique<PV>(m, std::move(cand._pv));
    }
  }

 private:
  Score _score;
  std::unique_ptr<PV> _pv;
};

struct SearchResultMPV {
 public:
  SearchResultMPV(int max_pvs) : _num_moves(max_pvs) {}

  void set_score(Score score)
  {
    _results.clear();
    _results.emplace(score, SearchResult(score));
  }

  void set_result(SearchResult&& res)
  {
    _results.clear();
    _results.emplace(res.score(), std::move(res));
  }

  const auto& results() const { return _results; };

  bool is_min() const { return _results.empty(); }

  void set_single_move(Move m, Score score)
  {
    _results.clear();
    _results.emplace(score, SearchResult::of_single_move(m, score));
  }

  const SearchResult& best_result() const { return _results.begin()->second; }
  const SearchResult& worst_result() const
  {
    return (--_results.end())->second;
  }

  Score max_score() const
  {
    if (_results.empty()) { return Score::min(); }
    return best_result().score();
  }
  Score min_score() const
  {
    if (std::ssize(_results) < _num_moves) { return Score::min(); }
    return worst_result().score();
  }

  void update_max(Move m, SearchResult&& cand)
  {
    _results.emplace(cand.score(), SearchResult::combine(m, std::move(cand)));
    if (std::ssize(_results) > _num_moves) {
      auto last = --_results.end();
      _results.erase(last);
    }
  }

  nref<PV> best_pv() const
  {
    if (_results.empty()) {
      return nullptr;
    } else {
      return best_result().best_pv();
    }
  }

 private:
  std::multimap<Score, SearchResult, std::greater<Score>> _results;
  const int _num_moves;
};

std::vector<Move> pv_to_vector(nref<PV> pv)
{
  if (pv == nullptr) {
    return {};
  } else {
    return pv->to_vector();
  }
}

template <class T> std::optional<T> front_opt(const vector<T>& v)
{
  if (v.empty()) {
    return nullopt;
  } else {
    return v.front();
  }
}

struct SearchContext final : public EngineCore {
  using ptr = unique_ptr<SearchContext>;

  SearchContext(
    const Board& board,
    const std::shared_ptr<TranspositionTable>& hash_table,
    const std::shared_ptr<MoveHistory>& move_history,
    const PCP::ptr& pcp,
    bool allow_partial,
    const shared_ptr<atomic_bool>& should_stop,
    const Experiment& experiment,
    const EvalParameters& eval_params)
      : _board(board),
        _hash_table(hash_table),
        _move_history(move_history),
        _pcp(pcp),
        _should_stop(should_stop),
        _experiment(experiment),
        _eval_params(eval_params),
        _allow_partial(allow_partial)
  {}

  virtual ~SearchContext() {}

  bool is_test() const { return _experiment.is_test(); }
  bool is_base() const { return _experiment.is_base(); }

  SearchResult search_rec_outer(
    const EvalScratch& scratch,
    const int depth,
    const int ply,
    const Score input_alpha,
    const Score input_beta)

  {
    auto ret = search_rec_inner(
      scratch,
      depth - 1,
      ply + 1,
      input_beta.dec_mate_moves().neg(),
      input_alpha.dec_mate_moves().neg());
    ret.flip();
    ret.backout_move();
    return ret;
  }

  template <class Result = SearchResult, bool is_root = false>
  Result search_rec_inner(
    const EvalScratch& pre_move_scratch,
    const int depth,
    const int ply,
    const Score input_alpha,
    const Score input_beta,
    Result result = Result())
  {
    const bool is_pv = input_alpha == input_beta.next();
    const bool is_quiescent = (depth <= 0);

    _node_count++;

    if (_should_stop->load() && _interruptible) {
      throw(SearchInterruptRequested());
    }

    if constexpr (!is_root) {
      if (ply > 512 || Rules::is_draw_without_stalemate(_board)) {
        result.set_score(Score::zero());
        return result;
      }
    }

    if constexpr (!is_root) {
      if (_pcp != nullptr && ply <= 3) {
        auto entry_opt = _pcp->lookup(_board.to_fen());
        if (!entry_opt.is_error() && entry_opt->has_value()) {
          auto& entry = **entry_opt;
          return Result(
            entry->eval.flip_for_color(_board.turn), PV::of_vector(entry->pv));
        }
      }
    }

    // prune if forced mate already found

    // Only prune best/worst case if not pv so we can get a nice pv sequence
    if (!is_pv) {
      Score best_possible = Score::of_moves_to_mate(1);
      if (best_possible <= input_alpha) {
        result.set_score(input_alpha);
        return result;
      }
      Score worst_possible = Score::of_moves_to_mate(0).neg();
      if (worst_possible >= input_beta) {
        result.set_score(input_beta);
        return result;
      }
    }

    // Don't use the cache if in quiescent mode
    Move high_pri_move = Move::invalid();
    const TranspositionTable::hash_slot* slot = nullptr;
    if (!is_quiescent) {
      slot = _hash_table->find(_board);

      // Check hash table
      if (slot != nullptr) {
        if constexpr (!is_root) {
          // Only do cache pruning if not pv so we can get a nice pv sequence
          if (!is_pv) {
            if (slot->depth >= depth) {
              if (slot->lower_bound >= input_beta) {
                return Result::of_single_move(slot->move, slot->lower_bound);
              } else if (slot->upper_bound <= input_alpha) {
                return Result::of_single_move(slot->move, slot->upper_bound);
              }
            }
            if (slot->depth < depth) {
              int d = depth - slot->depth;
              if (slot->lower_bound - (threshold_per_depth * d) >= input_beta) {
                return Result::of_single_move(slot->move, input_beta);
              }
            }
          }
        }
        // Try the cached best move first
        if (slot->move.is_valid()) { high_pri_move = slot->move; }
      }
    }

    // In quiescent, we only capture pieces
    auto& list = _move_lists[ply];
    list.clear();
    if (is_quiescent) {
      result.set_score(eval_board(pre_move_scratch));
      if (result.min_score() >= input_beta) { return result; }
      Rules::list_takes(_board, list);
    } else {
      Rules::list_moves(_board, pre_move_scratch, list);
    }

    // Sort moves from likely best to worst
    _move_history->sort_moves(_board, list, high_pri_move);

    bool has_valid_move = false;
    bool first = true;

    for (const auto& m : list) {
      ASSERT(m.is_valid());

      /* move */
      auto mi = _board.move(m);
      auto scratch = Rules::make_scratch(_board);

      if (!Rules::is_king_under_attack(_board, scratch, oponent(_board.turn))) {
        has_valid_move = true;

        auto inner_search = [&](int depth) -> SearchResult {
          // move down the search tree
          Score new_alpha = max(result.min_score(), input_alpha);

          auto depth_to_shorten = [&]() {
            if (first || (slot == nullptr) || depth < 4 || mi.capturou) {
              return 0;
            }
            return 2;
          };

          int depth_shortened = depth_to_shorten();
          bool did_pv_search = false;
          SearchResult child_result;
          if (!first && !is_pv && depth > 1) {
            child_result = search_rec_outer(
              scratch,
              depth - depth_shortened,
              ply,
              new_alpha,
              new_alpha.next());
            did_pv_search = true;
          }

          if (!did_pv_search || child_result.score() > new_alpha) {
            child_result = search_rec_outer(
              scratch, depth - depth_shortened, ply, new_alpha, input_beta);
          }

          if (depth_shortened > 0 && child_result.score() > new_alpha) {
            child_result =
              search_rec_outer(scratch, depth, ply, new_alpha, input_beta);
          }
          return child_result;
        };

        try {
          result.update_max(m, inner_search(depth));
        } catch (SearchInterruptRequested) {
          _board.undo(m, mi);
          if constexpr (is_root) {
            if (
              !result.is_min() && slot != nullptr &&
              result.max_score() > input_alpha && _allow_partial) {
              return result;
            }
          }
          throw(SearchInterruptRequested());
        }

        first = false;
      }

      // backout
      _board.undo(m, mi);

      // prune
      if (result.min_score() >= input_beta) { break; }
    }

    if (!is_quiescent && !has_valid_move) {
      if (Rules::is_king_under_attack(_board, pre_move_scratch, _board.turn)) {
        result.set_score(Score::of_moves_to_mate(0).neg());
      } else {
        // draw
        result.set_score(Score::zero());
      }
    }

    if (auto pv = result.best_pv()) {
      // Store result on the hash table
      auto m = pv->move;
      auto score = result.max_score();
      if (!is_quiescent) {
        if (score <= input_alpha) {
          /* upper bound */
          _hash_table->insert(
            _board, depth, Score::of_moves_to_mate(1).neg(), score, m);
        } else if (score >= input_beta) {
          /* lower bound */
          _hash_table->insert(
            _board, depth, score, Score::of_moves_to_mate(1), m);
        } else {
          /* exact eval */
          _hash_table->insert(_board, depth, score, score, m);
        }
      }
      if (m.is_valid()) { _move_history->add(_board, m); }
    }

    return result;
  }

  Score eval_board(const EvalScratch& scratch)
  {
    return Evaluator::eval_for_current_player(
      _board, scratch, _experiment, _eval_params);
  }

  virtual bee::OrError<optional<SearchResultOneDepth>> search_one_depth(
    int depth, Score lower_bound, Score upper_bound) override
  {
    if (depth <= 0) { return bee::Error("Search depth must be at least 1"); }

    _node_count = 0;
    auto search_or_stop = [&]() -> optional<SearchResult> {
      _interruptible = (depth > 1);
      try {
        return search_rec_inner<SearchResult, true>(
          Rules::make_scratch(_board), depth, 0, lower_bound, upper_bound);
      } catch (SearchInterruptRequested) {
        return nullopt;
      }
    };
    auto result_or_none = search_or_stop();
    if (!result_or_none.has_value()) { return nullopt; }
    auto& result = *result_or_none;

    auto pv = pv_to_vector(result.best_pv());
    optional<Move> m = front_opt(pv);

    return SearchResultOneDepth(result.score(), m, std::move(pv), _node_count);
  }

  virtual bee::OrError<optional<SearchResultOneDepthMPV>> search_one_depth_mpv(
    int depth, int max_pvs, Score lower_bound, Score upper_bound) override
  {
    if (depth <= 0) { return bee::Error("Search depth must be at least 1"); }

    _node_count = 0;
    auto search_or_stop = [&]() -> optional<SearchResultMPV> {
      _interruptible = (depth > 1);
      try {
        return search_rec_inner<SearchResultMPV, true>(
          Rules::make_scratch(_board),
          depth,
          0,
          lower_bound,
          upper_bound,
          SearchResultMPV(max_pvs));
      } catch (SearchInterruptRequested) {
        return nullopt;
      }
    };
    auto result_or_none = search_or_stop();
    if (!result_or_none.has_value()) { return nullopt; }
    auto& result = *result_or_none;

    vector<SearchResultOneDepth> results;
    for (const auto& [_, res] : result.results()) {
      auto pv = pv_to_vector(res.best_pv());
      optional<Move> m = front_opt(pv);
      results.emplace_back(res.score(), m, std::move(pv), _node_count);
    }

    return SearchResultOneDepthMPV(std::move(results));
  }

  uint64_t node_count() const { return _node_count; }

  const Board& board() const { return _board; }

 private:
  Board _board;
  uint64_t _node_count = 0;

  shared_ptr<TranspositionTable> _hash_table;
  shared_ptr<MoveHistory> _move_history;

  PCP::ptr _pcp;

  shared_ptr<atomic_bool> _should_stop;

  bool _interruptible = false;

  Experiment _experiment;

  EvalParameters _eval_params;

  array<MoveVector, 128> _move_lists;

  const bool _allow_partial;

  // Experiments
};

} // namespace

////////////////////////////////////////////////////////////////////////////////
// EngineCore
//

EngineCore::~EngineCore() {}

EngineCore::ptr EngineCore::create(
  const Board& board,
  const shared_ptr<TranspositionTable>& hash_table,
  const shared_ptr<MoveHistory>& move_history,
  const PCP::ptr& pcp,
  bool allow_partial,
  const shared_ptr<atomic_bool>& should_stop,
  const Experiment& experiment,
  const EvalParameters& eval_params)
{
  return make_unique<SearchContext>(
    board,
    hash_table,
    move_history,
    pcp,
    allow_partial,
    should_stop,
    experiment,
    eval_params);
}

////////////////////////////////////////////////////////////////////////////////
// SearchResultOneDepth
//

SearchResultOneDepth::SearchResultOneDepth(
  Score score, const optional<Move>& m, vector<Move>&& pv, uint64_t nodes)
    : _score(score), _move(m), _pv(std::move(pv)), _nodes(nodes)
{}

string SearchResultOneDepth::to_string() const
{
  return bee::format("[s:$ pv:$ nodes:$]", _score, _pv, _nodes);
}

////////////////////////////////////////////////////////////////////////////////
// SearchResultOneDepthMPV
//

SearchResultOneDepthMPV::~SearchResultOneDepthMPV() {}
SearchResultOneDepthMPV::SearchResultOneDepthMPV(
  vector<SearchResultOneDepth>&& results)
    : results(std::move(results))
{}

string SearchResultOneDepthMPV::to_string() const
{
  return bee::format(results);
}

uint64_t SearchResultOneDepthMPV::nodes() const
{
  uint64_t out = 0;
  for (auto& r : results) { out += r.nodes(); }
  return out;
}

} // namespace blackbit
