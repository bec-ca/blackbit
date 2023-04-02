#pragma once

#include "bee/error.hpp"
#include "board.hpp"
#include "engine.hpp"
#include "termino/element.hpp"
#include "termino/text_box.hpp"

#include <memory>

namespace blackbit {

template <class T> struct QueueBridge {
 public:
  std::optional<T> pop()
  {
    auto lock = std::unique_lock(_mutex);
    if (_local_queue.empty()) { return std::nullopt; }
    auto ret = std::move(_local_queue.front());
    _local_queue.pop();
    return ret;
  }

  void push(T&& info)
  {
    auto lock = std::unique_lock(_mutex);
    _local_queue.push(std::move(info));
    _fd->write("r");
  }

  QueueBridge(const bee::FileDescriptor::shared_ptr& fd) : _fd(fd) {}

 private:
  std::mutex _mutex;
  std::queue<T> _local_queue;
  bee::FileDescriptor::shared_ptr _fd;
};

struct TerminoEngine : public std::enable_shared_from_this<TerminoEngine> {
 private:
  struct ResultItem {
    std::vector<SearchResultInfo::ptr> results;
    int search_id;
  };

 public:
  using ptr = std::shared_ptr<TerminoEngine>;

  static bee::OrError<ptr> create(const PCP::ptr& pcp);

  void set_board(const Board& board);

  void set_on_update(
    const std::function<void(bee::OrError<bee::Unit>&&)>& on_update);

  termino::Element::ptr element() const;

  TerminoEngine(
    Engine::ptr&& engine,
    const std::shared_ptr<QueueBridge<ResultItem>>& queue_bridge);
  ~TerminoEngine();

  std::optional<Move> current_best_move();

 private:
  void _add_engine_lines(const ResultItem& results);

  void update(bee::OrError<bee::Unit>&& update);

  std::shared_ptr<termino::TextBox> _engine_info;

  Engine::ptr _engine;

  std::function<void(bee::OrError<bee::Unit>&&)> _on_update;

  std::shared_ptr<bee::Queue<SearchResultInfo::ptr>> _post_queue;

  std::optional<std::thread> _post_queue_thread;

  std::optional<Move> _current_best_move;

  std::shared_ptr<QueueBridge<ResultItem>> _queue_bridge;

  std::unique_ptr<Board> _board;

  int _current_search_id = 0;
};

} // namespace blackbit
