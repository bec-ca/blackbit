#include "pcp_generation.hpp"

#include "board.hpp"
#include "command/command_flags.hpp"
#include "engine.hpp"
#include "eval.hpp"
#include "generated_game_record.hpp"
#include "pcp.hpp"
#include "rules.hpp"

#include "bee/file_reader.hpp"
#include "bee/filesystem.hpp"
#include "bee/format_memory.hpp"
#include "bee/format_vector.hpp"
#include "bee/nref.hpp"
#include "bee/ref.hpp"
#include "bee/sort.hpp"
#include "command/command_builder.hpp"
#include "stone/stone_reader.hpp"
#include "stone/stone_writer.hpp"
#include "yasf/cof.hpp"

#include <algorithm>
#include <condition_variable>
#include <csignal>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <thread>
#include <unistd.h>
#include <unordered_map>

namespace fs = std::filesystem;

using bee::FileSystem;
using bee::print_line;
using bee::ref;
using bee::Span;
using bee::Time;
using std::make_shared;
using std::mutex;
using std::nullopt;
using std::optional;
using std::pair;
using std::shared_ptr;
using std::string;
using std::thread;
using std::unique_lock;
using std::unordered_map;
using std::vector;

namespace blackbit {
namespace {

struct GameTree {
 public:
  struct GameTreeNode {
   public:
    GameTreeNode(const string& fen, int ply) : fen(fen), ply(ply) {}
    const string fen;
    std::set<string> next_fens;
    int frequency = 0;
    const int ply;
  };

  void add_game(const vector<Move>& moves)
  {
    Board board;
    board.set_initial();
    auto node = find_or_add(board);
    for (Move m : moves) {
      node->frequency++;
      board.move(m);
      if (Rules::is_game_over_slow(board)) { break; }
      node->next_fens.insert(board.to_fen());
      node = find_or_add(board);
    }
  }

  const std::map<string, GameTreeNode> nodes() const { return _nodes; }

 private:
  ref<GameTreeNode> find_or_add(const Board& board)
  {
    auto fen = board.to_fen();
    auto it = _nodes.find(board.to_fen());
    if (it == _nodes.end()) {
      it = _nodes.emplace(fen, GameTreeNode(fen, board.ply())).first;
    }
    return it->second;
  }
  std::map<string, GameTreeNode> _nodes;
};

struct DynPCP final : public PCP {
 public:
  using ptr = shared_ptr<DynPCP>;

  virtual ~DynPCP() {}

  virtual bee::OrError<std::optional<entry>> lookup_raw(const std::string& fen)
  {
    std::shared_lock l(_lock);
    auto it = _positions.find(fen);
    if (it == _positions.end()) { return nullopt; }
    return it->second;
  }

  virtual bee::OrError<std::unordered_map<std::string, entry>> read_all()
  {
    std::shared_lock l(_lock);
    return _positions;
  }

  void update(const string& fen, const entry& e)
  {
    std::unique_lock l(_lock);
    _positions.insert_or_assign(fen, e);
  }

 private:
  unordered_map<string, entry> _positions;

  std::shared_mutex _lock;
};

struct PositionState {
 public:
  using ptr = shared_ptr<PositionState>;

  explicit PositionState(const gr::PCPEntry& entry) : _entry(entry) {}

  bool has_higher_priority_than(
    const ptr& other, Span initial_thinking_time) const
  {
    const auto& e1 = _entry;
    const auto& e2 = other->_entry;

    auto think_time1 = next_think_time(initial_thinking_time);
    auto think_time2 = other->next_think_time(initial_thinking_time);

    if (think_time1 != think_time2) {
      return think_time1 < think_time2;
    } else if (e1.ply != e2.ply) {
      return e1.ply > e2.ply;
    } else if (e1.frequency != e2.frequency) {
      return e1.frequency > e2.frequency;
    } else {
      return false;
    }
  }

  const gr::PCPEntry& entry() { return _entry; }

  void update_result(
    Span think_time, Time start_time, vector<gr::MoveInfo>&& best_moves)
  {
    _entry.think_time = think_time;
    _entry.best_moves = std::move(best_moves);
    _entry.last_update = Time::now();
    _entry.last_start = start_time;
  }

  void add_next(const ptr& state) { _next_states.insert(state); }
  void add_prev(const ptr& state) { _prev_states.insert(state); }

  const std::set<ptr> next_states() const { return _next_states; }

  void set_frequency_and_ply(int frequency, int ply)
  {
    _entry.frequency = frequency;
    _entry.ply = ply;
  }

  Time last_update() const { return _entry.last_update; }

  Span next_think_time(Span initial_thinking_time) const
  {
    auto want = _entry.think_time;
    if (!_need_to_redo()) { want = want * 2; }
    return std::max(initial_thinking_time, want);
  }

