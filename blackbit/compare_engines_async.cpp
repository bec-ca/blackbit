#include "compare_engines_async.hpp"

#include "bee/file_reader.hpp"
#include "bee/file_writer.hpp"
#include "bee/format_vector.hpp"
#include "generated_game_record.hpp"
#include "random.hpp"
#include "self_play_async.hpp"
#include "yasf/cof.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>

using namespace async;

using bee::format;
using bee::print_line;
using bee::Span;
using std::make_shared;
using std::queue;
using std::string;
using std::vector;

namespace blackbit {

namespace gr = generated_game_record;

namespace {

template <class T> struct ExpPair {
  ExpPair(T&& base, T&& test) : base(std::move(base)), test(std::move(test)) {}

  T base;
  T test;
};

struct GameScore {
  double white_score;
  double black_score;

  double score(Color color)
  {
    switch (color) {
    case Color::White:
      return white_score;
    case Color::Black:
      return black_score;
    case Color::None:
      assert(false);
    }
    assert(false);
  }
};

GameScore game_result_to_score(GameResult result)
{
  switch (result) {
  case GameResult::WhiteWon:
    return {1, 0};
  case GameResult::BlackWon:
    return {0, 1};
  case GameResult::Draw:
    return {0.5, 0.5};
  case GameResult::NotFinished:
    return {0.0, 0.0};
  }
  assert(false);
}

struct ExpGameResult {
  int id;
  GameResult game_result;
  Color test_color;
  vector<gr::MoveInfo> moves;
  string starting_fen;
  string final_fen;
};

struct GameInfo {
  int id;
  string starting_fen;
  Color test_color;

  EngineFactory base_engine_factory;
  EngineFactory test_engine_factory;

  const EngineFactory& get_factory(Color color) const
  {
    return test_color == color ? test_engine_factory : base_engine_factory;
  }
};

Task<bee::OrError<ExpGameResult>> run_one_game(
  const Span time_per_move, const GameInfo game_info)
{
  co_bail(
    result,
    co_await self_play_one_game(
      game_info.starting_fen,
      time_per_move,
      game_info.get_factory(Color::White),
      game_info.get_factory(Color::Black)));

  co_return ExpGameResult{
    .id = game_info.id,
    .game_result = result.result,
    .test_color = game_info.test_color,
    .moves = result.moves,
    .starting_fen = game_info.starting_fen,
    .final_fen = result.final_fen,
  };
}

string format_double(double value, int decimals, bool force_sign = false)
{
  bool is_negative = false;
  if (value < 0.0) {
    value = -value;
    is_negative = true;
  }
  std::stringstream stream;
  stream << std::fixed << std::setprecision(decimals) << value;

  char sign = is_negative ? '-' : (force_sign ? '+' : 0);
  string out = stream.str();
  if (sign != 0) { out = sign + out; }
  return out;
}

struct Sampler {
 public:
  Sampler(int num_samples)
      : _num_samples(num_samples), rng(Random::create(rand64()))
  {}

  void maybe_add(const string& str)
  {
    _seem_elements++;
    if (int(_sample.size()) < _num_samples) {
      _sample.push_back(str);
    } else {
      int r = rng->rand64() % _seem_elements;
      if (r <= _num_samples) { _sample.at(rng->rand64() % _num_samples) = str; }
    }
  }

  const vector<string>& sample() const { return _sample; }

 private:
  vector<string> _sample;
  const int _num_samples;
  int _seem_elements = 0;
  Random::ptr rng;
};

} // namespace

Deferred<bee::OrError<bee::Unit>> CompareEnginesAsync::compare(
  const string& positions_file,
  int num_rounds,
  int num_workers,
  const string& result_filename,
  const EngineFactory& base_engine_factory,
  const EngineFactory& test_engine_factory,
  Span time_per_move)
{
  randomize_seed();

  print_line("Reading game positions...");
  auto game_infos = make_shared<queue<GameInfo>>();
  {
    Sampler fen_sampler(num_rounds);
    bail(
      positions,
      bee::FileReader::open(bee::FilePath::of_string(positions_file)));
    while (!positions->is_eof()) {
      bail(line, positions->read_line());
      fen_sampler.maybe_add(line);
    }
    auto& fens = fen_sampler.sample();
    for (int i = 0; i < num_rounds; i++) {
      const string& starting_fen = fens[rand64() % fens.size()];

      auto create_game_info = [&i,
                               &starting_fen,
                               &base_engine_factory,
                               &test_engine_factory](Color test_color) {
        return GameInfo{
          .id = i,
          .starting_fen = starting_fen,
          .test_color = test_color,
          .base_engine_factory = base_engine_factory,
          .test_engine_factory = test_engine_factory,
        };
      };

      auto first_game = create_game_info(Color::White);
      auto second_game = create_game_info(Color::Black);

      if (rand64() % 2 == 0) { std::swap(first_game, second_game); }

      game_infos->push(first_game);
      game_infos->push(second_game);
    }
  }
  print_line("Done reading game positions");

  auto results = make_shared<ExpPair<double>>(0.0, 0.0);
  bail(
    experiment_result_unique,
    bee::FileWriter::create(bee::FilePath::of_string(result_filename)));
  auto experiment_result =
    make_shared<bee::FileWriter::ptr>(std::move(experiment_result_unique));
  auto games = make_shared<int>(0);
  return repeat_parallel(
           game_infos->size(),
           num_workers,
           [game_infos,
            results,
            games,
            experiment_result,
            time_per_move]() mutable -> Deferred<bee::OrError<bee::Unit>> {
             auto info = game_infos->front();
             game_infos->pop();
             return run_one_game(time_per_move, info)
               .to_deferred()
               .bind(
                 [results, games, experiment_result](
                   auto&& res_or_error) -> Deferred<bee::OrError<bee::Unit>> {
                   bail(res, res_or_error);
                   auto score = game_result_to_score(res.game_result);
                   results->base += score.score(oponent(res.test_color));
                   results->test += score.score(res.test_color);
                   ++*games;

                   print_line(
                     "game:$ base:$($%) test:$($%) delta:$($%)",
                     *games,
                     format_double(results->base, 1),
                     format_double(results->base / *games * 100.0, 1),
                     format_double(results->test, 1),
                     format_double(results->test / *games * 100.0, 1),
                     format_double(results->test - results->base, 1, true),
                     format_double(
                       (results->test - results->base) / *games * 100.0,
                       1,
                       true));

                   vector<gr::Param> params;

                   params.push_back(gr::Param{
                     .name = "test_played_white",
                     .value = format(res.test_color == Color::White)});

                   string white =
                     res.test_color == Color::White ? "test" : "base";
                   string black =
                     res.test_color == Color::Black ? "test" : "base";

                   auto white_score = score.score(Color::White);
                   auto black_score = score.score(Color::Black);

                   auto game = gr::Game{
                     .id = res.id,
                     .moves = res.moves,
                     .white = {.name = white},
                     .black = {.name = black},
                     .params = params,
                     .white_score = white_score,
                     .black_score = black_score,
                     .starting_fen = res.starting_fen,
                     .final_fen = res.final_fen,
                     .game_result = res.game_result,
                   };
                   must_unit(
                     (*experiment_result)
                       ->write(format("$\n", yasf::Cof::serialize(game))));
                   return bee::unit;
                 });
           })
    .map([](auto&& res) -> bee::OrError<bee::Unit> {
      for (auto& r : res) { bail_unit(r); }
      return bee::ok();
    });
}

} // namespace blackbit
