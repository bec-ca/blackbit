#pragma once

#include <cstdint>
#include <string>

#include "communication.hpp"
#include "eval.hpp"
#include "experiment_framework.hpp"
#include "pcp.hpp"

namespace blackbit {

struct BotState {
  virtual ~BotState();

  virtual void reset() = 0;

  virtual void set_ponder(bool ponder) = 0;
  virtual bool get_ponder() const = 0;
  virtual void set_post(bool post) = 0;
  virtual void set_max_depth(int max_depth) = 0;
  virtual void set_fen(const std::string& fen) = 0;
  virtual std::string get_fen() const = 0;
  virtual void set_time_control(int mps, bee::Span base, bee::Span inc) = 0;
  virtual void set_max_time(bee::Span max_time) = 0;
  virtual void undo() = 0;
  virtual Color get_turn() const = 0;
  virtual void set_time_remaining(bee::Span time_remaining) = 0;
  virtual bee::Span get_time_remaining() const = 0;

  virtual bee::OrError<Move> move() = 0;

  virtual bee::OrError<bee::Unit> user_move(const std::string& move_str) = 0;
  virtual bee::OrError<bee::Unit> user_move(const Move& move) = 0;

  virtual void print_board() const = 0;
  virtual bool is_over() const = 0;

  virtual void tear_down() = 0;

  static std::shared_ptr<BotState> create(
    const XboardWriter::ptr& writer,
    const Experiment& experiment,
    const EvalParameters& eval_params,
    bool use_mpv,
    int cache_size,
    PCP::ptr&& pcp);
};

} // namespace blackbit