  bool is_busy() const { return _is_busy; }
  void set_is_busy(bool value) { _is_busy = value; }
  Span think_time() const { return _entry.think_time; }

  bool can_enqueue() const
  {
    if (_is_busy) return false;
    if (_next_states.empty()) { return true; }
    for (const auto& next : _next_states) {
      if (next->think_time() <= think_time() && next->is_busy()) {
        return false;
      }
    }
    for (const auto& next : _next_states) {
      if (
        next->think_time() > think_time() ||
        (next->last_update() > _entry.last_start &&
         next->think_time() == think_time())) {
        return true;
      }
    }
    return false;
  }

  const std::set<ptr>& prev_states() const { return _prev_states; }

 private:
  bool _need_to_redo() const
  {
    for (const auto& next : _next_states) {
      if (
        next->entry().think_time <= _entry.think_time &&
        next->last_update() >= _entry.last_start) {
        return true;
      }
    }
    return false;
  }

  gr::PCPEntry _entry;
  std::set<ptr> _next_states;
  std::set<ptr> _prev_states;
  Span _next_think_time = Span::zero();

  bool _is_busy = false;
};

volatile std::atomic<int> sigint_received = 0;

struct StatePriority {
  Span think_time;
  int ply;
  int frequency;
  Time last_update;

  bool operator<(const StatePriority& other) const
  {
    if (think_time != other.think_time) {
      return think_time > other.think_time;
    } else if (ply != other.ply) {
      return ply < other.ply;
    } else if (frequency != other.frequency) {
      return frequency < other.frequency;
    } else if (last_update != other.last_update) {
      return last_update > other.last_update;
    } else {
      return false;
    }
  }
};

struct StateQueue {
 public:
  void push(Span next_think_time, const PositionState::ptr& state)
  {
    auto& entry = state->entry();
    _queue.emplace(
      StatePriority{
        next_think_time,
        int(entry.ply),
        int(entry.frequency),
        state->last_update()},
      state);
  }

  std::pair<Span, PositionState::ptr> pop()
  {
    auto ret = _queue.top();
    _queue.pop();
    return {ret.first.think_time, ret.second};
  }

  bool empty() const { return _queue.empty(); }

  size_t size() const { return _queue.size(); }

 private:
  std::priority_queue<std::pair<StatePriority, PositionState::ptr>> _queue;
};

struct WorkManager {
 public:
  WorkManager(
    int min_frequency,
    int max_ply,
    Span max_think_time,
    const bee::FilePath& pcp_file,
    const Span save_table_every,
    const Span initial_think_time,
    const optional<int>& max_tasks,
    const optional<Span>& max_total_time)
      : _min_frequency(min_frequency),
        _max_ply(max_ply),
        _max_think_time(max_think_time),
        _initial_think_time(initial_think_time),
        _pcp_file(pcp_file),
        _save_table_every(save_table_every),
        _pcp(make_shared<DynPCP>()),
        _start(Time::monotonic()),
        _last_save(Time::monotonic()),
        _max_tasks(max_tasks),
        _max_total_time(max_total_time)
  {}

  void add_state(const PositionState::ptr& state)
  {
    auto l = unique_lock(_mutex);
    _update(state->entry());
    _maybe_enqueue(state);
  }

  void finish_state(
    const PositionState::ptr& state,
    Span think_time,
    Time start_time,
    vector<gr::MoveInfo>&& best_moves)
  {
    auto l = unique_lock(_mutex);
    state->update_result(think_time, start_time, std::move(best_moves));
    state->set_is_busy(false);
    _num_busy--;
    _update(state->entry());
    _cond.notify_one();
    for (auto& prev : state->prev_states()) { _maybe_enqueue(prev); }
    _maybe_enqueue(state);
  }

  optional<std::pair<Span, PositionState::ptr>> dequeue()
  {
    auto l = unique_lock(_mutex);
    if (_max_tasks.has_value() && _dequeues >= *_max_tasks) {
      print_line("Reached maximum number of tasks");
      return nullopt;
    }
    if (_max_total_time.has_value()) {
      auto total_time = Time::monotonic() - _start;
      if (total_time > *_max_total_time) {
        print_line("Reached maximum time");
        return nullopt;
      }
    }
    must_unit(_maybe_save_table_no_lock());

    while (true) {
      if (sigint_received.load() > 0) { return nullopt; }
      if (!_queue.empty()) { break; }

      if (_num_busy == 0) { return std::nullopt; }
      using namespace std::chrono_literals;
      _cond.wait_for(l, 10s);
    }

    auto [think_time, state] = _queue.pop();
    _dequeues++;

    auto& entry = state->entry();
    print_line(
      "f:$ p:$ t:$ q:$ d:$",
      entry.frequency,
      entry.ply,
      think_time,
      _queue.size(),
      _dequeues);

    return make_pair(think_time, state);
  }

