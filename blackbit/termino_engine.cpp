#include "termino_engine.hpp"

#include "async/async.hpp"
#include "async/scheduler_context.hpp"
#include "bee/error.hpp"
#include "bee/format_vector.hpp"
#include "bee/string_util.hpp"
#include "engine.hpp"
#include "termino/text_box.hpp"

using namespace async;

using bee::Span;
using std::function;
using std::make_shared;
using std::make_unique;
using std::optional;
using std::shared_ptr;
using std::vector;
using std::weak_ptr;

namespace blackbit {

TerminoEngine::TerminoEngine(
  Engine::ptr&& engine, const shared_ptr<QueueBridge<ResultItem>>& queue_bridge)
    : _engine_info(make_shared<termino::TextBox>()),
      _engine(std::move(engine)),
      _on_update([](auto) {}),
      _queue_bridge(queue_bridge),
      _board(make_unique<Board>())
{}

TerminoEngine::~TerminoEngine()
{
  _post_queue->close();
  if (_post_queue_thread.has_value()) { _post_queue_thread->join(); }
}

bee::OrError<TerminoEngine::ptr> TerminoEngine::create(const PCP::ptr& pcp)
{
  auto engine = Engine::create(
    Experiment::base(), EvalParameters::default_params(), pcp, 1ll << 34, true);

  bail(post_pipe, bee::Pipe::create());

  auto post_read_pipe = post_pipe.read_fd;
  post_read_pipe->set_blocking(false);

  auto queue_bridge = make_shared<QueueBridge<ResultItem>>(post_pipe.write_fd);

  auto termino_engine =
    make_shared<TerminoEngine>(std::move(engine), queue_bridge);

  bail_unit(
    add_fd(post_read_pipe, [termino_engine, post_read_pipe, queue_bridge]() {
      termino_engine->update([&]() -> bee::OrError<bee::Unit> {
        bee::DataBuffer buffer;
        bail(res, post_read_pipe->read_all_available(buffer));
        if (res.is_eof()) { shot("Engine gone"); }
        while (auto info = queue_bridge->pop()) {
          termino_engine->_add_engine_lines(*info);
        }
        return bee::ok();
      }());
    }));

  return termino_engine;
}

void TerminoEngine::set_board(const Board& board)
{
  _current_best_move.reset();
  _current_search_id++;
  auto queue = weak_ptr(_queue_bridge);
  *_board = board;
  _engine->start_mpv_search_sp(
    board,
    100,
    10,
    [queue,
     search_id = _current_search_id](vector<SearchResultInfo::ptr>&& results) {
      if (auto ptr = queue.lock()) {
        ptr->push(ResultItem{
          .results = std::move(results),
          .search_id = search_id,
        });
      }
    });
}

void TerminoEngine::set_on_update(
  const function<void(bee::OrError<bee::Unit>&&)>& on_update)
{
  _on_update = on_update;
}

termino::Element::ptr TerminoEngine::element() const { return _engine_info; }

optional<Move> TerminoEngine::current_best_move() { return _current_best_move; }

void TerminoEngine::_add_engine_lines(const ResultItem& item)
{
  if (item.search_id != _current_search_id) { return; }

  _engine_info->clear();

  optional<uint64_t> nodes;
  optional<Span> think_time;
  optional<int> depth;
  for (const auto& info : item.results) {
    if (!nodes || *nodes < info->nodes) {
      nodes = info->nodes;
      think_time = info->think_time;
      depth = info->depth;
    }
  }

  if (nodes) {
    _engine_info->add_line(bee::format(
      "depth:$ knodes/s:$ time:$",
      *depth,
      *nodes / think_time->to_float_seconds() / 1000.0,
      think_time->to_string()));
    _engine_info->add_line("");
  }
  for (const auto& info : item.results) {
    auto pretty_moves = info->make_pretty_moves(*_board);
    auto move =
      pretty_moves.empty() ? pretty_moves[0] : bee::format(info->best_move);
    _engine_info->add_line(
      bee::format("$: $ pv:$", info->depth, info->eval, pretty_moves));
  }
}

void TerminoEngine::update(bee::OrError<bee::Unit>&& update)
{
  _on_update(std::move(update));
}

} // namespace blackbit
