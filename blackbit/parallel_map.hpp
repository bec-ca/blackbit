#pragma once

#include "bee/queue.hpp"

#include <cassert>
#include <thread>
#include <vector>

namespace blackbit {
namespace parallel_map {

template <class T, class R>
struct State : public std::enable_shared_from_this<State<T, R>> {
  using ptr = std::shared_ptr<State>;

  struct EndIterator {};

  struct Iterator {
   public:
    Iterator(ptr state) : _state(state), _it(state->output_queue->begin()) {}

    Iterator(const Iterator& other) = delete;
    Iterator(Iterator&& other) = default;

    R& operator*() { return *_it; }

    Iterator& operator++()
    {
      ++_it;
      if (_it.ended()) { _state->join(); }
      return *this;
    }

    bool operator==(const EndIterator&) const { return _it.ended(); }

   private:
    ptr _state;
    typename bee::Queue<R>::Iterator _it;
  };

  State()
      : workers(std::make_shared<std::vector<std::thread>>()),
        input_queue(std::make_shared<bee::Queue<T>>()),
        output_queue(std::make_shared<bee::Queue<R>>())
  {}

  ~State() { assert(_joined); }

  State(const State& other) = delete;
  State(State&& other) = delete;

  void join()
  {
    assert(!_joined);
    waiter.join();
    _joined = true;
  }

  Iterator begin() { return Iterator(this->shared_from_this()); }
  EndIterator end() { return EndIterator(); }

  std::shared_ptr<std::vector<std::thread>> workers;
  std::shared_ptr<bee::Queue<T>> input_queue;
  std::shared_ptr<bee::Queue<R>> output_queue;
  std::thread waiter;

 private:
  bool _joined = false;
};

template <class T, class R> struct StateWrapper {
 public:
  using S = State<T, R>;

  StateWrapper(const typename S::ptr& state) : _state(state) {}

  auto begin() { return _state->begin(); }
  auto end() { return _state->end(); }

 private:
  typename S::ptr _state;
};

template <
  std::move_constructible T,
  std::invocable<T> F,
  std::move_constructible R = std::invoke_result_t<F, T>>
StateWrapper<T, R> go(const std::vector<T>& inputs, int num_workers, F map)
{
  auto state = std::make_shared<State<T, R>>();

  for (auto& v : inputs) { state->input_queue->push(std::move(v)); }
  state->input_queue->close();

  for (int i = 0; i < num_workers; i++) {
    state->workers->emplace_back([input_queue = state->input_queue,
                                  output_queue = state->output_queue,
                                  map] {
      for (const auto& input : *input_queue) { output_queue->push(map(input)); }
    });
  }

  state->waiter =
    std::thread([workers = state->workers, output_queue = state->output_queue] {
      for (auto& worker : *workers) { worker.join(); }
      output_queue->close();
    });

  return state;
}

} // namespace parallel_map
} // namespace blackbit