  int size() const { return std::ssize(_queue); }

  int dequeues() const { return _dequeues; }

  bool is_done() const
  {
    auto l = unique_lock(_mutex);
    return _queue.empty() && _num_busy == 0;
  }

  bee::OrError<bee::Unit> save_table()
  {
    auto l = unique_lock(_mutex);
    return _save_table_no_lock();
  }

  PCP::ptr pcp() const { return _pcp; }

 private:
  void _maybe_enqueue(const PositionState::ptr& state)
  {
    auto think_time = state->next_think_time(_initial_think_time);
    auto& entry = state->entry();
    if (
      state->can_enqueue() && think_time < _max_think_time &&
      entry.ply <= _max_ply && entry.frequency >= _min_frequency) {
      _queue.push(think_time, state);
      state->set_is_busy(true);
      _num_busy++;
    }
  }

  bee::OrError<bee::Unit> _save_table_no_lock()
  {
    auto start = Time::monotonic();
    auto tmp_file = _pcp_file + ".tmp";
    vector<pair<string, string>> data(_positions.begin(), _positions.end());
    bail_unit(stone::StoneWriter::write(tmp_file, data));
    fs::rename(tmp_file.to_std_path(), _pcp_file.to_std_path());
    print_line(
      "Wrote opening table of size: $, Took: $",
      data.size(),
      Time::monotonic() - start);
    return bee::ok();
  }

  bee::OrError<bee::Unit> _maybe_save_table_no_lock()
  {
    auto now = Time::monotonic();
    if (_last_save + _save_table_every < now) {
      _last_save = now;
      return _save_table_no_lock();
    }
    return bee::ok();
  };

  void _update(const gr::PCPEntry& entry)
  {
    _pcp->update(entry.fen, entry);
    _positions.insert_or_assign(entry.fen, yasf::Cof::serialize(entry));
  }

  // fields
  const int _min_frequency;
  const int _max_ply;
  const Span _max_think_time;
  const Span _initial_think_time;
  const bee::FilePath _pcp_file;

  const Span _save_table_every;

  mutable mutex _mutex;
  std::condition_variable _cond;

  StateQueue _queue;

  DynPCP::ptr _pcp;

  unordered_map<string, string> _positions;

  const Time _start;
  Time _last_save;

  std::optional<int> _max_tasks;

  std::optional<Span> _max_total_time;

