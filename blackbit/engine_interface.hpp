#pragma once

#include "async/task.hpp"
#include "bee/error.hpp"
#include "bee/time.hpp"
#include "move.hpp"

#include <memory>

namespace blackbit {

struct EngineInterface {
 public:
  using ptr = std::unique_ptr<EngineInterface>;

  virtual ~EngineInterface();
  virtual bee::OrError<bee::Unit> set_fen(const std::string& fen) = 0;
  virtual bee::OrError<bee::Unit> set_time_per_move(
    const bee::Span time_per_move) = 0;
  virtual bee::OrError<bee::Unit> send_move(const Move& m) = 0;
  virtual async::Task<bee::OrError<Move>> find_move() = 0;
  virtual async::Task<bee::Unit> close() = 0;
};

} // namespace blackbit
