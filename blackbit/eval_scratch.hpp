#pragma once

#include "bitboard.hpp"
#include "player_pair.hpp"

namespace blackbit {

struct EvalScratch {
  PlayerPair<BitBoard> attacks_bb;
};

} // namespace blackbit
