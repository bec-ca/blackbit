#pragma once

#include "bee/error.hpp"
#include "board.hpp"
#include "engine_interface.hpp"

#include <memory>

namespace blackbit {

struct EngineStateInterface {
 public:
  using ptr = std::shared_ptr<EngineStateInterface>;

  virtual bee::OrError<bee::Unit> send_cmd(const std::string& command) = 0;
  virtual void handle_move(bee::OrError<std::string>&& move) = 0;
  virtual void set_engine_name(const std::string& name) = 0;
  virtual const Board& board() const = 0;
};

struct EngineProtocol {
 public:
  using ptr = std::unique_ptr<EngineProtocol>;

  virtual ~EngineProtocol() {}

  void set_interface(const EngineStateInterface::ptr& interface)
  {
    _interface = interface;
  }

  virtual bee::OrError<bee::Unit> set_fen(const std::string& fen) = 0;
  virtual bee::OrError<bee::Unit> set_time_per_move(
    const bee::Span time_per_move) = 0;
  virtual bee::OrError<bee::Unit> user_move(Move move) = 0;
  virtual bee::OrError<bee::Unit> initialize() = 0;
  virtual bee::OrError<bee::Unit> request_move() = 0;
  virtual bee::OrError<bee::Unit> request_close() = 0;
  virtual void handle_command(const std::string& cmd) = 0;

 protected:
  EngineStateInterface::ptr _interface;
};

bee::OrError<EngineInterface::ptr> create_external_engine(
  const std::string& engine_cmd, EngineProtocol::ptr&& protocol);

} // namespace blackbit
