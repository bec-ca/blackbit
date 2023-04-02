#include "external_engine.hpp"

#include "async/async.hpp"
#include "async/async_fd.hpp"
#include "async/async_process.hpp"
#include "async/pipe.hpp"
#include "bee/error.hpp"
#include "bee/format.hpp"
#include "bee/sub_process.hpp"
#include "board.hpp"
#include "engine_interface.hpp"
#include "move.hpp"

#include <memory>

using namespace async;

using bee::print_err_line;
using bee::Span;
using bee::SubProcess;
using std::make_unique;
using std::shared_ptr;
using std::string;
using std::unique_ptr;

namespace blackbit {

struct ExternalEngineProcess
    : public std::enable_shared_from_this<ExternalEngineProcess> {
 public:
  ExternalEngineProcess(const string& cmd) : _cmd(cmd)
  {
    assert(_pipe != nullptr);
  }

  ~ExternalEngineProcess() { assert(_pipe == nullptr); }

  bee::OrError<Pipe<bee::OrError<string>>::ptr> start()
  {
    assert(_proc == nullptr);

    SubProcess::Pipe::ptr stdin_pipe = SubProcess::Pipe::create();
    SubProcess::Pipe::ptr stdout_pipe = SubProcess::Pipe::create();

    auto args = SubProcess::CreateProcessArgs{
      .cmd = _cmd, .stdin_spec = stdin_pipe, .stdout_spec = stdout_pipe,
      //.stderr_spec = SubProcess::Filename{"/dev/null"},
    };

    auto ptr = this->shared_from_this();
    bail(proc, AsyncProcess::spawn_process(args, [ptr](int error_code) {
           auto status = error_code == 0
                         ? bee::ok()
                         : bee::Error::format(
                             "Process exited with error. (code:$) (cmd:$)",
                             error_code,
                             ptr->_cmd);
           ptr->tear_down(std::move(status));
         }));

    bail_assign(
      _stdout_fd, AsyncFD::of_fd(bee::copy(stdout_pipe->fd()), false));

    bail_assign(_stdin_fd, AsyncFD::of_fd(bee::copy(stdin_pipe->fd()), false));

    _stdout_fd->set_ready_callback([ptr]() { ptr->_on_ready(); });

    _proc = proc;

    return _pipe;
  }

  bee::OrError<bee::Unit> send_cmd(const string& cmd)
  {
    return _stdin_fd->write(cmd + "\n");
  }

 private:
  void tear_down(bee::OrError<bee::Unit>&& status)
  {
    if (_pipe == nullptr) { assert(false && "Already torn down"); }
    if (status.is_error()) { _pipe->push(status.error()); }
    assert(_stdin_fd->close());
    assert(_stdout_fd->close());
    _stdin_fd = nullptr;
    _stdout_fd = nullptr;
    _pipe->close();
    _pipe = nullptr;
    _proc = nullptr;
  }

  void _handle_data(bee::DataBuffer&& buf)
  {
    if (_pipe == nullptr) { assert(false && "Got data after exited"); }
    _incoming_data.write(std::move(buf));

    while (_pipe != nullptr) {
      auto line = _incoming_data.read_line();
      if (!line.has_value()) { break; }
      _pipe->push(std::move(*line));
    }
  }

  void _on_ready()
  {
    bee::DataBuffer buf;
    auto res = _stdout_fd->read(buf);
    if (res.is_error()) {
      _handle_error(std::move(res.error()));
      return;
    }
    _handle_data(std::move(buf));
  }

  void _handle_error(const bee::Error& err)
  {
    // TODO: handle error better
    print_err_line(err);
  }

  string _cmd;

  Pipe<bee::OrError<string>>::ptr _pipe = Pipe<bee::OrError<string>>::create();

  SubProcess::ptr _proc;

  bee::DataBuffer _incoming_data;

  AsyncFD::ptr _stdout_fd;
  AsyncFD::ptr _stdin_fd;
};

struct ExternalEngineState
    : public std::enable_shared_from_this<ExternalEngineState>,
      public EngineInterface,
      public EngineStateInterface {
 public:
  using ptr = shared_ptr<ExternalEngineState>;

  ExternalEngineState(
    const string& engine_cmd, EngineProtocol::ptr&& engine_protocol)
      : _proc(make_shared<ExternalEngineProcess>(engine_cmd)),
        _board(make_unique<Board>()),
        _result(Ivar<bee::OrError<bee::Unit>>::create()),
        _engine_protocol(std::move(engine_protocol)),
        _engine_cmd(engine_cmd)
  {
    _board->set_initial();
  }

  virtual ~ExternalEngineState() { assert(_proc == nullptr); }

  bee::OrError<bee::Unit> start()
  {
    _engine_protocol->set_interface(shared_from_this());

    bail(pipe, _proc->start());

    bail_unit(_engine_protocol->initialize());

    auto ptr = shared_from_this();
    pipe
      ->iter_values([ptr](bee::OrError<string>&& msg) -> Deferred<bee::Unit> {
        ptr->_handle_command(std::move(msg));
        return bee::unit;
      })
      .to_deferred()
      .iter([ptr](bee::Unit&&) {
        if (ptr->is_torn_down()) {
          return;
        } else if (!ptr->_is_exiting) {
          ptr->_tear_down(bee::Error::format(
            "Engine exited unexpectedly (cmd:$)", ptr->_engine_cmd));
        } else {
          ptr->_tear_down(bee::ok());
        }
      });

    return bee::ok();
  }

  virtual bee::OrError<bee::Unit> set_fen(const string& fen) override
  {
    _board->set_fen(fen);
    return _engine_protocol->set_fen(_board->to_fen());
  }

  virtual bee::OrError<bee::Unit> set_time_per_move(
    const Span time_per_move) override
  {
    return _engine_protocol->set_time_per_move(time_per_move);
  }

  virtual bee::OrError<bee::Unit> send_move(const Move& move) override
  {
    _board->move(move);
    return _engine_protocol->user_move(move);
  }

  virtual Task<bee::OrError<Move>> find_move() override
  {
    if (_move_ivar != nullptr) {
      assert(false && "Was already waiting for a move");
    }
    co_bail_unit(_engine_protocol->request_move());

    _move_ivar = Ivar<bee::OrError<Move>>::create();
    co_return co_await _move_ivar;
  }

  virtual Task<bee::Unit> close() override
  {
    assert(!_is_exiting);
    if (_engine_protocol != nullptr) { _engine_protocol->request_close(); }
    _is_exiting = true;
    co_await _result;
    co_return bee::unit;
  }

  virtual void set_engine_name(const string& name) override
  {
    _engine_name = name;
  }

  virtual const Board& board() const override { return *_board; }

  virtual bee::OrError<bee::Unit> send_cmd(const string& cmd) override
  {
    return _proc->send_cmd(cmd);
  }

  virtual void handle_move(bee::OrError<string>&& move_str_or_error) override
  {
    if (_move_ivar == nullptr) { assert(false && "Was not expecting a move"); }
    auto handle = [&]() -> bee::OrError<Move> {
      bail(move_str, move_str_or_error);
      bail(
        parsed_move,
        _board->parse_xboard_move_string(move_str),
        "Got invalid move from engine (name:$)",
        _engine_name);
      _board->move(parsed_move);
      return parsed_move;
    };
    _move_ivar->resolve(handle());
    _move_ivar = nullptr;
  }

 private:
  bool is_torn_down() const { return _is_torn_down; }

  void _tear_down(bee::OrError<bee::Unit>&& value)
  {
    assert(!_is_torn_down);
    if (_move_ivar != nullptr) {
      if (value.is_error()) {
        _move_ivar->resolve(value.error());
      } else {
        _move_ivar->resolve(
          bee::Error("Attempted to tear down engine while waiting for move"));
      }
      _move_ivar = nullptr;
    }
    _result->resolve(std::move(value));
    _proc = nullptr;
    _engine_protocol = nullptr;
    _is_torn_down = true;
  }

  void _handle_command(bee::OrError<string>&& cmd_or_error)
  {
    if (cmd_or_error.is_error()) {
      auto err = cmd_or_error.error();
      if (_move_ivar != nullptr) {
        _move_ivar->resolve(err);
        _move_ivar = nullptr;
      }
      _result->resolve(std::move(err));
    } else {
      auto& cmd = cmd_or_error.value();
      _engine_protocol->handle_command(cmd);
    }
  }

  shared_ptr<ExternalEngineProcess> _proc;
  Ivar<bee::OrError<Move>>::ptr _move_ivar;
  unique_ptr<Board> _board;
  string _engine_name;
  Ivar<bee::OrError<bee::Unit>>::ptr _result;
  EngineProtocol::ptr _engine_protocol;
  string _engine_cmd;

  bool _is_exiting = false;
  bool _is_torn_down = false;
};

struct SharedEngineInterfaceWrapper : public EngineInterface {
 public:
  using ptr = unique_ptr<SharedEngineInterfaceWrapper>;

