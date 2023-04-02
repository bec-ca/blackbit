#include "view_positions.hpp"

#include "async/async_command.hpp"
#include "async/scheduler_context.hpp"
#include "bee/file_reader.hpp"
#include "bee/format.hpp"
#include "bee/format_optional.hpp"
#include "bee/format_vector.hpp"
#include "bee/string_util.hpp"
#include "blackbit/eval.hpp"
#include "board.hpp"
#include "generated_game_record.hpp"
#include "rules.hpp"
#include "termino/margin.hpp"
#include "termino/shelf.hpp"
#include "termino/stack.hpp"
#include "termino/termino.hpp"
#include "termino/termino_app.hpp"
#include "termino/text_box.hpp"
#include "termino_board.hpp"
#include "termino_engine.hpp"
#include "yasf/cof.hpp"

#include <algorithm>

using namespace termino;
using namespace async;
using bee::format;
using bee::print_line;
using std::make_shared;
using std::make_unique;
using std::nullopt;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;
using std::weak_ptr;

namespace blackbit {
namespace {

struct HistoryInfo {
  MoveInfo mi;
  Move move;
  string pretty_move;
  gr::MoveInfo move_annotations;
};

struct AppMain : public TerminoApp {
 public:
  virtual Task<bee::Unit> tear_down() override
  {
    _engine.reset();
    co_return bee::unit;
  }

  static Task<bee::OrError<bee::Unit>> run(vector<gr::Position>&& positions)
  {
    co_bail(engine, TerminoEngine::create(nullptr));

    auto app_main = make_shared<AppMain>(std::move(positions), engine);
    weak_ptr<AppMain> app_main_weak(app_main);

    engine->set_on_update([app_main_weak](bee::OrError<bee::Unit>&& error) {
      if (auto app_main = app_main_weak.lock()) {
        app_main->tear_down_if_error([&]() -> bee::OrError<bee::Unit> {
          bail_unit(error);
          if (auto app_main = app_main_weak.lock()) {
            return app_main->render();
          }
          return bee::ok();
        }());
      }
    });

    auto err = co_await app_main->start();
    if (!err.is_error() && app_main->is_done()) {
      print_line("Exiting normally");
    }
    co_return err;
  }

  void set_position(const gr::Position& position)
  {
    _board->set_fen(position.fen);

    _game_info->clear();
    _game_info->add_line(
      format("White player: $", yasf::Cof::serialize(position.white)));
    _game_info->add_line(
      format("Black player: $", yasf::Cof::serialize(position.black)));
    _game_info->add_line(format("Result: $", position.game_result));
    for (const auto& param : position.params) {
      _game_info->add_line(format("$: $", param.name, param.value));
    }
    _game_info->add_line(_board->to_fen());

    _engine->set_board(*_board);

    _move_info->clear();
    _move_info->add_line(
      format("move: $", Rules::pretty_move(*_board, position.move_taken.move)));
    _move_info->add_line(
      format("evaluation: $", position.move_taken.evaluation));
    _move_info->add_line(format("depth: $", position.move_taken.depth));
    _move_info->add_line(format("nodes: $", position.move_taken.nodes));
    _move_info->add_line(
      format("think_time: $", position.move_taken.think_time));
    _move_info->add_line(format("pv: $", position.move_taken.pv));

    _move_info->add_line("");
    Board copy = *_board;
    copy.move(position.move_taken.move);
    _move_info->add_line(format(
      "move: $", Rules::pretty_move(copy, position.next_move_taken.move)));
    _move_info->add_line(
      format("evaluation: $", position.next_move_taken.evaluation));
    _move_info->add_line(format("depth: $", position.next_move_taken.depth));
    _move_info->add_line(format("nodes: $", position.next_move_taken.nodes));
    _move_info->add_line(
      format("think_time: $", position.next_move_taken.think_time));
    _move_info->add_line(format("pv: $", position.next_move_taken.pv));
  }

  virtual bee::OrError<bee::Unit> render() override
  {
    _title->clear();
    _title->add_line(
      format("Showing position $/$", _position_idx + 1, _positions.size()));

    _board_view->update_board(*_board, nullopt);

    return bee::ok();
  }

  void maybe_update_position_idx(int add)
  {
    int new_value = _position_idx + add;
    if (new_value < 0) return;
    if (new_value >= int(_positions.size())) return;
    _position_idx = new_value;
    set_position(_positions[_position_idx]);
  }

  virtual bee::OrError<bee::Unit> handle_input(const Key& key) override
  {
    if (key.key_code == KeyCode::Escape) {
      mark_done();
    } else if (key.key_code == KeyCode::PgDown) {
      maybe_update_position_idx(1);
    } else if (key.key_code == KeyCode::PgUp) {
      maybe_update_position_idx(-1);
    } else if (key.key_code == KeyCode::Printable && key.character == ' ') {
      auto current_best_move = _engine->current_best_move();
      if (current_best_move.has_value()) {
        _board->move(*current_best_move);
        _engine->set_board(*_board);
      }
    }
    return bee::ok();
  }

  AppMain(vector<gr::Position>&& positions, const TerminoEngine::ptr& engine)
      : _board(make_unique<Board>()),
        _title(make_shared<TextBox>()),
        _game_info(make_shared<TextBox>()),
        _move_info(make_shared<TextBox>()),
        _board_view(make_shared<TerminoBoard>()),
        _positions(std::move(positions)),
        _engine(engine)
  {
    set_root(termino::Margin::create(
      termino::Stack::create(
        {
          _title,
          termino::Shelf::create(
            {
              _board_view,
              termino::Stack::create(
                {
                  _game_info,
                  _move_info,
                },
                1),
            },
            1),
          _engine->element(),
        },
        1),
      1));
    set_position(_positions[0]);
  }

 private:
  int _position_idx = 0;

  unique_ptr<Board> _board;
  shared_ptr<TextBox> _title;
  shared_ptr<TextBox> _game_info;
  shared_ptr<TextBox> _move_info;
  shared_ptr<TerminoBoard> _board_view;

  vector<gr::Position> _positions;

  TerminoEngine::ptr _engine;
};

double position_eval_change(const gr::Position& p1)
{
  auto eval_before_clamped = std::clamp(
    p1.move_taken.evaluation.value_or(Score::zero()).to_pawns(), -10., 10.);
  auto eval_after_clamped = std::clamp(
    p1.next_move_taken.evaluation.value_or(Score::zero()).to_pawns(),
    -10.,
    10.);
  return std::abs(eval_after_clamped - eval_before_clamped);
}

Task<bee::OrError<bee::Unit>> view_positions_main(
  const string& positions_filename)
{
  vector<gr::Position> positions;
  co_bail(
    games_file,
    bee::FileReader::open(bee::FilePath::of_string(positions_filename)));
  while (!games_file->is_eof()) {
    co_bail(line, games_file->read_line());
    co_bail(position, yasf::Cof::deserialize<gr::Position>(line));
    positions.push_back(std::move(position));
  }

  sort(
    positions.begin(),
    positions.end(),
    [](const gr::Position& p1, const gr::Position& p2) {
      return position_eval_change(p1) > position_eval_change(p2);
    });

  co_return co_await AppMain::run(std::move(positions));
}

} // namespace

command::Cmd ViewPositions::command()
{
  using namespace command::flags;
  auto builder = command::CommandBuilder("View positions");
  auto positions_filename = builder.required("--positions-file", string_flag);
  return run_coro(
    builder, [=]() { return view_positions_main(*positions_filename); });
}

} // namespace blackbit
