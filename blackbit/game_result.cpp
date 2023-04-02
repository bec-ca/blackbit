#include "game_result.hpp"

using blackbit::GameResult;
using std::pair;
using std::string;
using std::vector;

namespace {

vector<pair<GameResult, string>> game_result_repr = {
  {GameResult::WhiteWon, "WhiteWon"},
  {GameResult::BlackWon, "BlackWon"},
  {GameResult::Draw, "Draw"},
  {GameResult::NotFinished, "NotFinished"},
};

}

namespace blackbit {

GameResult game_result_from_winner(Color color)
{
  switch (color) {
  case Color::White:
    return GameResult::WhiteWon;
  case Color::Black:
    return GameResult::BlackWon;
  case Color::None:
    assert(false);
  }
  assert(false);
}

} // namespace blackbit

namespace bee {

std::string to_string<GameResult>::convert(GameResult result)
{
  for (const auto& p : game_result_repr) {
    if (p.first == result) return p.second;
  }
  assert(false && "This should not happen");
}

} // namespace bee

namespace yasf {

Value::ptr Serialize<GameResult>::serialize(const GameResult& value)
{
  return ser<std::string>(bee::format(value));
}

bee::OrError<GameResult> Serialize<GameResult>::deserialize(
  const Value::ptr& value)
{
  bail(str, des<std::string>(value), "Parsing game result");
  for (const auto& p : game_result_repr) {
    if (p.second == str) return p.first;
  }
  shot("Invalid game result value: $", str);
}

} // namespace yasf
