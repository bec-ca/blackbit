#include "external_engine.hpp"

#include "bee/format.hpp"
#include "bee/string_util.hpp"

using bee::format;
using bee::print_line;
using bee::Span;
using std::deque;
using std::make_unique;
using std::queue;
using std::string;
using std::vector;

namespace blackbit {

struct XboardEngineProtocol : public EngineProtocol {
 public:
  XboardEngineProtocol() {}
  virtual ~XboardEngineProtocol() {}

  virtual bee::OrError<bee::Unit> set_fen(const string& fen)
  {
    return _interface->send_cmd(format("setboard $", fen));
  }

  virtual bee::OrError<bee::Unit> set_time_per_move(const Span time_per_move)
  {
    return _interface->send_cmd(
      format("st $", time_per_move.to_float_seconds()));
  }

  virtual bee::OrError<bee::Unit> user_move(Move move)
  {
    return _interface->send_cmd(format("usermove $", move.to_string()));
  }

  virtual bee::OrError<bee::Unit> request_move()
  {
    return _interface->send_cmd("go");
  }

  virtual bee::OrError<bee::Unit> initialize()
  {
    bail_unit(_interface->send_cmd("xboard"));
    bail_unit(_interface->send_cmd("protover 2"));
    bail_unit(_interface->send_cmd("new"));
    bail_unit(_interface->send_cmd("force"));
    return bee::ok();
  }

  virtual bee::OrError<bee::Unit> request_close()
  {
    return _interface->send_cmd("quit");
  }

  virtual void handle_command(const string& cmd)
  {
    deque<string> q;
    {
      auto parts = bee::split(cmd, " ");
      if (parts.empty()) return;
      q = deque<string>(parts.begin(), parts.end());
    }
    string command = std::move(q.front());
    q.pop_front();
    if (command == "") {
      // ignore
    } else if (command == "feature") {
      for (auto& feature : q) {
        auto parts = bee::split(feature, "=");
        if (parts.size() != 2) { continue; }
        const string& feature_name = parts[0];
        if (feature_name == "myname") {
          _interface->set_engine_name(parts[1]);
        } else {
          // ignore
        }
      }
    } else if (command == "move" && q.size() == 1) {
      auto ret = _interface->send_cmd("force");
      if (ret.is_error()) {
        _interface->handle_move(ret.error());
      } else {
        auto move_str = q.front();
        _interface->handle_move(move_str);
      }
    } else {
      print_line("Got unexpected command from engine: '$'", cmd);
    }
  }
};

struct UCIEngineProtocol : public EngineProtocol {
 public:
  UCIEngineProtocol() : _starting_fen(Board::initial_fen()) {}

  virtual ~UCIEngineProtocol() {}

  virtual bee::OrError<bee::Unit> initialize()
  {
    return _interface->send_cmd("uci");
  }

  virtual bee::OrError<bee::Unit> set_fen(const string& fen)
  {
    _starting_fen = fen;
    return bee::ok();
  }

  virtual bee::OrError<bee::Unit> set_time_per_move(const Span time_per_move)
  {
    _time_per_move = time_per_move;
    return bee::ok();
  }

  virtual bee::OrError<bee::Unit> user_move(Move move)
  {
    _add_move(move);
    return bee::ok();
  }

  virtual bee::OrError<bee::Unit> request_move()
  {
    string moves = bee::join(_moves, " ");
    bail_unit(
      _send_cmd(format("position fen $ moves $", _starting_fen, moves)));
    bail_unit(_send_cmd(format("go movetime $", _time_per_move.to_millis())));
    return bee::ok();
  }

  virtual bee::OrError<bee::Unit> request_close()
  {
    return _interface->send_cmd("quit");
  }

  void handle_command(const string& cmd)
  {
    auto parts = bee::split(cmd, " ");
    if (parts.empty()) return;
    string command = parts[0];
    if (command == "") {
      // ignore
    } else if (command == "id") {
      // ignore
    } else if (command == "uciok") {
      _interface->send_cmd("isready");
    } else if (command == "info") {
      // ignore
    } else if (command == "option") {
      // ignore
    } else if (command == "readyok") {
      _set_engine_ready();
    } else if (command == "bestmove" && parts.size() >= 2) {
      auto move_str = parts[1];
      _moves.push_back(move_str);
      _interface->handle_move(move_str);
    } else {
      // Unknown command, should ignore
    }
  }

 private:
  void _add_move(Move move) { _moves.push_back(move.to_string()); }
  void _set_engine_ready()
  {
    _is_engine_ready = true;
    while (!_commands.empty()) {
      // TODO: dont ignore error
      _interface->send_cmd(_commands.front());
      _commands.pop();
    }
  }

  bee::OrError<bee::Unit> _send_cmd(string&& cmd)
  {
    if (_is_engine_ready) {
      return _interface->send_cmd(cmd);
    } else {
      _commands.push(std::move(cmd));
      return bee::ok();
    }
  }

  string _starting_fen;
  vector<string> _moves;
  bool _is_engine_ready = false;

  Span _time_per_move = Span::of_seconds(1.0);

  queue<string> _commands;
};

EngineProtocol::ptr create_xboard_client_protocol()
{
  return make_unique<XboardEngineProtocol>();
}

EngineProtocol::ptr create_uci_client_protocol()
{
  return make_unique<UCIEngineProtocol>();
}

} // namespace blackbit
