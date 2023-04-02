#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <sys/stat.h>

#include "analyze_games.hpp"
#include "bee/file_reader.hpp"
#include "bee/span.hpp"
#include "bee/util.hpp"
#include "benchmark.hpp"
#include "board.hpp"
#include "bot_state.hpp"
#include "command/command_builder.hpp"
#include "communication.hpp"
#include "engine.hpp"
#include "engine_tournament.hpp"
#include "eval_game.hpp"
#include "experiment_runner.hpp"
#include "pcp_generation.hpp"
#include "training.hpp"
#include "view_games.hpp"
#include "view_positions.hpp"
#include "xboard_protocol.hpp"

using bee::print_line;
using bee::Span;
using std::optional;
using std::string;

namespace blackbit {
namespace {

bee::OrError<bee::Unit> analyse_positions(
  optional<string> positions_file_opt,
  optional<int> seconds_per_position_opt,
  bool enable_test)
{
  if (!positions_file_opt) { return bee::Error("Positions file required"); }

  auto positions_file = *positions_file_opt;

  bail(
    positions, bee::FileReader::open(bee::FilePath::of_string(positions_file)));

  auto seconds_per_position = seconds_per_position_opt.value_or(60);

  auto writer = XboardWriter::standard();

  auto experiment =
    enable_test ? Experiment::test_with_seed(0) : Experiment::base();

  auto state = BotState::create(
    writer, experiment, EvalParameters::default_params(), false, 30, nullptr);
  state->set_max_time(Span::of_seconds(seconds_per_position));
  state->set_ponder(false);
  state->set_post(true);

  while (!positions->is_eof()) {
    bail(fen, positions->read_line());
    state->set_fen(fen);
    state->print_board();
    auto move = state->move();
    print_line("Best move: $", move);
  }

  print_line("End of file reached");

  return bee::unit;
}

using command::Cmd;
using command::CommandBuilder;
using command::GroupBuilder;

Cmd analyse_positions_command()
{
  using namespace command::flags;
  auto builder = CommandBuilder("Analyse positions");
  auto positions_file = builder.optional("--positions-file", string_flag);
  auto seconds_per_position =
    builder.optional("--seconds-per-position", int_flag);
  auto enable_test = builder.no_arg("--enable-test");
  return builder.run([=]() {
    return analyse_positions(
      *positions_file, *seconds_per_position, *enable_test);
  });
}

Cmd main_command()
{
  return GroupBuilder("Blackbit")
    .cmd("xboard", XboardProtocol::command())
    .cmd("analyze-positions", analyse_positions_command())
    .cmd("analyze-games", AnalyzeGames::command())
    .cmd("view-games", ViewGames::command())
    .cmd("view-positions", ViewPositions::command())
    .cmd("run-experiment", ExperimentRunner::command())
    .cmd("run-benchmark", Benchmark::command())
    .cmd("run-benchmark-mpv", Benchmark::command_mpv())
    .cmd("eval-game", EvalGame::command())
    .cmd("rl", Training::command())
    .cmd("engine-tournament", EngineTournament::command())
    .cmd("gen-pcp", PCPGeneration::command())
    .build();
}

} // namespace

} // namespace blackbit

int main(int argc, char* argv[])
{
  return blackbit::main_command().main(argc, argv);
}
