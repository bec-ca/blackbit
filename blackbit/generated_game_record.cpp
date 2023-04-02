#include "generated_game_record.hpp"

#include <type_traits>

#include "bee/format.hpp"
#include "bee/util.hpp"
#include "yasf/parser_helpers.hpp"
#include "yasf/serializer.hpp"

using PH = yasf::ParserHelper;

namespace generated_game_record {

////////////////////////////////////////////////////////////////////////////////
// Param
//

bee::OrError<Param> Param::of_yasf_value(const yasf::Value::ptr& value)
{
  if (!value->is_list()) {
    return PH::err("$: Expected list for record", (value));
  }

  std::optional<std::string> output_name;
  std::optional<std::string> output_value;

  for (const auto& element : value->list()) {
    if (!element->is_key_value()) {
      return PH::err("Expected a key value as a record element", element);
    }

    const auto& kv = element->key_value();
    const std::string& name = kv.key;
    if (name == "name") {
      if (output_name.has_value()) {
        return PH::err("Field 'name' is defined more than once", element);
      }
      bail_assign(output_name, yasf::des<std::string>(kv.value));
    } else if (name == "value") {
      if (output_value.has_value()) {
        return PH::err("Field 'value' is defined more than once", element);
      }
      bail_assign(output_value, yasf::des<std::string>(kv.value));
    } else {
      return PH::err("No such field in record of type Param", element);
    }
  }

  if (!output_name.has_value()) {
    return PH::err("Field 'name' not defined", value);
  }
  if (!output_value.has_value()) {
    return PH::err("Field 'value' not defined", value);
  }

  return Param{
    .name = std::move(*output_name),
    .value = std::move(*output_value),
  };
}

yasf::Value::ptr Param::to_yasf_value() const
{
  std::vector<yasf::Value::ptr> fields;
  PH::push_back_field(fields, yasf::ser<std::string>(name), "name");
  PH::push_back_field(fields, yasf::ser<std::string>(value), "value");
  return yasf::Value::create_list(std::move(fields), std::nullopt);
}

////////////////////////////////////////////////////////////////////////////////
// Player
//

bee::OrError<Player> Player::of_yasf_value(const yasf::Value::ptr& value)
{
  if (!value->is_list()) {
    return PH::err("$: Expected list for record", (value));
  }

  std::optional<std::string> output_name;
  std::optional<std::string> output_version;
  std::optional<std::vector<Param>> output_params;

  for (const auto& element : value->list()) {
    if (!element->is_key_value()) {
      return PH::err("Expected a key value as a record element", element);
    }

    const auto& kv = element->key_value();
    const std::string& name = kv.key;
    if (name == "name") {
      if (output_name.has_value()) {
        return PH::err("Field 'name' is defined more than once", element);
      }
      bail_assign(output_name, yasf::des<std::string>(kv.value));
    } else if (name == "version") {
      if (output_version.has_value()) {
        return PH::err("Field 'version' is defined more than once", element);
      }
      bail_assign(output_version, yasf::des<std::string>(kv.value));
    } else if (name == "params") {
      if (output_params.has_value()) {
        return PH::err("Field 'params' is defined more than once", element);
      }
      bail_assign(output_params, yasf::des<std::vector<Param>>(kv.value));
    } else {
      return PH::err("No such field in record of type Player", element);
    }
  }

  if (!output_name.has_value()) {
    return PH::err("Field 'name' not defined", value);
  }
  if (!output_params.has_value()) { output_params.emplace(); }
  return Player{
    .name = std::move(*output_name),
    .version = std::move(output_version),
    .params = std::move(*output_params),
  };
}

yasf::Value::ptr Player::to_yasf_value() const
{
  std::vector<yasf::Value::ptr> fields;
  PH::push_back_field(fields, yasf::ser<std::string>(name), "name");
  if (version.has_value()) {
    PH::push_back_field(fields, yasf::ser<std::string>(*version), "version");
  }
  if (!params.empty()) {
    PH::push_back_field(
      fields, yasf::ser<std::vector<Param>>(params), "params");
  }
  return yasf::Value::create_list(std::move(fields), std::nullopt);
}

////////////////////////////////////////////////////////////////////////////////
// MoveInfo
//

bee::OrError<MoveInfo> MoveInfo::of_yasf_value(const yasf::Value::ptr& value)
{
  if (!value->is_list()) {
    return PH::err("$: Expected list for record", (value));
  }

  std::optional<blackbit::Move> output_move;
  std::optional<std::vector<blackbit::Move>> output_pv;
  std::optional<blackbit::Score> output_evaluation;
  std::optional<int64_t> output_depth;
  std::optional<int64_t> output_nodes;
  std::optional<bee::Span> output_think_time;

  for (const auto& element : value->list()) {
    if (!element->is_key_value()) {
      return PH::err("Expected a key value as a record element", element);
    }

    const auto& kv = element->key_value();
    const std::string& name = kv.key;
    if (name == "move") {
      if (output_move.has_value()) {
        return PH::err("Field 'move' is defined more than once", element);
      }
      bail_assign(output_move, yasf::des<blackbit::Move>(kv.value));
    } else if (name == "pv") {
      if (output_pv.has_value()) {
        return PH::err("Field 'pv' is defined more than once", element);
      }
      bail_assign(output_pv, yasf::des<std::vector<blackbit::Move>>(kv.value));
    } else if (name == "evaluation") {
      if (output_evaluation.has_value()) {
        return PH::err("Field 'evaluation' is defined more than once", element);
      }
      bail_assign(output_evaluation, yasf::des<blackbit::Score>(kv.value));
    } else if (name == "depth") {
      if (output_depth.has_value()) {
        return PH::err("Field 'depth' is defined more than once", element);
      }
      bail_assign(output_depth, PH::to_int(kv.value));
    } else if (name == "nodes") {
      if (output_nodes.has_value()) {
        return PH::err("Field 'nodes' is defined more than once", element);
      }
      bail_assign(output_nodes, PH::to_int(kv.value));
    } else if (name == "think_time") {
      if (output_think_time.has_value()) {
        return PH::err("Field 'think_time' is defined more than once", element);
      }
      bail_assign(output_think_time, yasf::des<bee::Span>(kv.value));
    } else {
      return PH::err("No such field in record of type MoveInfo", element);
    }
  }

  if (!output_move.has_value()) {
    return PH::err("Field 'move' not defined", value);
  }
  if (!output_pv.has_value()) { output_pv.emplace(); }
  return MoveInfo{
    .move = std::move(*output_move),
    .pv = std::move(*output_pv),
    .evaluation = std::move(output_evaluation),
    .depth = std::move(output_depth),
    .nodes = std::move(output_nodes),
    .think_time = std::move(output_think_time),
  };
}

yasf::Value::ptr MoveInfo::to_yasf_value() const
{
  std::vector<yasf::Value::ptr> fields;
  PH::push_back_field(fields, yasf::ser<blackbit::Move>(move), "move");
  if (!pv.empty()) {
    PH::push_back_field(
      fields, yasf::ser<std::vector<blackbit::Move>>(pv), "pv");
  }
  if (evaluation.has_value()) {
    PH::push_back_field(
      fields, yasf::ser<blackbit::Score>(*evaluation), "evaluation");
  }
  if (depth.has_value()) {
    PH::push_back_field(fields, PH::of_int(*depth), "depth");
  }
  if (nodes.has_value()) {
    PH::push_back_field(fields, PH::of_int(*nodes), "nodes");
  }
  if (think_time.has_value()) {
    PH::push_back_field(
      fields, yasf::ser<bee::Span>(*think_time), "think_time");
  }
  return yasf::Value::create_list(std::move(fields), std::nullopt);
}

////////////////////////////////////////////////////////////////////////////////
// Game
//

bee::OrError<Game> Game::of_yasf_value(const yasf::Value::ptr& value)
{
  if (!value->is_list()) {
    return PH::err("$: Expected list for record", (value));
  }

  std::optional<int64_t> output_id;
  std::optional<std::vector<MoveInfo>> output_moves;
  std::optional<Player> output_white;
  std::optional<Player> output_black;
  std::optional<std::vector<Param>> output_params;
  std::optional<double> output_white_score;
  std::optional<double> output_black_score;
  std::optional<std::string> output_starting_fen;
  std::optional<std::string> output_final_fen;
  std::optional<blackbit::GameResult> output_game_result;

  for (const auto& element : value->list()) {
    if (!element->is_key_value()) {
      return PH::err("Expected a key value as a record element", element);
    }

    const auto& kv = element->key_value();
    const std::string& name = kv.key;
    if (name == "id") {
      if (output_id.has_value()) {
        return PH::err("Field 'id' is defined more than once", element);
      }
      bail_assign(output_id, PH::to_int(kv.value));
    } else if (name == "moves") {
      if (output_moves.has_value()) {
        return PH::err("Field 'moves' is defined more than once", element);
      }
      bail_assign(output_moves, yasf::des<std::vector<MoveInfo>>(kv.value));
    } else if (name == "white") {
      if (output_white.has_value()) {
        return PH::err("Field 'white' is defined more than once", element);
      }
      bail_assign(output_white, Player::of_yasf_value(kv.value));
    } else if (name == "black") {
      if (output_black.has_value()) {
        return PH::err("Field 'black' is defined more than once", element);
      }
      bail_assign(output_black, Player::of_yasf_value(kv.value));
    } else if (name == "params") {
      if (output_params.has_value()) {
        return PH::err("Field 'params' is defined more than once", element);
      }
      bail_assign(output_params, yasf::des<std::vector<Param>>(kv.value));
    } else if (name == "white_score") {
      if (output_white_score.has_value()) {
        return PH::err(
          "Field 'white_score' is defined more than once", element);
      }
      bail_assign(output_white_score, PH::to_float(kv.value));
    } else if (name == "black_score") {
      if (output_black_score.has_value()) {
        return PH::err(
          "Field 'black_score' is defined more than once", element);
      }
      bail_assign(output_black_score, PH::to_float(kv.value));
    } else if (name == "starting_fen") {
      if (output_starting_fen.has_value()) {
        return PH::err(
          "Field 'starting_fen' is defined more than once", element);
      }
      bail_assign(output_starting_fen, yasf::des<std::string>(kv.value));
    } else if (name == "final_fen") {
      if (output_final_fen.has_value()) {
        return PH::err("Field 'final_fen' is defined more than once", element);
      }
      bail_assign(output_final_fen, yasf::des<std::string>(kv.value));
    } else if (name == "game_result") {
      if (output_game_result.has_value()) {
        return PH::err(
          "Field 'game_result' is defined more than once", element);
      }
      bail_assign(
        output_game_result, yasf::des<blackbit::GameResult>(kv.value));
    } else {
      return PH::err("No such field in record of type Game", element);
    }
  }

  if (!output_moves.has_value()) {
    return PH::err("Field 'moves' not defined", value);
  }
  if (!output_white.has_value()) {
    return PH::err("Field 'white' not defined", value);
  }
  if (!output_black.has_value()) {
    return PH::err("Field 'black' not defined", value);
  }
  if (!output_params.has_value()) { output_params.emplace(); }
  return Game{
    .id = std::move(output_id),
    .moves = std::move(*output_moves),
    .white = std::move(*output_white),
    .black = std::move(*output_black),
    .params = std::move(*output_params),
    .white_score = std::move(output_white_score),
    .black_score = std::move(output_black_score),
    .starting_fen = std::move(output_starting_fen),
    .final_fen = std::move(output_final_fen),
    .game_result = std::move(output_game_result),
  };
}

yasf::Value::ptr Game::to_yasf_value() const
{
  std::vector<yasf::Value::ptr> fields;
  if (id.has_value()) { PH::push_back_field(fields, PH::of_int(*id), "id"); }
  PH::push_back_field(fields, yasf::ser<std::vector<MoveInfo>>(moves), "moves");
  PH::push_back_field(fields, (white).to_yasf_value(), "white");
  PH::push_back_field(fields, (black).to_yasf_value(), "black");
  if (!params.empty()) {
    PH::push_back_field(
      fields, yasf::ser<std::vector<Param>>(params), "params");
  }
  if (white_score.has_value()) {
    PH::push_back_field(fields, PH::of_float(*white_score), "white_score");
  }
  if (black_score.has_value()) {
    PH::push_back_field(fields, PH::of_float(*black_score), "black_score");
  }
  if (starting_fen.has_value()) {
    PH::push_back_field(
      fields, yasf::ser<std::string>(*starting_fen), "starting_fen");
  }
  if (final_fen.has_value()) {
    PH::push_back_field(
      fields, yasf::ser<std::string>(*final_fen), "final_fen");
  }
  if (game_result.has_value()) {
    PH::push_back_field(
      fields, yasf::ser<blackbit::GameResult>(*game_result), "game_result");
  }
  return yasf::Value::create_list(std::move(fields), std::nullopt);
}

////////////////////////////////////////////////////////////////////////////////
// Position
//

bee::OrError<Position> Position::of_yasf_value(const yasf::Value::ptr& value)
{
  if (!value->is_list()) {
    return PH::err("$: Expected list for record", (value));
  }

  std::optional<std::string> output_fen;
  std::optional<MoveInfo> output_move_taken;
  std::optional<MoveInfo> output_next_move_taken;
  std::optional<Player> output_white;
  std::optional<Player> output_black;
  std::optional<double> output_white_score;
  std::optional<double> output_black_score;
  std::optional<blackbit::GameResult> output_game_result;
  std::optional<std::vector<Param>> output_params;

  for (const auto& element : value->list()) {
    if (!element->is_key_value()) {
      return PH::err("Expected a key value as a record element", element);
    }

    const auto& kv = element->key_value();
    const std::string& name = kv.key;
    if (name == "fen") {
      if (output_fen.has_value()) {
        return PH::err("Field 'fen' is defined more than once", element);
      }
      bail_assign(output_fen, yasf::des<std::string>(kv.value));
    } else if (name == "move_taken") {
      if (output_move_taken.has_value()) {
        return PH::err("Field 'move_taken' is defined more than once", element);
      }
      bail_assign(output_move_taken, MoveInfo::of_yasf_value(kv.value));
    } else if (name == "next_move_taken") {
      if (output_next_move_taken.has_value()) {
        return PH::err(
          "Field 'next_move_taken' is defined more than once", element);
      }
      bail_assign(output_next_move_taken, MoveInfo::of_yasf_value(kv.value));
    } else if (name == "white") {
      if (output_white.has_value()) {
        return PH::err("Field 'white' is defined more than once", element);
      }
      bail_assign(output_white, Player::of_yasf_value(kv.value));
    } else if (name == "black") {
      if (output_black.has_value()) {
        return PH::err("Field 'black' is defined more than once", element);
      }
      bail_assign(output_black, Player::of_yasf_value(kv.value));
    } else if (name == "white_score") {
      if (output_white_score.has_value()) {
        return PH::err(
          "Field 'white_score' is defined more than once", element);
      }
      bail_assign(output_white_score, PH::to_float(kv.value));
    } else if (name == "black_score") {
      if (output_black_score.has_value()) {
        return PH::err(
          "Field 'black_score' is defined more than once", element);
      }
      bail_assign(output_black_score, PH::to_float(kv.value));
    } else if (name == "game_result") {
      if (output_game_result.has_value()) {
        return PH::err(
          "Field 'game_result' is defined more than once", element);
      }
      bail_assign(
        output_game_result, yasf::des<blackbit::GameResult>(kv.value));
    } else if (name == "params") {
      if (output_params.has_value()) {
        return PH::err("Field 'params' is defined more than once", element);
      }
      bail_assign(output_params, yasf::des<std::vector<Param>>(kv.value));
    } else {
      return PH::err("No such field in record of type Position", element);
    }
  }

  if (!output_fen.has_value()) {
    return PH::err("Field 'fen' not defined", value);
  }
  if (!output_move_taken.has_value()) {
    return PH::err("Field 'move_taken' not defined", value);
  }
  if (!output_next_move_taken.has_value()) {
    return PH::err("Field 'next_move_taken' not defined", value);
  }
  if (!output_white.has_value()) {
    return PH::err("Field 'white' not defined", value);
  }
  if (!output_black.has_value()) {
    return PH::err("Field 'black' not defined", value);
  }
  if (!output_params.has_value()) { output_params.emplace(); }
  return Position{
    .fen = std::move(*output_fen),
    .move_taken = std::move(*output_move_taken),
    .next_move_taken = std::move(*output_next_move_taken),
    .white = std::move(*output_white),
    .black = std::move(*output_black),
    .white_score = std::move(output_white_score),
    .black_score = std::move(output_black_score),
    .game_result = std::move(output_game_result),
    .params = std::move(*output_params),
  };
}

yasf::Value::ptr Position::to_yasf_value() const
{
  std::vector<yasf::Value::ptr> fields;
  PH::push_back_field(fields, yasf::ser<std::string>(fen), "fen");
  PH::push_back_field(fields, (move_taken).to_yasf_value(), "move_taken");
  PH::push_back_field(
    fields, (next_move_taken).to_yasf_value(), "next_move_taken");
  PH::push_back_field(fields, (white).to_yasf_value(), "white");
  PH::push_back_field(fields, (black).to_yasf_value(), "black");
  if (white_score.has_value()) {
    PH::push_back_field(fields, PH::of_float(*white_score), "white_score");
  }
  if (black_score.has_value()) {
    PH::push_back_field(fields, PH::of_float(*black_score), "black_score");
  }
  if (game_result.has_value()) {
    PH::push_back_field(
      fields, yasf::ser<blackbit::GameResult>(*game_result), "game_result");
  }
  if (!params.empty()) {
    PH::push_back_field(
      fields, yasf::ser<std::vector<Param>>(params), "params");
  }
  return yasf::Value::create_list(std::move(fields), std::nullopt);
}

////////////////////////////////////////////////////////////////////////////////
// PCPEntry
//

bee::OrError<PCPEntry> PCPEntry::of_yasf_value(const yasf::Value::ptr& value)
{
  if (!value->is_list()) {
    return PH::err("$: Expected list for record", (value));
  }

  std::optional<std::string> output_fen;
  std::optional<bee::Span> output_think_time;
  std::optional<int64_t> output_frequency;
  std::optional<int64_t> output_ply;
  std::optional<std::vector<MoveInfo>> output_best_moves;
  std::optional<bee::Time> output_last_update;
  std::optional<bee::Time> output_last_start;

  for (const auto& element : value->list()) {
    if (!element->is_key_value()) {
      return PH::err("Expected a key value as a record element", element);
    }

    const auto& kv = element->key_value();
    const std::string& name = kv.key;
    if (name == "fen") {
      if (output_fen.has_value()) {
        return PH::err("Field 'fen' is defined more than once", element);
      }
      bail_assign(output_fen, yasf::des<std::string>(kv.value));
    } else if (name == "think_time") {
      if (output_think_time.has_value()) {
        return PH::err("Field 'think_time' is defined more than once", element);
      }
      bail_assign(output_think_time, yasf::des<bee::Span>(kv.value));
    } else if (name == "frequency") {
      if (output_frequency.has_value()) {
        return PH::err("Field 'frequency' is defined more than once", element);
      }
      bail_assign(output_frequency, PH::to_int(kv.value));
    } else if (name == "ply") {
      if (output_ply.has_value()) {
        return PH::err("Field 'ply' is defined more than once", element);
      }
      bail_assign(output_ply, PH::to_int(kv.value));
    } else if (name == "best_moves") {
      if (output_best_moves.has_value()) {
        return PH::err("Field 'best_moves' is defined more than once", element);
      }
      bail_assign(
        output_best_moves, yasf::des<std::vector<MoveInfo>>(kv.value));
    } else if (name == "last_update") {
      if (output_last_update.has_value()) {
        return PH::err(
          "Field 'last_update' is defined more than once", element);
      }
      bail_assign(output_last_update, yasf::des<bee::Time>(kv.value));
    } else if (name == "last_start") {
      if (output_last_start.has_value()) {
        return PH::err("Field 'last_start' is defined more than once", element);
      }
      bail_assign(output_last_start, yasf::des<bee::Time>(kv.value));
    } else {
      return PH::err("No such field in record of type PCPEntry", element);
    }
  }

  if (!output_fen.has_value()) {
    return PH::err("Field 'fen' not defined", value);
  }
  if (!output_think_time.has_value()) {
    return PH::err("Field 'think_time' not defined", value);
  }
  if (!output_frequency.has_value()) {
    return PH::err("Field 'frequency' not defined", value);
  }
  if (!output_ply.has_value()) {
    return PH::err("Field 'ply' not defined", value);
  }
  if (!output_best_moves.has_value()) {
    return PH::err("Field 'best_moves' not defined", value);
  }
  if (!output_last_update.has_value()) {
    return PH::err("Field 'last_update' not defined", value);
  }
  if (!output_last_start.has_value()) {
    return PH::err("Field 'last_start' not defined", value);
  }

  return PCPEntry{
    .fen = std::move(*output_fen),
    .think_time = std::move(*output_think_time),
    .frequency = std::move(*output_frequency),
    .ply = std::move(*output_ply),
    .best_moves = std::move(*output_best_moves),
    .last_update = std::move(*output_last_update),
    .last_start = std::move(*output_last_start),
  };
}

yasf::Value::ptr PCPEntry::to_yasf_value() const
{
  std::vector<yasf::Value::ptr> fields;
  PH::push_back_field(fields, yasf::ser<std::string>(fen), "fen");
  PH::push_back_field(fields, yasf::ser<bee::Span>(think_time), "think_time");
  PH::push_back_field(fields, PH::of_int(frequency), "frequency");
  PH::push_back_field(fields, PH::of_int(ply), "ply");
  PH::push_back_field(
    fields, yasf::ser<std::vector<MoveInfo>>(best_moves), "best_moves");
  PH::push_back_field(fields, yasf::ser<bee::Time>(last_update), "last_update");
  PH::push_back_field(fields, yasf::ser<bee::Time>(last_start), "last_start");
  return yasf::Value::create_list(std::move(fields), std::nullopt);
}

} // namespace generated_game_record

// olint-allow: missing-package-namespace
