#include "experiment_runner.hpp"

#include "compare_engines.hpp"
#include "random.hpp"
#include "self_play.hpp"

#include "bee/span.hpp"
#include "command/command_builder.hpp"

#include <sstream>

using std::string;

namespace blackbit {
namespace {

bee::OrError<bee::Unit> run_experiment_main(
  const string& positions_file,
  double seconds_per_move,
  int num_rounds,
  int num_workers,
  int repeat_position,
  const string& result_filename)
{
  auto rng = Random::create(rand64());

  auto create_base_params = []() {
    return EngineParams{
      .experiment = Experiment::base(),
      .eval_params = EvalParameters::default_params(),
    };
  };

  auto create_test_params = [rng]() {
    return EngineParams{
      .experiment = Experiment::test(*rng),
      .eval_params = EvalParameters::default_params(),
    };
  };

  return CompareEngines::compare(
    positions_file,
    seconds_per_move,
    num_rounds,
    num_workers,
    repeat_position,
    50,
    result_filename,
    create_base_params,
    create_test_params);
}

} // namespace

command::Cmd ExperimentRunner::command()
{
  using namespace command::flags;
  auto builder = command::CommandBuilder("Run an experiment with the engine");
  auto positions_file = builder.required("--positions-file", string_flag);
  auto seconds_per_move =
    builder.optional_with_default("--seconds-per-move", float_flag, 2.0);
  auto num_rounds =
    builder.optional_with_default("--num-rounds", int_flag, 400);
  auto num_workers =
    builder.optional_with_default("--num-workers", int_flag, 4);
  auto result_filename =
    builder.optional_with_default("--result-file", string_flag, "output.csv");
  auto repeat_position =
    builder.optional_with_default("--repeat-position", int_flag, 1);
  return builder.run([=]() {
    return run_experiment_main(
      *positions_file,
      *seconds_per_move,
      *num_rounds,
      *num_workers,
      *repeat_position,
      *result_filename);
  });
}

} // namespace blackbit
