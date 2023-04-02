#include "score.hpp"

#include "bee/format.hpp"
#include "bee/util.hpp"

#include <vector>

using std::pair;
using std::string;
using std::vector;

namespace blackbit {
namespace {

constexpr uint64_t pow_int(int64_t base, int exp)
{
  if (exp == 0) return 1;
  if (exp == 1) return base;
  if (exp % 2 == 1) {
    return pow_int(base, exp - 1) * base;
  } else {
    auto ret = pow_int(base, exp / 2);
    return ret * ret;
  }
}

string format_ratio(int64_t numerand, int64_t denominator, int num_decimals)
{
  auto format = [&](int64_t numerand) {
    int64_t rem = numerand % denominator;
    int64_t q = numerand / denominator;
    string output = std::to_string(q);

    if (num_decimals > 0) {
      int64_t rem_q = (rem * pow_int(10, num_decimals)) / denominator;
      vector<int> digits;
      for (int i = 0; i < num_decimals; i++) {
        digits.push_back(rem_q % 10);
        rem_q /= 10;
      }
      output += '.';
      for (char c : bee::rev_it(digits)) { output += c + '0'; }
    }

    return output;
  };

  if (numerand >= 0) {
    return "+" + format(numerand);
  } else {
    return "-" + format(-numerand);
  }
}

} // namespace

double Score::to_pawns() const
{
  // if (is_mate()) {
  //   if (is_negative()) {
  //     return -std::numeric_limits<double>::infinity();
  //   } else {
  //     return std::numeric_limits<double>::infinity();
  //   }
  // }
  return _score_milli / static_cast<double>(pawn_value);
}

int Score::to_xboard() const
{
  if (is_mate()) {
    int ret = 100000 + (moves_to_mate() + 1) / 2;
    if (is_negative()) ret = -ret;
    return ret;
  } else {
    return to_centi_pawns();
  }
}

string Score::to_string() const
{
  if (is_mate()) {
    return bee::format("$M $", is_negative() ? "-" : "+", moves_to_mate());
  }
  return format_ratio(_score_milli, pawn_value, 3);
}

yasf::Value::ptr Score::to_yasf_value() const
{
  pair<string, yasf::Value::ptr> p;
  if (is_mate()) {
    int m = moves_to_mate();
    if (is_negative()) m = -m;
    p = make_pair("Mate", yasf::ser<int>(m));
  } else {
    p = make_pair(
      "Pawns",
      yasf::ser<string>(format_ratio(to_milli_pawns(), Score::pawn_value, 3)));
  }
  return ser(p);
}

bee::OrError<Score> Score::of_yasf_value(const yasf::Value::ptr& value)
{
  bail(p, (des<pair<string, yasf::Value::ptr>>(value)));
  if (p.first == "Mate") {
    bail(moves, yasf::des<int>(p.second));
    auto ret = Score::of_moves_to_mate(std::abs(moves));
    if (moves < 0) ret = ret.neg();
    return ret;
  } else if (p.first == "Pawns") {
    bail(pawns, des<double>(p.second));
    return Score::of_pawns(pawns);
  } else {
    return bee::Error::format("Unexpected score type: $", p.first);
  }
}

} // namespace blackbit
