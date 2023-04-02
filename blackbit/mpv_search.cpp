#include "mpv_search.hpp"

#include "engine_core.hpp"
#include "move.hpp"
#include "rules.hpp"

#include "bee/nref.hpp"

#include <memory>
#include <mutex>
#include <set>
#include <thread>

using bee::format;
using bee::nref;
using bee::Span;
using bee::Time;
using std::atomic_bool;
using std::function;
using std::lock_guard;
using std::make_unique;
using std::multiset;
using std::mutex;
using std::optional;
using std::shared_ptr;
using std::string;
using std::thread;
using std::unique_ptr;
using std::variant;
using std::vector;
using std::visit;

namespace blackbit {

namespace {

struct PartialScore {
 public:
  PartialScore(const PartialScore&) = default;
  PartialScore(PartialScore&&) = default;

  PartialScore& operator=(const PartialScore&) = default;
  PartialScore& operator=(PartialScore&&) = default;

  static PartialScore at_most(Score score) { return AtMost{score}; }

  static PartialScore exactly(Score score) { return Exactly{score}; }

  constexpr Score exact_score() const noexcept
  {
    return visit(
      []<class T>(const T& v) -> Score {
        if constexpr (std::is_same_v<T, AtMost>) {
          assert(false);
        } else if constexpr (std::is_same_v<T, Exactly>) {
          return v.value;
        }
      },
      _value);
  }

  inline constexpr bool is_exact() const noexcept
  {
    return visit(
      []<class T>(const T&) {
        if constexpr (std::is_same_v<T, AtMost>) {
          return false;
        } else if constexpr (std::is_same_v<T, Exactly>) {
          return true;
        }
      },
      _value);
  }

  string to_string() const
  {
    return visit(
      []<class T>(const T& v) {
        if constexpr (std::is_same_v<T, AtMost>) {
          return format("AtMost($)", v.value);
        } else if constexpr (std::is_same_v<T, Exactly>) {
          return format("Exactly($)", v.value);
        }
      },
      _value);
  }

 private:
  struct AtMost {
    Score value;
  };

  struct Exactly {
    Score value;
  };

  PartialScore(AtMost v) : _value(v) {}
  PartialScore(Exactly v) : _value(v) {}

  using value_type = variant<AtMost, Exactly>;

  value_type _value;
};

struct MoveSearchState {
  using ptr = unique_ptr<MoveSearchState>;

  MoveSearchState(const MoveSearchState&) = delete;
  MoveSearchState(MoveSearchState&&) = delete;

  MoveSearchState& operator=(const MoveSearchState& other) = delete;
  MoveSearchState& operator=(MoveSearchState&& other) = delete;

  explicit MoveSearchState(Move m) : m(m) {}
  Move m;
  bool taken = false;
  int next_depth = 1;

  SearchResultInfo::ptr last_result = nullptr;
  PartialScore last_score = PartialScore::at_most(Score::max());
};

struct MpvContext : public std::enable_shared_from_this<MpvContext> {
 public:
  MpvContext(
    unique_ptr<Board>&& board,
    const int max_depth,
    const int max_pvs,
    const optional<int> num_workers_opt,
    shared_ptr<TranspositionTable> hash_table,
    shared_ptr<MoveHistory> move_history,
    const shared_ptr<atomic_bool>& should_stop,
    const Experiment& experiment,
    const EvalParameters& eval_params,
    function<void(vector<SearchResultInfo::ptr>&&)>&& on_update)
      : _board(std::move(board)),
        _max_depth(max_depth),
        _max_pvs(max_pvs),
        _num_workers(num_workers_opt.value_or(1)),
        _hash_table(hash_table),
        _move_history(move_history),
        _should_stop(should_stop),
        _experiment(experiment),
        _eval_params(eval_params),
        _on_update(std::move(on_update)),
        _player(_board->turn)
  {
    assert(_max_pvs > 0);
    assert(_max_depth > 0);
    assert(_on_update != nullptr);
  }

