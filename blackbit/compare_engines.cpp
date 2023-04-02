#include "compare_engines.hpp"

#include "engine.hpp"
#include "generated_game_record.hpp"
#include "parallel_map.hpp"
#include "random.hpp"
#include "rules.hpp"
#include "self_play.hpp"

#include "bee/file_reader.hpp"
#include "bee/file_writer.hpp"
#include "bee/format_vector.hpp"
#include "bee/sampler.hpp"
#include "bee/span.hpp"
#include "bee/string_util.hpp"
#include "bee/time_block.hpp"
#include "bee/util.hpp"
#include "bif/array.hpp"
#include "bif/bifer.hpp"
#include "bif/debif.hpp"
#include "bif/string.hpp"
#include "yasf/cof.hpp"

#include <iomanip>
#include <random>
#include <sstream>

using bee::format;
using bee::print_line;
using bee::Sampler;
using bee::Span;
using bee::to_vector;
using std::function;
using std::map;
using std::mt19937;
using std::random_device;
using std::set;
using std::string;
using std::vector;

namespace blackbit {

namespace {

template <class T> struct ExpPair {
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
  map<string, int> flags;
};

struct GameInfo {
  int id;
  string starting_fen;
  Color test_color;
  EngineParams base_params;
  EngineParams test_params;
  Span time_per_move;
  size_t hash_size;
  int max_depth;
  bool clear_cache_before_move;
};

ExpGameResult run_one_game(const GameInfo& game_info)
{
  auto result = self_play_one_game({
    .starting_fen = game_info.starting_fen,
    .white_params = game_info.test_color == Color::White
                    ? game_info.test_params
                    : game_info.base_params,
    .black_params = game_info.test_color == Color::Black
                    ? game_info.test_params
                    : game_info.base_params,
    .time_per_move = game_info.time_per_move,
    .hash_size = game_info.hash_size,
    .max_depth = game_info.max_depth,
    .clear_cache_before_move = game_info.clear_cache_before_move,
  });

  return ExpGameResult{
    .id = game_info.id,
    .game_result = result.result,
    .test_color = game_info.test_color,
    .moves = result.moves,
    .starting_fen = game_info.starting_fen,
    .final_fen = result.final_fen,
    .flags = game_info.test_params.experiment.flags_to_values(),
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

bee::OrError<vector<string>> load_positions_from_bif(
  const string& filename, int num_positions, mt19937& prng)
{
  bail(
    content,
    bee::FileReader::read_file_bytes(bee::FilePath::of_string(filename)));
  auto bifer = make_shared<bif::Bifer>(std::move(content));
  auto array = bif::debif<bif::Array<bif::String>>(bifer);
  set<string> positions;
  std::uniform_int_distribution<int> dist(0, array.size() - 1);
  while (std::ssize(positions) < num_positions) {
    positions.insert(array[dist(prng)]);
  }
  return to_vector(positions);
}

bee::OrError<vector<string>> load_positions_from_txt(
  const string& filename, int num_positions, int seed)
{
  Sampler<string> fen_sampler(num_positions, seed);
  bail(positions, bee::FileReader::open(bee::FilePath::of_string(filename)));
  while (!positions->is_eof()) {
    bail(line, positions->read_line());
    fen_sampler.maybe_add(line);
  }
  return std::move(fen_sampler.sample());
}

bee::OrError<vector<string>> load_positions(
  const string& filename, int num_positions, mt19937& prng)
{
  bee::TimeBlock tb(
    [](Span span) { print_line("Loading positions took: $", span); });
  print_line("Loading positions...");
  if (filename.ends_with(".bif")) {
    return load_positions_from_bif(filename, num_positions, prng);
  } else {
    return load_positions_from_txt(filename, num_positions, prng());
  }
}

} // namespace

bee::OrError<bee::Unit> CompareEngines::compare(
  const string& positions_file,
  double seconds_per_move,
  const int num_rounds,
  const int num_workers,
  const int repeat_position,
  const int max_depth,
  const string& result_filename,
  const function<EngineParams()>& create_base_params,
  const function<EngineParams()>& create_test_params)
{
  randomize_seed();

  auto time_per_move = Span::of_seconds(seconds_per_move);

  const size_t hash_size = 1 << 24;
  const bool clear_cache_before_move = true;

  mt19937 prng((random_device()()));

  vector<GameInfo> game_infos;
  {
    bail(fens, load_positions(positions_file, num_rounds, prng));

    shuffle(fens.begin(), fens.end(), prng);

    game_infos.reserve(num_rounds * 2);
    int id = 0;
    for (const auto& fen : fens) {
      for (int k = 0; k < repeat_position; k++) {
        id++;
        auto base_params = create_base_params();
        auto test_params = create_test_params();

        auto create_game_info = [&](Color test_color) {
          return GameInfo{
            .id = id,
            .starting_fen = fen,
            .test_color = test_color,
            .base_params = base_params,
            .test_params = test_params,
            .time_per_move = time_per_move,
            .hash_size = hash_size,
            .max_depth = max_depth,
            .clear_cache_before_move = clear_cache_before_move,
          };
        };

        auto first_color = rand64() % 2 == 0 ? Color::White : Color::Black;

        auto first_game = create_game_info(first_color);
        auto second_game = create_game_info(oponent(first_color));

        game_infos.push_back(first_game);
        game_infos.push_back(second_game);
      }
    }
  }

  ExpPair<double> result{0, 0};
  bail(
    experiment_result,
    bee::FileWriter::create(bee::FilePath::of_string(result_filename)));
  int games = 0;
  auto pm = parallel_map::go(game_infos, num_workers, run_one_game);
  for (auto& res : pm) {
    if (res.game_result == GameResult::NotFinished) {
      print_line("Got unfinished game");
      continue;
    }
    auto score = game_result_to_score(res.game_result);
    result.base += score.score(oponent(res.test_color));
    result.test += score.score(res.test_color);
    games++;

    print_line(
      "game:$ base:$($%) test:$($%) delta:$($%)",
      games,
      format_double(result.base, 1),
      format_double(result.base / games * 100.0, 1),
      format_double(result.test, 1),
      format_double(result.test / games * 100.0, 1),
      format_double(result.test - result.base, 1, true),
      format_double((result.test - result.base) / games * 100.0, 1, true));

    if (experiment_result != nullptr) {
      vector<gr::Param> params;
      for (const auto& flag : res.flags) {
        params.push_back(
          gr::Param{.name = flag.first, .value = format(flag.second)});
      }

      params.push_back(gr::Param{
        .name = "test_played_white",
        .value = format(res.test_color == Color::White)});

      string white = res.test_color == Color::White ? "test" : "base";
      string black = res.test_color == Color::Black ? "test" : "base";

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
        experiment_result->write(format("$\n", yasf::Cof::serialize(game))));
    }
  }

  return bee::unit;
}

} // namespace blackbit
