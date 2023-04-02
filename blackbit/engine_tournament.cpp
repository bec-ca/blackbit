#include "engine_tournament.hpp"

#include "external_engine_protocols.hpp"
#include "generated_game_record.hpp"
#include "random.hpp"
#include "self_play_async.hpp"

#include "async/async_command.hpp"
#include "async/deferred_awaitable.hpp"
#include "bee/file_reader.hpp"
#include "bee/file_writer.hpp"
#include "bee/format_vector.hpp"
#include "bee/pretty_print.hpp"
#include "bee/sampler.hpp"
#include "bee/string_util.hpp"
#include "yasf/config_parser.hpp"
#include "yasf/serializer.hpp"

#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>

using namespace async;

using bee::PrettyPrint;
using bee::print_line;
using bee::Sampler;
using bee::Span;
using std::make_shared;
using std::map;
using std::mt19937;
using std::queue;
using std::random_device;
using std::string;
using std::vector;

namespace blackbit {
namespace {

template <class T> struct ExpPair {
  ExpPair(T&& base, T&& test) : base(move(base)), test(move(test)) {}

  T base;
  T test;
};

struct EngineSpec {
  EngineFactory factory;
  string name;
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
  GameResult game_result;
  GameEndReason end_reason;
  string white_engine_name;
  string black_engine_name;
  vector<gr::MoveInfo> moves;
  string starting_fen;
  string final_fen;

  const string& engine_name(Color color) const
  {
    switch (color) {
    case Color::White:
      return white_engine_name;
    case Color::Black:
      return black_engine_name;
    case Color::None:
      assert(false);
    }
  }
};

struct GameInfo {
  string starting_fen;

  EngineSpec white_engine_spec;
  EngineSpec black_engine_spec;

  const EngineFactory& get_factory(Color color) const
  {
    return color == Color::White ? white_engine_spec.factory
                                 : black_engine_spec.factory;
  }
};

Task<bee::OrError<ExpGameResult>> run_one_game(
  const Span time_per_move, const GameInfo& game_info)
{
  co_bail(
    result,
    co_await self_play_one_game(
      game_info.starting_fen,
      time_per_move,
      game_info.get_factory(Color::White),
      game_info.get_factory(Color::Black)));

  co_return ExpGameResult{
    .game_result = result.result,
    .end_reason = result.end_reason,
    .white_engine_name = game_info.white_engine_spec.name,
    .black_engine_name = game_info.black_engine_spec.name,
    .moves = result.moves,
    .starting_fen = game_info.starting_fen,
    .final_fen = result.final_fen,
  };
}

struct EngineScore {
  double total_score = 0;
  int num_games = 0;

  void add_game(double score)
  {
    num_games++;
    total_score += score;
  }

