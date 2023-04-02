#include "yasf/core_types.hpp"
#include "yasf/generator.hpp"
#include "yasf/generator_main_lib.hpp"

using namespace yasf;

namespace blackbit {
namespace {

Definitions create_def()
{
  using namespace types;
  constexpr auto move = ext("blackbit::Move", "move.hpp");
  constexpr auto score = ext("blackbit::Score", "score.hpp");
  constexpr auto game_result = ext("blackbit::GameResult", "game_result.hpp");

  constexpr auto param = record(
    "Param", fields(required_field("name", str), required_field("value", str)));

  constexpr auto player = record(
    "Player",
    fields(
      required_field("name", str),
      optional_field("version", str),
      optional_field("params", vec(param))));

  constexpr auto move_info = record(
    "MoveInfo",
    fields(
      required_field("move", move),
      optional_field("pv", vec(move)),
      optional_field("evaluation", score),
      optional_field("depth", int_type),
      optional_field("nodes", int_type),
      optional_field("think_time", Span)));
  constexpr auto move_info_vector = vec(move_info);

  constexpr auto game = record(
    "Game",
    fields(
      optional_field("id", int_type),
      required_field("moves", move_info_vector),
      required_field("white", player),
      required_field("black", player),
      optional_field("params", vec(param)),
      optional_field("white_score", float_type),
      optional_field("black_score", float_type),
      optional_field("starting_fen", str),
      optional_field("final_fen", str),
      optional_field("game_result", game_result)));

  constexpr auto position = record(
    "Position",
    fields(
      required_field("fen", str),
      required_field("move_taken", move_info),
      required_field("next_move_taken", move_info),
      required_field("white", player),
      required_field("black", player),
      optional_field("white_score", float_type),
      optional_field("black_score", float_type),
      optional_field("game_result", game_result),
      optional_field("params", vec(param))));

  constexpr auto opening_entry = record(
    "PCPEntry",
    fields(
      required_field("fen", str),
      required_field("think_time", Span),
      required_field("frequency", int_type),
      required_field("ply", int_type),
      required_field("best_moves", move_info_vector),
      required_field("last_update", Time),
      required_field("last_start", Time)));

  return Definitions{
    .types =
      {
        param,
        player,
        move_info,
        game,
        position,
        opening_entry,
      },
  };
}

} // namespace
} // namespace blackbit

Definitions create_def() { return blackbit::create_def(); }
