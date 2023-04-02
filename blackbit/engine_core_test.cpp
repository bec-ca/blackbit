#include "engine_core.hpp"

#include "eval.hpp"
#include "transposition_table.hpp"

#include "bee/format_optional.hpp"
#include "bee/testing.hpp"

using bee::print_line;
using std::atomic_bool;
using std::make_shared;

namespace blackbit {
namespace {

auto make_engine()
{
  Board board;
  board.set_initial();
  auto hash_table = make_shared<TranspositionTable>(100);
  auto move_history = make_shared<MoveHistory>();
  auto should_stop = make_shared<atomic_bool>(false);

  return EngineCore::create(
    board,
    hash_table,
    move_history,
    nullptr,
    false,
    should_stop,
    Experiment::base(),
    EvalParameters::default_params());
}

TEST(pv)
{
  auto core = make_engine();
  auto result = core->search_one_depth(4, Score::min(), Score::max());
  print_line(result);
}

TEST(mpv)
{
  auto core = make_engine();
  auto result = core->search_one_depth_mpv(4, 5, Score::min(), Score::max());
  print_line(result);
}

} // namespace
} // namespace blackbit
