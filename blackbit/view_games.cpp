#include "view_games.hpp"

#include "board.hpp"
#include "generated_game_record.hpp"
#include "rules.hpp"

#include "async/async_command.hpp"
#include "async/scheduler_context.hpp"
#include "bee/file_reader.hpp"
#include "bee/format.hpp"
#include "bee/format_optional.hpp"
#include "bee/format_vector.hpp"
#include "bee/string_util.hpp"
#include "blackbit/eval.hpp"
#include "blackbit/pcp.hpp"
#include "termino/label.hpp"
#include "termino/margin.hpp"
#include "termino/shelf.hpp"
#include "termino/stack.hpp"
#include "termino/termino.hpp"
#include "termino/termino_app.hpp"
#include "termino/text_box.hpp"
#include "termino_board.hpp"
#include "termino_engine.hpp"
#include "yasf/cof.hpp"

#include <memory>

using namespace termino;
using namespace async;
using bee::format;
using bee::print_line;
using std::make_shared;
using std::make_unique;
using std::nullopt;
using std::optional;
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
  optional<gr::MoveInfo> move_annotations;
  vector<string> pv_pretty;
};

struct AppMain : public TerminoApp {
 public:
  virtual Task<bee::Unit> tear_down() override
  {
    _engine.reset();
    co_return bee::unit;
  }

