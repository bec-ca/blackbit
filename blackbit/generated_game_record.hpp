#pragma once

#include "bee/error.hpp"
#include "bee/span.hpp"
#include "bee/time.hpp"
#include "game_result.hpp"
#include "move.hpp"
#include "score.hpp"
#include "yasf/serializer.hpp"
#include "yasf/to_stringable_mixin.hpp"

#include <set>
#include <string>
#include <variant>
#include <vector>

namespace generated_game_record {

struct Param : public yasf::ToStringableMixin<Param> {
  std::string name;
  std::string value;

  static bee::OrError<Param> of_yasf_value(
    const yasf::Value::ptr& config_value);

  yasf::Value::ptr to_yasf_value() const;
};

struct Player : public yasf::ToStringableMixin<Player> {
  std::string name;
  std::optional<std::string> version = std::nullopt;
  std::vector<Param> params = {};

  static bee::OrError<Player> of_yasf_value(
    const yasf::Value::ptr& config_value);

  yasf::Value::ptr to_yasf_value() const;
};

struct MoveInfo : public yasf::ToStringableMixin<MoveInfo> {
  blackbit::Move move;
  std::vector<blackbit::Move> pv = {};
  std::optional<blackbit::Score> evaluation = std::nullopt;
  std::optional<int64_t> depth = std::nullopt;
  std::optional<int64_t> nodes = std::nullopt;
  std::optional<bee::Span> think_time = std::nullopt;

  static bee::OrError<MoveInfo> of_yasf_value(
    const yasf::Value::ptr& config_value);

  yasf::Value::ptr to_yasf_value() const;
};

struct Game : public yasf::ToStringableMixin<Game> {
  std::optional<int64_t> id = std::nullopt;
  std::vector<MoveInfo> moves;
  Player white;
  Player black;
  std::vector<Param> params = {};
  std::optional<double> white_score = std::nullopt;
  std::optional<double> black_score = std::nullopt;
  std::optional<std::string> starting_fen = std::nullopt;
  std::optional<std::string> final_fen = std::nullopt;
  std::optional<blackbit::GameResult> game_result = std::nullopt;

  static bee::OrError<Game> of_yasf_value(const yasf::Value::ptr& config_value);

  yasf::Value::ptr to_yasf_value() const;
};

struct Position : public yasf::ToStringableMixin<Position> {
  std::string fen;
  MoveInfo move_taken;
  MoveInfo next_move_taken;
  Player white;
  Player black;
  std::optional<double> white_score = std::nullopt;
  std::optional<double> black_score = std::nullopt;
  std::optional<blackbit::GameResult> game_result = std::nullopt;
  std::vector<Param> params = {};

  static bee::OrError<Position> of_yasf_value(
    const yasf::Value::ptr& config_value);

  yasf::Value::ptr to_yasf_value() const;
};

struct PCPEntry : public yasf::ToStringableMixin<PCPEntry> {
  std::string fen;
  bee::Span think_time;
  int64_t frequency = 0;
  int64_t ply = 0;
  std::vector<MoveInfo> best_moves;
  bee::Time last_update;
  bee::Time last_start;

  static bee::OrError<PCPEntry> of_yasf_value(
    const yasf::Value::ptr& config_value);

  yasf::Value::ptr to_yasf_value() const;
};

} // namespace generated_game_record

// olint-allow: missing-package-namespace
