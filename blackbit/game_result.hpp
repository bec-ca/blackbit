#pragma once

#include "color.hpp"

#include "bee/to_string.hpp"
#include "yasf/serializer.hpp"

#include <cassert>
#include <string>

namespace blackbit {

enum class GameResult {
  WhiteWon,
  BlackWon,
  Draw,
  NotFinished,
};

GameResult game_result_from_winner(Color color);

} // namespace blackbit

namespace bee {
template <> struct to_string<blackbit::GameResult> {
  static std::string convert(blackbit::GameResult result);
};
} // namespace bee

namespace yasf {

template <> struct Serialize<blackbit::GameResult> {
  static Value::ptr serialize(const blackbit::GameResult& value);
  static bee::OrError<blackbit::GameResult> deserialize(
    const Value::ptr& value);
};

} // namespace yasf