  void sort_moves(vector<nref<MoveSearchState>>& sorted_moves)
  {
    auto is_better_than =
      [&](nref<MoveSearchState> rm1, nref<MoveSearchState> rm2) {
        auto& m1 = *rm1;
        auto& m2 = *rm2;

        if (!m1.last_score.is_exact() && !m2.last_score.is_exact()) {
          return false;
        } else if (!m1.last_score.is_exact()) {
          return false;
        } else if (!m2.last_score.is_exact()) {
          return true;
        }

        auto s1 = m1.last_score.exact_score();
        auto s2 = m2.last_score.exact_score();

        if (s1 != s2) {
          return s1 > s2;
        } else if (m1.last_result == nullptr && m2.last_result == nullptr) {
          return false;
        } else if (m1.last_result == nullptr) {
          return false;
        } else if (m2.last_result == nullptr) {
          return true;
        } else if (m1.last_result->depth != m2.last_result->depth) {
          return m1.last_result->depth > m2.last_result->depth;
        } else {
          return m1.m < m2.m;
        }
      };

    sort(sorted_moves.begin(), sorted_moves.end(), is_better_than);
  };

  void update_result(
    Move m,
    MoveSearchState& move_state,
    SearchResultOneDepth&& result,
    Span ellapsed,
    int depth,
    Score lower_bound)
  {
    auto score = result.score();
    auto result_info = SearchResultInfo::create(
      m, std::move(result.pv()), score, _node_count, depth, ellapsed);

    move_state.last_result = std::move(result_info);

    if (score <= lower_bound) {
      move_state.last_score = PartialScore::at_most(lower_bound);
    } else {
      move_state.last_score = PartialScore::exactly(score);
    }

    vector<nref<MoveSearchState>> sorted_moves;
    for (auto& m : _legal_moves) { sorted_moves.emplace_back(m); }
    sort_moves(sorted_moves);

    vector<SearchResultInfo::ptr> results;
    results.reserve(_max_pvs);
    for (auto& m : sorted_moves) {
      if (m->last_result == nullptr) continue;
      if (!m->last_score.is_exact()) { continue; }
      results.push_back(m->last_result->clone());
      results.back()->flip(_player);
      if (std::ssize(results) >= _max_pvs) break;
    }
    if (!results.empty()) {
      _latest_search_result.clear();
      for (auto& result : results) {
        _latest_search_result.push_back(result->clone());
      }
    }
    _on_update(std::move(results));
  };

  nref<MoveSearchState> select_work(bool can_bump_depth)
  {
    auto is_higher_pri_than =
      [](const MoveSearchState& s1, const MoveSearchState& s2) {
        if (s1.last_result == nullptr && s2.last_result == nullptr) {
          return false;
        } else if (s1.last_result == nullptr) {
          return true;
        } else if (s2.last_result == nullptr) {
          return false;
        } else if (s1.last_result->depth != s2.last_result->depth) {
          return s1.last_result->depth < s2.last_result->depth;
        } else if (s1.last_result->eval != s2.last_result->eval) {
          return s1.last_result->eval < s2.last_result->eval;
        } else {
          return false;
        }
      };
    nref<MoveSearchState> selected_state(nullptr);

    bool has_not_taken = false;
    for (auto& m : _legal_moves) {
      if (m->taken) continue;
      has_not_taken = true;
      if (m->next_depth > _current_depth) { continue; }
      if (
        selected_state == nullptr || is_higher_pri_than(*m, *selected_state)) {
        selected_state = m;
      }
    }

    if (
      selected_state == nullptr && can_bump_depth && has_not_taken &&
      _current_depth < _max_depth) {
      _current_depth++;
      return select_work(false);
    } else if (selected_state != nullptr) {
      selected_state->taken = true;
    }
    return selected_state;
  }

  nref<MoveSearchState> select_work()
  {
    lock_guard<mutex> guard(_mutex_critical_part);
    return select_work(true);
  }