  static Task<bee::OrError<bee::Unit>> run(
    vector<gr::Game>&& games, const PCP::ptr& pcp)
  {
    co_bail(engine, TerminoEngine::create(pcp));

    auto app_main = make_shared<AppMain>(std::move(games), pcp, engine);
    weak_ptr<AppMain> app_main_weak(app_main);

    engine->set_on_update([app_main_weak](bee::OrError<bee::Unit>&& error) {
      if (auto app_main = app_main_weak.lock()) {
        app_main->tear_down_if_error([&]() -> bee::OrError<bee::Unit> {
          bail_unit(error);
          if (auto app_main = app_main_weak.lock()) {
            return app_main->refresh();
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

  optional<HistoryInfo> maybe_move()
  {
    if (!_game.has_value()) { return nullopt; }
    auto& game = *_game;
    if (_move_idx >= int(game.moves.size())) { return nullopt; }
    const gr::MoveInfo& move = game.moves[_move_idx];
    Move m = move.move;
    string pretty_move = Rules::pretty_move(*_board, m);
    MoveInfo mi = _board->move(m);
    vector<string> pv_pretty;
    {
      auto copy = make_unique<Board>(*_board);
      copy->undo(m, mi);
      for (const auto& m : move.pv) {
        pv_pretty.push_back(Rules::pretty_move(*copy, m));
        copy->move(m);
      }
    }
    return HistoryInfo{
      .mi = mi,
      .move = m,
      .pretty_move = pretty_move,
      .move_annotations = move,
      .pv_pretty = pv_pretty,
    };
  }

  void reset_engine()
  {
    _eval_info->clear();
    _move_info->clear();
    _engine->set_board(*_board);
  }

  void update_move_info()
  {
    auto add_move = [&](const auto& move) {
      auto ann = move.move_annotations;

      _move_info->add_line(format("move: $", move.pretty_move));
      if (ann.has_value()) {
        _move_info->add_line(format("evaluation: $", ann->evaluation));
        _move_info->add_line(format("depth: $", ann->depth));
        _move_info->add_line(format("nodes: $", ann->nodes));
        _move_info->add_line(format("think_time: $", ann->think_time));
      }
      _move_info->add_line(format("pv: $", move.pv_pretty));
      _move_info->add_line(format("fen: $", _board->to_fen()));
    };

    reset_engine();

    if (!_history.empty()) {
      _move_info->add_line("Last move:");
      add_move(_history.back());
    }

    if (auto next_move = maybe_move()) {
      _board->undo(next_move->move, next_move->mi);
      _move_info->add_line("");
      _move_info->add_line("Next move:");
      add_move(*next_move);
    }

    maybe_display_pcp();
  }

  std::optional<PCP::entry> lookup_pcp()
  {
    if (_pcp == nullptr) { return nullopt; }
    auto res = _pcp->lookup_raw(_board->to_fen());
    if (res.is_error()) { return nullopt; }
    return std::move(*res);
  }

  void maybe_display_pcp()
  {
    _pcp_info->clear();
    auto entry_opt = lookup_pcp();
    if (!entry_opt.has_value()) { return; }
    auto& entry = *entry_opt;
    _pcp_info->add_line(format("Think time: $", entry.think_time));
    for (auto& move : entry.best_moves) {
      auto repr = Rules::pretty_move(*_board, move.move);
      _pcp_info->add_line(format(
        "$ $ (depth $) (pv moves $)",
        move.evaluation,
        repr,
        move.depth,
        move.pv.size()));
    }
  }

  void set_game(const gr::Game& game)
  {
    _game = game;
    if (_game->starting_fen.has_value()) {
      _board->set_fen(*_game->starting_fen);
    } else {
      _board->set_initial();
    }
    _history.clear();
    _move_idx = 0;

    _game_info->clear();
    _game_info->add_line(format("Half moves: $", _game->moves.size()));
    _game_info->add_line(
      format("White player: $", yasf::Cof::serialize(_game->white)));
    _game_info->add_line(
      format("Black player: $", yasf::Cof::serialize(_game->black)));
    _game_info->add_line(format("Result: $", _game->game_result));
    for (const auto& param : _game->params) {
      _game_info->add_line(format("$: $", param.name, param.value));
    }
    update_move_info();
  }

  void set_initial()
  {
    _game = nullopt;
    _board->set_initial();
    _history.clear();
    _move_idx = 0;

    _game_info->clear();
    update_move_info();
  }

  virtual bee::OrError<bee::Unit> render() override
  {
    _title->clear();
    if (_game.has_value()) {
      _title->add_line(
        format("Showing game $, Move $", _game_idx + 1, _move_idx + 1));
    }

    optional<Move> last_move;
    if (
      _game.has_value() && _move_idx > 0 &&
      _move_idx - 1 < std::ssize(_game->moves)) {
      last_move = _game->moves.at(_move_idx - 1).move;
    }
    _board_view->update_board(*_board, last_move);
    return bee::ok();
  }

  void next_move()
  {
    if (auto next_move = maybe_move()) {
      _history.push_back(*next_move);
      _move_idx++;
      update_move_info();
    }
  }

  void previous_move()
  {
    if (!_history.empty()) {
      _move_idx--;
      auto& last = _history.back();
      _board->undo(last.move, last.mi);
      _history.pop_back();
      update_move_info();
    }
  }

  void next_game()
  {
    if (_game_idx + 1 < int(_games.size())) {
      _game_idx++;
      set_game(_games[_game_idx]);
    }
  }

  void previous_game()
  {
    if (_game_idx > 0) {
      _game_idx--;
      set_game(_games[_game_idx]);
    }
  }

  void update_input_move_label()
  {
    if (_in_insert_move_mode) {
      _input_move_label->set_text(format("Enter move: $", _inserted_move));
    } else {
      _input_move_label->set_text("");
    }
  }

  void enter_insert_move_mode()
  {
    _in_insert_move_mode = true;
    _inserted_move = "";
    update_input_move_label();
  }

  void leave_insert_move_mode()
  {
    _in_insert_move_mode = false;
    _inserted_move = "";
    update_input_move_label();
  }

  void make_inserted_move()
  {
    auto move_str = _inserted_move;
    leave_insert_move_mode();
    auto m = Rules::parse_pretty_move(*_board, move_str);
    if (m.is_error()) {
      _input_move_label->set_text(m.error().msg());
      return;
    }
    if (!Rules::is_legal_move(*_board, Rules::make_scratch(*_board), *m)) {
      _input_move_label->set_text("Illegal move");
      return;
    }
    auto pretty_move = Rules::pretty_move(*_board, *m);
    MoveInfo mi = _board->move(*m);
    auto info = HistoryInfo{
      .mi = mi,
      .move = *m,
      .pretty_move = pretty_move,
      .move_annotations = nullopt,
      .pv_pretty = {},
    };
    _history.push_back(info);
    update_move_info();
  }

  virtual bee::OrError<bee::Unit> handle_input(const Key& key) override
  {
    if (_in_insert_move_mode) {
      switch (key.key_code) {
      case KeyCode::Backspace:
        if (!_inserted_move.empty()) {
          _inserted_move.pop_back();
          update_input_move_label();
        }
        break;
      case KeyCode::Escape:
        leave_insert_move_mode();
        break;
      case KeyCode::Printable:
        _inserted_move += key.character;
        update_input_move_label();
        break;
      case KeyCode::Enter:
        make_inserted_move();
        break;
      default:
        break;
      }
    } else {
      switch (key.key_code) {
      case KeyCode::Escape:
        mark_done();
        break;
      case KeyCode::Right:
        next_move();
        break;
      case KeyCode::Left:
        previous_move();
        break;
      case KeyCode::PgDown:
        next_game();
        break;
      case KeyCode::PgUp:
        previous_game();
        break;
      case KeyCode::Printable:
        if (key.character == 'i') { enter_insert_move_mode(); }
        break;
      default:
        break;
      }
    }
    return bee::ok();
  }

  AppMain(
    vector<gr::Game>&& games,
    const PCP::ptr& pcp,
    const TerminoEngine::ptr& engine)
      : _board(make_unique<Board>()),
        _title(make_shared<TextBox>()),
        _game_info(make_shared<TextBox>()),
        _eval_info(make_shared<TextBox>()),
        _move_info(make_shared<TextBox>()),
        _board_view(make_shared<TerminoBoard>()),
        _games(std::move(games)),
        _pcp(pcp),
        _engine(engine)
  {
    {
      using namespace termino;
      set_root(Margin::create(
        Stack::create(
          {
            _title,
            Shelf::create(
              {
                _board_view,
                Stack::create(
                  {
                    _game_info,
                    _eval_info,
                    _move_info,
                    _pcp_info,
                  },
                  1),
              },
              1),
            _input_move_label,
            _engine->element(),
          },
          1),
        1));
    }
    if (!_games.empty()) {
      set_game(_games[0]);
    } else {
      set_initial();
    }
  }

 private:
  int _game_idx = 0;
  int _move_idx = 0;

  optional<gr::Game> _game;

  unique_ptr<Board> _board;
  shared_ptr<TextBox> _title;
  shared_ptr<TextBox> _game_info;
  shared_ptr<TextBox> _eval_info;
  shared_ptr<TextBox> _move_info;
  shared_ptr<Label> _input_move_label = make_shared<Label>();
  shared_ptr<TextBox> _pcp_info = make_shared<TextBox>();
  shared_ptr<TerminoBoard> _board_view;

  vector<HistoryInfo> _history;

  vector<gr::Game> _games;

  PCP::ptr _pcp;

  TerminoEngine::ptr _engine;

  bool _in_insert_move_mode = false;
  string _inserted_move;
};

bee::OrError<vector<gr::Game>> maybe_load_games(
  const optional<string>& games_filename)
{
  vector<gr::Game> games;
  if (games_filename.has_value()) {
    bail(
      games_file,
      bee::FileReader::open(bee::FilePath::of_string(*games_filename)));
    while (!games_file->is_eof()) {
      bail(line, games_file->read_line());
      bail(game, yasf::Cof::deserialize<gr::Game>(line));
      if (game.moves.empty()) continue;
      games.push_back(std::move(game));
    }
  }
  return games;
}

bee::OrError<PCP::ptr> maybe_load_pcp(const optional<string>& filename)
{
  if (filename.has_value()) {
    return PCP::open_in_memory(bee::FilePath::of_string(*filename));
  }
  return nullptr;
}

Task<bee::OrError<bee::Unit>> view_games(
  const optional<string>& games_filename, const optional<string>& pcp_filename)
{
  co_bail(games, maybe_load_games(games_filename));
  co_bail(pcp, maybe_load_pcp(pcp_filename));
  co_return co_await AppMain::run(std::move(games), pcp);
}

} // namespace

command::Cmd ViewGames::command()
{
  using namespace command::flags;
  auto builder = command::CommandBuilder("View games");
  auto games_filename = builder.optional("--games-file", string_flag);
  auto pcp = builder.optional("--pcp-file", string_flag);
  return run_coro(builder, [=]() { return view_games(*games_filename, *pcp); });
}

} // namespace blackbit
