#include "search_result_info.hpp"

#include "bee/util.hpp"
#include "rules.hpp"

#include "bee/format_vector.hpp"

using bee::Span;
using std::make_unique;
using std::string;
using std::unique_ptr;
using std::vector;

namespace blackbit {

////////////////////////////////////////////////////////////////////////////////
// PV
//

bool operator==(const unique_ptr<PV>& pv1, const unique_ptr<PV>& pv2)
{
  if (pv1 == nullptr && pv2 == nullptr) {
    return true;
  } else if (pv1 == nullptr || pv2 == nullptr) {
    return true;
  } else {
    return *pv1 == *pv2;
  }
}

PV::ptr PV::clone() const
{
  auto next_clone = next != nullptr ? next->clone() : nullptr;
  return std::make_unique<PV>(move, std::move(next_clone));
}

PV::~PV() {}

bool PV::operator==(const PV& other) const
{
  return move == other.move && (next == other.next);
}

vector<Move> PV::to_vector() const
{
  vector<Move> out;
  for (auto pv = this; pv; pv = pv->next.get()) { out.push_back(pv->move); }
  return out;
}

PV::ptr PV::of_vector(const std::vector<Move>& moves)
{
  PV::ptr ret;
  for (auto& move : bee::rev_it(moves)) {
    ret = make_unique<PV>(move, std::move(ret));
  }
  return ret;
}

string PV::to_string() const { return bee::format(to_vector()); }

////////////////////////////////////////////////////////////////////////////////
// SearchResultInfo
//

std::vector<std::string> SearchResultInfo::make_pretty_moves(
  const Board& board) const
{
  std::vector<std::string> pretty_moves;
  Board copy = board;
  for (const auto& m : pv) {
    pretty_moves.push_back(Rules::pretty_move(copy, m));
    copy.move(m);
  }
  return pretty_moves;
};

SearchResultInfo::ptr SearchResultInfo::create(
  Move m,
  vector<Move>&& pv,
  Score eval,
  uint64_t nodes,
  int depth,
  Span ellapsed)
{
  return std::make_unique<SearchResultInfo>(SearchResultInfo{
    .best_move = m,
    .eval = eval,
    .pv = std::move(pv),
    .depth = depth,
    .think_time = ellapsed,
    .nodes = nodes,
  });
}

bool SearchResultInfo::operator==(const SearchResultInfo& other) const
{
  return best_move == other.best_move && eval == other.eval && pv == other.pv &&
         depth == other.depth && think_time == other.think_time &&
         nodes == other.nodes;
}

SearchResultInfo::ptr SearchResultInfo::clone() const
{
  return make_unique<SearchResultInfo>(SearchResultInfo{
    .best_move = best_move,
    .eval = eval,
    .pv = pv,
    .depth = depth,
    .think_time = think_time,
    .nodes = nodes,
  });
}

string SearchResultInfo::to_string() const
{
  return bee::format("[m:$ e:$ d:$]", best_move, eval, depth);
}

void SearchResultInfo::flip(Color color)
{
  if (color == Color::Black) eval = -eval;
}

} // namespace blackbit
