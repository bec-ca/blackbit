#include "bee/testing.hpp"

#include "bee/format.hpp"
#include "bee/format_vector.hpp"
#include "engine.hpp"
#include "rules.hpp"

#include <thread>

using bee::print_line;
using bee::Span;
using std::nullopt;
using std::optional;
using std::string;
using std::vector;

namespace blackbit {
namespace {

string pp(const Board& b, Move m) { return Rules::pretty_move(b, m); }

auto print_callback(const Board& board)
{
  return [board](SearchResultInfo::ptr&& info) {
    print_line(
      "move:$ eval:$ depth:$ nodes:$ pv:$",
      pp(board, info->best_move),
      info->eval,
      info->depth,
      info->nodes,
      info->make_pretty_moves(board));
  };
}

void run_test_in_proc(const optional<string>& fen)
{
  auto engine = EngineInProcess::create(
    Experiment::base(), EvalParameters::default_params(), nullptr, 1, false);
  Board board;
  if (fen.has_value()) {
    board.set_fen(*fen);
  } else {
    board.set_initial();
  }
  must(res, engine->find_best_move(board, 2, nullopt, print_callback(board)));
  auto move = res->best_move;
  assert(move.is_valid());
  print_line(pp(board, move));
};

TEST(basic_in_process)
{
  run_test_in_proc(nullopt);
  run_test_in_proc(nullopt);
}

TEST(basic)
{
  auto run_test = []() {
    auto engine = Engine::create(
      Experiment::base(),
      EvalParameters::default_params(),
      nullptr,
      100,
      false);
    Board board;
    board.set_initial();
    must(res, engine->find_best_move(board, 2, nullopt, print_callback(board)));
    auto move = res->best_move;
    assert(move.is_valid());
    print_line(pp(board, move));
  };

  run_test();
  run_test();
}

TEST(background)
{
  auto engine = Engine::create(
    Experiment::base(), EvalParameters::default_params(), nullptr, 100, false);
  Board board;
  board.set_initial();
  auto move_future = engine->start_search(board, 2, print_callback(board));
  must(res, move_future->wait());
  auto move = res->best_move;
  assert(move.is_valid());
  print_line(pp(board, move));
}

TEST(background_multiple_searches)
{
  auto engine = Engine::create(
    Experiment::base(), EvalParameters::default_params(), nullptr, 100, false);
  Board board;
  board.set_initial();
  for (int i = 0; i < 4; i++) {
    auto move_future = engine->start_search(board, 2, print_callback(board));
    must(res, move_future->wait());
    auto move = res->best_move;
    assert(move.is_valid());
    print_line(pp(board, move));
  }
}

void sleep_ms(int millis)
{
  std::this_thread::sleep_for(std::chrono::milliseconds(millis));
}

TEST(background_timed)
{
  auto engine = Engine::create(
    Experiment::base(), EvalParameters::default_params(), nullptr, 100, false);
  Board board;
  board.set_initial();
  auto move_future = engine->start_search(board, 1000, nullptr);
  sleep_ms(100);
  must(res, move_future->wait_at_most(Span::zero()));
  auto move = res->best_move;
  print_line(move.is_valid());
}

TEST(background_cache_size)
{
  auto run_test = [](size_t cache_size) {
    print_line("Hash size: $", cache_size);
    auto engine = Engine::create(
      Experiment::base(),
      EvalParameters::default_params(),
      nullptr,
      cache_size,
      false);
    Board board;
    board.set_initial();
    must(res, engine->find_best_move(board, 4, nullopt, print_callback(board)));
    auto move = res->best_move;
    assert(move.is_valid());
    print_line(pp(board, move));
  };
  run_test(1);
  run_test(1000000);
}

TEST(multi_pv_search)
{
  auto engine = Engine::create(
    Experiment::base(), EvalParameters::default_params(), nullptr, 100, false);
  Board board;
  board.set_initial();
  auto move_future = engine->start_mpv_search(
    board, 1, 5, 1, [board](const vector<SearchResultInfo::ptr>& results) {
      print_line("-------------------------------------");
      for (const auto& r : results) {
        print_line("$ $", r->eval, r->make_pretty_moves(board));
      }
    });
  must_unit(move_future->wait());
}

TEST(multi_pv_search_with_mate)
{
  auto engine = Engine::create(
    Experiment::base(), EvalParameters::default_params(), nullptr, 100, false);
  Board board;
  board.set_fen("1k6/2p5/p2qp3/p6p/2KPb2P/1P3r2/P1R5/R7 b - - 0 42");
  vector<SearchResultInfo::ptr> last_result;
  auto move_future = engine->start_mpv_search(
    board, 5, 10, 1, [&last_result](vector<SearchResultInfo::ptr>&& results) {
      last_result = std::move(results);
    });
  must_unit(move_future->wait());
  print_line("-------------------------------------");
  for (const auto& r : last_result) {
    print_line("$ $", r->eval, r->make_pretty_moves(board));
  }
}

TEST(multi_stop)
{
  auto engine = Engine::create(
    Experiment::base(), EvalParameters::default_params(), nullptr, 100, false);
  Board board;
  board.set_initial();
  auto move_future = engine->start_mpv_search(
    board, 100, 5, 1, [](const vector<SearchResultInfo::ptr>&) {});
  sleep_ms(100);
  must_unit(move_future->result_now());
}

TEST(multi_worker)
{
  auto engine = Engine::create(
    Experiment::base(), EvalParameters::default_params(), nullptr, 100, false);
  Board board;
  board.set_initial();
  auto move_future = engine->start_mpv_search(
    board, 100, 5, 10, [](const vector<SearchResultInfo::ptr>&) {});
  sleep_ms(100);
  must_unit(move_future->result_now());
}

TEST(draw) { run_test_in_proc("k7/8/8/8/Kp6/8/8/8 w - - 0 42"); }

} // namespace

} // namespace blackbit
