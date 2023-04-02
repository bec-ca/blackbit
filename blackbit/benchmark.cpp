#include "benchmark.hpp"

#include "bot_state.hpp"
#include "engine.hpp"
#include "experiment_framework.hpp"
#include "game_result.hpp"
#include "random.hpp"
#include "rules.hpp"
#include "statistics.hpp"

#include "bee/file_reader.hpp"
#include "bee/format_vector.hpp"
#include "bee/queue.hpp"
#include "bee/time.hpp"
#include "bee/util.hpp"
#include "command/command_builder.hpp"

#include <sstream>

using bee::format;
using bee::print_line;
using bee::Queue;
using bee::Span;
using bee::Time;
using std::make_shared;
using std::optional;
using std::shared_ptr;
using std::string;
using std::vector;

namespace blackbit {
namespace {

struct Result {
  Span span;
  uint64_t nodes;
  int depth;
};

Experiment create_exp(bool test_mode)
{
  if (test_mode) {
    auto rng = Random::create(0);
    return Experiment::test(*rng);
  } else {
    return Experiment::base();
  }
}

void run_worker(
  bool test_mode,
  Span time_to_think,
  shared_ptr<Queue<string>> fen_queue,
  shared_ptr<Queue<Result>> result_queue)
{
  auto engine = EngineInProcess::create(
    create_exp(test_mode),
    EvalParameters::default_params(),
    nullptr,
    1 << 30,
    true);

  for (string fen : *fen_queue) {
    Board board;
    board.set_fen(fen);
    if (
      Rules::result(board, Rules::make_scratch(board)) !=
      GameResult::NotFinished) {
      continue;
    }

    auto start = Time::monotonic();
    auto result = engine->find_best_move(board, 30, time_to_think, nullptr);
    auto span = Time::monotonic().diff(start);

    if (result.is_error()) {
      print_line("Engine failed to search, fen:$ error:$", fen, result.error());
      continue;
    }
    auto& r = *result.force();

    result_queue->push(
      Result{.span = span, .nodes = r.nodes, .depth = r.depth});
  }
  return;
}

bee::OrError<bee::Unit> run_benchmark(
  const string& positions_file,
  Span time_to_think,
  optional<int> num_positions_opt,
  optional<int> num_workers_opt,
  bool test_mode)
{
  bail(
    positions, bee::FileReader::open(bee::FilePath::of_string(positions_file)));
  bail(fens, positions->read_all_lines());

  int num_positions = num_positions_opt.value_or(512);

  auto fen_queue = make_shared<Queue<string>>();
  auto result_queue = make_shared<Queue<Result>>();

  int num_workers = num_workers_opt.value_or(16);
  vector<std::thread> workers;
  for (int i = 0; i < num_workers; i++) {
    workers.emplace_back(
      [=] { run_worker(test_mode, time_to_think, fen_queue, result_queue); });
  }

  auto rng = Random::create(0);
  for (int i = 1; i <= num_positions; i++) {
    fen_queue->push(fens[rng->rand64() % fens.size()]);
  }
  fen_queue->close();

  std::thread consumer([result_queue] {
    int positions = 0;
    vector<double> knodes_per_second;
    vector<double> depth;
    vector<double> actual_time;
    auto summarize = [](const vector<double>& values) {
      double mean = Statistics::mean(values);
      double stddev = Statistics::stddev(values);
      double confidence =
        Statistics::normal_confidence_95(stddev, values.size());
      return format("$±$", mean, confidence);
    };
    auto summary = [&]() {
      print_line(
        "positions:$ knodes/s:$ depth:$ actual_time(s):$",
        positions,
        summarize(knodes_per_second),
        summarize(depth),
        summarize(actual_time));
    };
    for (auto& res : *result_queue) {
      positions++;
      knodes_per_second.push_back(
        res.nodes / res.span.to_float_seconds() / 1000.0);
      depth.push_back(res.depth);
      actual_time.push_back(res.span.to_float_seconds());
      if (positions % 32 == 0) { summary(); }
    }
    summary();
  });

  for (auto& worker : workers) { worker.join(); }

  result_queue->close();
  consumer.join();

  return bee::unit;
}

bee::OrError<bee::Unit> run_benchmark_mpv()
{
  Board board;
  board.set_initial();
  Span sum_ellapsed = Span::zero();
  const int repeat = 100;
  auto engine = Engine::create(
    Experiment::base(),
    EvalParameters::default_params(),
    nullptr,
    1ull << 31,
    true);
  for (int i = 0; i < repeat; i++) {
    auto start_time = Time::monotonic();
    engine->find_best_moves_mpv(
      board, 8, 1, 16, Span::of_seconds(10), [](auto&&) {});
    sum_ellapsed += Time::monotonic() - start_time;
  }
  print_line("Average time: $", sum_ellapsed / repeat);
  return bee::unit;
}

} // namespace

command::Cmd Benchmark::command()
{
  using namespace command::flags;
  auto builder = command::CommandBuilder("Bechmark the bot");
  auto positions_file = builder.required("--positions-file", string_flag);
  auto time_to_think =
    builder.optional_with_default("--search-time-secs", float_flag, 2.0);
  auto num_positions = builder.optional("--num-positions", int_flag);
  auto num_workers = builder.optional("--num-workers", int_flag);
  auto test_mode = builder.no_arg("--test-mode");
  return builder.run([=] {
    return run_benchmark(
      *positions_file,
      Span::of_seconds(*time_to_think),
      *num_positions,
      *num_workers,
      *test_mode);
  });
}

command::Cmd Benchmark::command_mpv()
{
  using namespace command::flags;
  auto builder = command::CommandBuilder("Bechmark mpv overhead");
  return builder.run([=] { return run_benchmark_mpv(); });
}

} // namespace blackbit