  double delta_percent() const
  {
    return (total_score / double(num_games) - 0.5) * 100.0;
  }
};

Task<bee::OrError<bee::Unit>> run_tournament(
  const string& positions_file,
  int num_rounds,
  int num_workers,
  const string& result_filename,
  const map<string, EngineSpec>& engine_specs,
  Span time_per_move)
{
  randomize_seed();

  mt19937 prng((random_device()()));

  print_line("Reading game positions...");
  auto game_infos = make_shared<queue<GameInfo>>();
  {
    Sampler<string> fen_sampler(num_rounds, prng());
    co_bail(
      positions,
      bee::FileReader::open(bee::FilePath::of_string(positions_file)));
    while (!positions->is_eof()) {
      co_bail(line, positions->read_line());
      fen_sampler.maybe_add(line);
    }

    auto fens = std::move(fen_sampler.sample());

    shuffle(fens.begin(), fens.end(), prng);

    for (auto& fen : fens) {
      for (auto& [_, white_engine_spec] : engine_specs) {
        for (auto& [_, black_engine_spec] : engine_specs) {
          if (white_engine_spec.name == black_engine_spec.name) { continue; }
          game_infos->push(GameInfo{
            .starting_fen = fen,
            .white_engine_spec = white_engine_spec,
            .black_engine_spec = black_engine_spec,
          });
        }
      }
    }
  }
  print_line("Done reading game positions");

  map<string, map<string, EngineScore>> results;
  co_bail(
    experiment_result_unique,
    bee::FileWriter::create(bee::FilePath::of_string(result_filename)));
  int game_count = 0;
  auto res = co_await repeat_parallel(
    game_infos->size(),
    num_workers,
    [&game_infos, &results, &game_count, &time_per_move]() mutable
    -> Task<bee::OrError<bee::Unit>> {
      auto info = game_infos->front();
      game_infos->pop();
      co_bail(res, co_await run_one_game(time_per_move, info));
      if (res.end_reason == GameEndReason::EngineFailed) {
        print_line(
          "Game ended because engine failed: $ ($ vs $)",
          res.game_result,
          res.white_engine_name,
          res.black_engine_name);
      }
      auto score = game_result_to_score(res.game_result);

      for (auto color : AllColors) {
        results[res.engine_name(color)][res.engine_name(oponent(color))]
          .add_game(score.score(color));
      }

      ++game_count;

      print_line("================================");
      print_line("Game:$", game_count);
      for (auto& [engine, oponents] : results) {
        print_line("-----------");
        print_line("enigne: $", engine);
        for (auto& [op, score] : oponents) {
          print_line(
            "vs $: $/$($%)",
            op,
            PrettyPrint::format_double(score.total_score, 1),
            score.num_games,
            PrettyPrint::format_double(score.delta_percent(), 2));
        }
      }

      co_return bee::unit;
    });
  for (auto& r : res) { co_bail_unit(r); }
  co_return bee::ok();
}

template <class F>
auto create_engine_factory(const string& engine_cmd, F&& create_protocol)
{
  return [engine_cmd, create_protocol = std::forward<F>(create_protocol)]()
           -> bee::OrError<EngineInterface::ptr> {
    return create_external_engine(engine_cmd, create_protocol());
  };
};

bee::OrError<EngineFactory> create_factory(const string& cmd_exp)
{
  if (cmd_exp.empty()) { shot("Command cannot be an empty string"); }
  auto parts = bee::split(cmd_exp, ":");
  bool is_xboard = false;
  string command;

  if (parts.empty()) { shot("Command cannot be an empty string"); }
  if (parts.size() <= 1) {
    is_xboard = true;
    command = parts[0];
  } else if (parts.size() == 2) {
    string engine_type = parts[0];
    command = parts[1];
    if (engine_type == "uci") {
      is_xboard = false;
    } else if (engine_type == "xboard") {
      is_xboard = true;
    } else {
      shot("Unknown engine type: $", engine_type);
    }
  } else {
    shot("Invalid engine spec");
  }

  if (is_xboard) {
    return create_engine_factory(command, create_xboard_client_protocol);
  } else {
    return create_engine_factory(command, create_uci_client_protocol);
  }
}

bee::OrError<map<string, string>> parse_engines_config(const string& filename)
{
  bail(parsed, yasf::ConfigParser::parse_from_file(filename));
  return yasf::des<map<string, string>>(parsed);
}

Task<bee::OrError<bee::Unit>> main(
  const string& engines_config_path,
  int concurrent_games,
  int num_games,
  const string& positions_file,
  const string& results_file,
  double seconds_per_move)
{
  co_bail(engines_config, parse_engines_config(engines_config_path));

  map<string, EngineSpec> engine_specs;
  for (auto& [name, spec] : engines_config) {
    co_bail(factory, create_factory(spec));
    engine_specs.emplace(name, EngineSpec{.factory = factory, .name = name});
  }

  co_return co_await run_tournament(
    positions_file,
    num_games,
    concurrent_games,
    results_file,
    engine_specs,
    Span::of_seconds(seconds_per_move));
}

} // namespace

command::Cmd EngineTournament::command()
{
  using namespace command;
  using namespace command::flags;
  auto builder = CommandBuilder("Compare two external engines");
  auto engines_config = builder.required("--engines-config", string_flag);
  auto concurrent_games =
    builder.optional_with_default("--concurrent-games", int_flag, 16);
  auto num_games = builder.optional_with_default("--num-games", int_flag, 1024);
  auto positions_file = builder.required("--positions", string_flag);
  auto results_file = builder.required("--results", string_flag);
  auto seconds_per_move =
    builder.optional_with_default("--seconds-per-move", float_flag, 1.0);
  return run_coro(builder, [=] {
    return main(
      *engines_config,
      *concurrent_games,
      *num_games,
      *positions_file,
      *results_file,
      *seconds_per_move);
  });
}

} // namespace blackbit