  explicit SharedEngineInterfaceWrapper(ExternalEngineState::ptr&& engine)
      : _engine(engine)
  {}

  ~SharedEngineInterfaceWrapper() { assert(_closed); }

  virtual bee::OrError<bee::Unit> set_fen(const std::string& fen) override
  {
    return _engine->set_fen(fen);
  }

  virtual bee::OrError<bee::Unit> set_time_per_move(
    const Span time_per_move) override
  {
    return _engine->set_time_per_move(time_per_move);
  }

  virtual bee::OrError<bee::Unit> send_move(const Move& m) override
  {
    return _engine->send_move(m);
  }

  virtual async::Task<bee::OrError<Move>> find_move() override
  {
    return _engine->find_move();
  }

  virtual async::Task<bee::Unit> close() override
  {
    _closed = true;
    return _engine->close();
  }

  bee::OrError<bee::Unit> start() { return _engine->start(); }

 private:
  ExternalEngineState::ptr _engine;

  bool _closed = false;
};

bee::OrError<EngineInterface::ptr> create_external_engine(
  const std::string& engine_cmd, EngineProtocol::ptr&& protocol)

{
  auto engine = make_unique<SharedEngineInterfaceWrapper>(
    make_shared<ExternalEngineState>(engine_cmd, std::move(protocol)));
  bail_unit(engine->start());
  return engine;
}

} // namespace blackbit