  int _dequeues = 0;
  int _num_busy = 0;
};

void handle_sigint(int)
{
  int previous = sigint_received.fetch_add(1);
  if (previous > 0) {
    const char msg[] = "SIGINT was received twice, exiting now...\n";
    write(STDOUT_FILENO, msg, sizeof(msg));
    std::_Exit(1);
  } else {
    const char msg[] =
      "SIGINT was received, waiting for pending work to finish...\n"
      "Send SIGINT again to exit immediatelly, unsaved work will be lost.\n";
    write(STDOUT_FILENO, msg, sizeof(msg));
  }
}

void run_worker(shared_ptr<WorkManager> queue)
{
  auto engine = Engine::create(
    Experiment::base(),
    EvalParameters::default_params(),
    queue->pcp(),
    1ull << 32,
    true);

  Board board;
  while (auto next = queue->dequeue()) {
    auto start_time = Time::now();
    auto [think_time, state] = *next;
    auto& entry = state->entry();

    board.set_fen(entry.fen);

    auto result =
      engine->find_best_moves_mpv_sp(board, 100, 4, think_time, [](auto&&) {});

    if (result.is_error()) { continue; }

    vector<gr::MoveInfo> best_moves;
    for (const auto& m : *result) {
      best_moves.push_back({
        .move = m->best_move,
        .pv = m->pv,
        .evaluation = m->eval,
        .depth = m->depth,
        .nodes = m->nodes,
        .think_time = m->think_time,
      });
    }

    queue->finish_state(state, think_time, start_time, std::move(best_moves));
  }
}

bee::OrError<bee::Unit> read_games(
  unordered_map<string, PositionState::ptr>& existing_pcp,
  const string& games_filename)
{
  bee::print_line("Reading games...");
  bail(
    games_file,
    bee::FileReader::open(bee::FilePath::of_string(games_filename)));
  GameTree game_tree;
  int count_games = 0;
  while (!games_file->is_eof()) {
    bail(line, games_file->read_line());
    bail(game, yasf::Cof::deserialize<gr::Game>(line));
    vector<Move> moves;
    for (auto& m : game.moves) { moves.push_back(m.move); }
    game_tree.add_game(moves);
    count_games++;
  }
  print_line("Games read: $", count_games);

  auto positions = game_tree.nodes();
  print_line("Unique positions read: $", positions.size());

  auto now = Time::now();
  for (auto& [fen, info] : positions) {
    auto it = existing_pcp.find(fen);
    if (it == existing_pcp.end()) {
      existing_pcp.emplace(
        fen,
        make_shared<PositionState>(gr::PCPEntry{
          .fen = fen,
          .think_time = Span::zero(),
          .frequency = info.frequency,
          .ply = info.ply,
          .best_moves = {},
          .last_update = now,
          .last_start = now,
        }));
    } else {
      it->second->set_frequency_and_ply(info.frequency, info.ply);
    }
  }

  // set next positions
  for (auto& [fen, info] : positions) {
    auto it = existing_pcp.find(fen);
    assert(it != existing_pcp.end());
    auto& state = it->second;
    for (const auto& next : info.next_fens) {
      auto next_it = existing_pcp.find(next);
      assert(next_it != existing_pcp.end());
      auto& next_state = next_it->second;
      state->add_next(next_state);
      next_state->add_prev(state);
    }
  }
  return bee::ok();
}

bee::OrError<bee::Unit> gen_main(
  const string& games_filename,
  Span initial_think_time,
  const Span& max_think_time,
  const bee::FilePath& pcp_file,
  const Span save_table_every,
  int min_frequency,
  int max_ply,
  int num_workers,
  const optional<int>& max_tasks,
  const optional<int>& max_total_time_sec)
{
  std::signal(SIGINT, handle_sigint);

  unordered_map<string, PositionState::ptr> existing_pcp;
  if (FileSystem::exists(pcp_file)) {
    bee::print_line("Reading existing pcp...");
    bail(ot, PCP::open_on_disk(pcp_file));
    bail(existing_pcp_table, ot->read_all());
    print_line("Existing pcp size: $", existing_pcp_table.size());
    for (const auto& [fen, entry] : existing_pcp_table) {
      existing_pcp.emplace(fen, make_shared<PositionState>(entry));
    }
  }

  bail_unit(read_games(existing_pcp, games_filename));

  optional<Span> max_total_time;
  if (max_total_time_sec.has_value()) {
    max_total_time = Span::of_seconds(*max_total_time_sec);
    print_line("Will run for at most: $", *max_total_time);
  }

  bee::print_line("Enqueueing work...");
  auto manager = make_shared<WorkManager>(
    min_frequency,
    max_ply,
    max_think_time,
    pcp_file,
    save_table_every,
    initial_think_time,
    max_tasks,
    max_total_time);
  for (auto& [_, entry] : existing_pcp) { manager->add_state(entry); }

  bee::print_line("Starting workers...");

  vector<thread> workers;
  for (int i = 0; i < num_workers; i++) {
    workers.emplace_back(run_worker, manager);
  }

  for (auto& t : workers) { t.join(); }

  bail_unit(manager->save_table());

  if (sigint_received.load()) {
    return bee::Error("Exited because SIGINT was received");
  }

  if (manager->is_done()) { return bee::Error("Nothing left to do"); }

  return bee::ok();
}

} // namespace

command::Cmd PCPGeneration::command()
{
  using namespace command;
  using namespace command::flags;
  auto file_path = flag_of_value_type<bee::FilePath>();
  auto builder = CommandBuilder("Created opening table");
  auto games_filename = builder.required("--games-file", string_flag);
  auto think_time_sec =
    builder.optional_with_default("--think-time-sec", float_flag, 60.0);
  auto max_think_time_sec =
    builder.optional("--max-think-time-sec", float_flag);
  auto opening_file = builder.required("--pcp-file", file_path);
  auto save_table_every =
    builder.optional_with_default("--save-table-every-sec", float_flag, 60.0);
  auto min_frequency =
    builder.optional_with_default("--min-frequency", int_flag, 2);
  auto num_workers =
    builder.optional_with_default("--num-workers", int_flag, 16);
  auto max_ply = builder.optional_with_default("--max-ply", int_flag, 100);
  auto max_tasks = builder.optional("--max-tasks", int_flag);
  auto max_total_time_sec = builder.optional("--max-total-time-sec", int_flag);
  return builder.run([=]() {
    return gen_main(
      *games_filename,
      Span::of_seconds(*think_time_sec),
      Span::of_seconds((*max_think_time_sec).value_or(1000000)),
      *opening_file,
      Span::of_seconds(*save_table_every),
      *min_frequency,
      *max_ply,
      *num_workers,
      *max_tasks,
      *max_total_time_sec);
  });
}

} // namespace blackbit
