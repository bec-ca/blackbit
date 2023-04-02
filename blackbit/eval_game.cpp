#include "eval_game.hpp"

#include "engine.hpp"
#include "generated_game_record.hpp"
#include "rules.hpp"

#include "bee/file_reader.hpp"
#include "bee/format_optional.hpp"
#include "command/command_builder.hpp"
#include "yasf/cof.hpp"

#include <optional>

using bee::print_err_line;
using bee::print_line;
using bee::Span;
using std::optional;
using std::string;
using std::vector;

namespace blackbit {
namespace {

struct PieceLocation {
  Color color;
  PieceType piece_type;
  Place pos;
  double weight;
};

struct ScoringInfo {
  vector<PieceLocation> pieces;

  Score score;
};

using piece_location_double = PieceTypeArray<BoardArray<double>>;

struct test_case {
  string fen;
  string wanted_move;
};

bee::OrError<bee::Unit> eval_game_main(
  double think_time_sec, const string& games_filename)
{
  const int cache_size = 1000 * (1 << 20); // ~1gb
  auto think_time = Span::of_seconds(think_time_sec);

  vector<gr::Game> games;
  bail(
    games_file,
    bee::FileReader::open(bee::FilePath::of_string(games_filename)));
  while (!games_file->is_eof()) {
    bail(line, games_file->read_line());
    bail(game, yasf::Cof::deserialize<gr::Game>(line));
    games.push_back(std::move(game));
  }

  EvalParameters default_params = EvalParameters::default_params();

  auto engine = Engine::create(
    Experiment::base(), default_params, nullptr, cache_size, true);

  int game_count = 0;
  for (auto& game : games) {
    game_count++;

    print_line("Game $", game_count);

    Board board;
    if (game.starting_fen.has_value()) {
      board.set_fen(*game.starting_fen);
    } else {
      board.set_initial();
    }

    for (const auto& move : game.moves) {
      bail(
        got_moves,
        engine->find_best_moves_mpv(board, 50, 30, 16, think_time, nullptr));
      auto& best_move = *got_moves[0];

      auto pm = [&board](const optional<Move>& m) -> string {
        if (!m.has_value()) {
          return "N/A";
        } else {
          return Rules::pretty_move(board, *m);
        }
      };

      optional<SearchResultInfo::ptr> move_made_info;
      for (auto& m : got_moves) {
        if (m->best_move == move.move) { move_made_info = m->clone(); }
      }

      optional<Score> move_made_score;
      optional<Score> pawn_loss;
      if (move_made_info.has_value()) {
        auto& info = **move_made_info;
        move_made_score = info.eval;
        if (board.turn == Color::White) {
          pawn_loss = info.eval - best_move.eval;
        } else {
          pawn_loss = best_move.eval - info.eval;
        }
      }
      print_line(
        "Best: $ $. played: $ $. Pawn loss: $",
        pm(best_move.best_move),
        best_move.eval,
        pm(move.move),
        move_made_score,
        pawn_loss);

      if (!Rules::is_legal_move(board, Rules::make_scratch(board), move.move)) {
        print_err_line("Got invalid move: $", move.move);
        break;
      }
      board.move(move.move);

      if (
        Rules::result(board, Rules::make_scratch(board)) !=
        GameResult::NotFinished)
        break;
    }
  }

  return bee::unit;
}

} // namespace

command::Cmd EvalGame::command()
{
  using namespace command::flags;
  auto builder = command::CommandBuilder("Run game evaluation");
  auto think_time_sec =
    builder.optional_with_default("--think-time-sec", float_flag, 60.0);
  auto games_file = builder.required("--games-file", string_flag);
  return builder.run(
    [=] { return eval_game_main(*think_time_sec, *games_file); });
}

} // namespace blackbit