  void run_worker()
  {
    Board board = *_board;
    while (true) {
      if (_should_stop->load() || _current_depth > _max_depth) { break; }
      auto move_state = select_work();
      if (move_state == nullptr) { break; }
      Move m = move_state->m;
      int depth = move_state->next_depth;
      Score lower_bound = _lower_bound.at(depth);
      move_state->next_depth++;
      auto mi = board.move(m);
      auto core = EngineCore::create(
        board,
        _hash_table,
        _move_history,
        nullptr,
        false,
        _should_stop,
        _experiment,
        _eval_params);
      board.undo(m, mi);
      must(r, core->search_one_depth(depth, Score::min(), -lower_bound));
      if (!r.has_value()) { break; }
      auto& result = r.value();
      result.flip();
      result.prepend_move(m);
      Span ellapsed = Time::monotonic().diff(_start);
      {
        lock_guard<mutex> guard(_mutex_critical_part);
        move_state->taken = false;
        _node_count += result.nodes();
        auto& best_scores = _best_scores[depth];
        best_scores.insert(result.score());
        update_result(
          m, *move_state, std::move(result), ellapsed, depth, lower_bound);
        if (int(best_scores.size()) > _max_pvs) {
          // remove the lowest score
          best_scores.erase(best_scores.begin());
        }
        if (int(best_scores.size()) == _max_pvs) {
          _lower_bound[depth] = *best_scores.begin() - Score::of_pawns(1.0);
        }
      }
    }
  };

  bee::OrError<vector<SearchResultInfo::ptr>> search_multi_pv()
  {
    _start = Time::monotonic();

    MoveVector valid_moves;
    auto scratch = Rules::make_scratch(*_board);
    Rules::list_moves(*_board, scratch, valid_moves);
    _legal_moves.reserve(valid_moves.size());
    for (Move m : valid_moves) {
      if (Rules::is_legal_move(*_board, scratch, m)) {
        _legal_moves.push_back(make_unique<MoveSearchState>(m));
      }
    }
    if (_legal_moves.empty()) {
      _on_update({});
      return bee::Error("No legal moves");
    }

    _lower_bound.resize(_max_depth + 1, Score::min());
    _best_scores.resize(_max_depth + 1);

    vector<thread> workers;
    auto ptr = shared_from_this();
    for (int i = 0; i < _num_workers; i++) {
      workers.push_back(thread([ptr]() { ptr->run_worker(); }));
    }
    for (auto& w : workers) { w.join(); }

    if (_latest_search_result.empty()) {
      return bee::Error("Engine failed to find a move on mpv search");
    }

    return std::move(_latest_search_result);
  }

 private:
  unique_ptr<Board> _board;
  int _max_depth;
  int _max_pvs;
  int _num_workers;
  shared_ptr<TranspositionTable> _hash_table;
  shared_ptr<MoveHistory> _move_history;
  shared_ptr<atomic_bool> _should_stop;
  Experiment _experiment;
  EvalParameters _eval_params;
  function<void(vector<SearchResultInfo::ptr>&&)> _on_update;

  const Color _player;
  vector<MoveSearchState::ptr> _legal_moves;
  Time _start;
  mutex _mutex_critical_part;
  uint64_t _node_count = 0;
  vector<multiset<Score>> _best_scores;
  vector<Score> _lower_bound;
  vector<SearchResultInfo::ptr> _latest_search_result;

  int _current_depth = 1;
};

} // namespace

bee::OrError<vector<SearchResultInfo::ptr>> MpvSearch::search(
  std::unique_ptr<Board>&& board,
  int max_depth,
  int max_pvs,
  std::optional<int> num_workers,
  const shared_ptr<TranspositionTable>& hash_table,
  const shared_ptr<MoveHistory>& move_history,
  const std::shared_ptr<std::atomic_bool>& should_stop,
  const Experiment& experiment,
  const EvalParameters& eval_params,
  std::function<void(std::vector<SearchResultInfo::ptr>&&)>&& on_update)
{
  if (max_depth < 1) { return bee::Error("Max depth must be at least 1"); }
  auto context = make_shared<MpvContext>(
    std::move(board),
    max_depth,
    max_pvs,
    num_workers,
    hash_table,
    move_history,
    should_stop,
    experiment,
    eval_params,
    std::move(on_update));
  return context->search_multi_pv();
}

} // namespace blackbit
