#include "analyze_games.hpp"

#include "bee/file_reader.hpp"
#include "bee/file_writer.hpp"
#include "bee/string_util.hpp"
#include "bee/util.hpp"
#include "board.hpp"
#include "command/command_builder.hpp"
#include "generated_game_record.hpp"
#include "rules.hpp"
#include "statistics.hpp"
#include "yasf/cof.hpp"

#include <optional>

namespace gr = generated_game_record;

using bee::format;
using bee::print_line;
using bee::Span;
using std::decay_t;
using std::make_unique;
using std::map;
using std::nullopt;
using std::optional;
using std::pair;
using std::set;
using std::string;
using std::vector;

namespace blackbit {
namespace {

bee::OrError<bee::Unit> write_csv(
  const vector<vector<pair<string, string>>>& data, const string& filename)
{
  vector<string> columns;
  {
    set<string> seen_columns;
    for (const auto& values : data) {
      for (const auto& value : values) {
        if (seen_columns.find(value.first) != seen_columns.end()) { continue; }
        columns.push_back(value.first);
        seen_columns.insert(value.first);
      }
    }
  }

  vector<string> rows;
  rows.push_back(bee::join(columns, ",") + '\n');
  for (const auto& values : data) {
    vector<string> row;
    map<string, string> value_index(values.begin(), values.end());
    for (const auto& column : columns) {
      auto it = value_index.find(column);
      if (it == value_index.end()) {
        row.push_back("");
      } else {
        row.push_back(it->second);
      }
    }
    rows.push_back(bee::join(row, ",") + '\n');
  }

  return bee::FileWriter::save_file(
    bee::FilePath::of_string(filename), bee::join(rows, ""));
}

struct Stat {
 public:
  void add(double value)
  {
    _sum += value;
    _count++;
  }

  double average() const { return _sum / _count; }
  double sum() const { return _sum; }

 private:
  double _sum = 0;
  uint64_t _count = 0;
};

template <class T> struct is_optional;

template <class T> struct is_optional<optional<T>> {
  constexpr static bool value = true;
};

template <class T> struct is_optional {
  constexpr static bool value = false;
};

template <class T> string csv_fmt(T&& value)
{
  using K = decay_t<decltype(value)>;
  if constexpr (is_optional<K>::value) {
    if (value.has_value()) {
      return format(*value);
    } else {
      return "";
    }
  } else {
    return format(std::forward<T>(value));
  }
}

struct PerPlayerStat {
  int game_count = 0;
  double sum_score = 0;

  void add_game(double score, int count = 1)
  {
    game_count += count;
    sum_score += score;
  }

  double avg_score_conf() const
  {
    return Statistics::bernoulli_confidence_95(avg_score(), game_count);
  }

  double avg_score() const { return sum_score / game_count; }
};

struct PerGameId {
  int game_count = 0;

  map<string, PerPlayerStat> per_player;

  void add_game(
    const string& player1_name,
    const string& player2_name,
    double player1_score,
    double player2_score)
  {
    game_count++;
    per_player[player1_name].add_game(player1_score);
    per_player[player2_name].add_game(player2_score);
  }
};

string format_float_3decimals(double value)
{
  if (value < 0) { return "-" + format_float_3decimals(-value); }
  int v = floor(value * 1000.0 + 0.5);
  auto str = format(v);
  if (str.size() < 4) { str.insert(0, 4 - str.size(), '0'); }
  str.insert(str.size() - 3, 1, '.');
  return str;
}

bee::OrError<bee::Unit> analyze_games(
  const string& games_filename,
  const optional<string>& summary_csv_filename,
  const optional<string>& positions_csv_filename,
  const optional<string>& positions_cof_filename)
{
  vector<gr::Game> games;
  bail(
    games_file,
    bee::FileReader::open(bee::FilePath::of_string(games_filename)));
  while (!games_file->is_eof()) {
    bail(line, games_file->read_line());
    bail(game, yasf::Cof::deserialize<gr::Game>(line));
    games.push_back(std::move(game));
  }

  print_line("Read $ games", games.size());

  map<string, set<GameResult>> results_by_starting_fen;

  for (const auto& game : games) {
    if (!game.game_result.has_value()) continue;
    results_by_starting_fen[game.starting_fen.value_or(Board::initial_fen())]
      .insert(*game.game_result);
  }

  auto has_different_game_results = [&](const string& starting_fen) {
    auto it = results_by_starting_fen.find(starting_fen);
    if (it == results_by_starting_fen.end()) return false;
    return it->second.size() > 1;
  };

  auto board = make_unique<Board>();

  vector<vector<pair<string, string>>> summary;
  vector<vector<pair<string, string>>> positions;
  vector<gr::Position> position_records;

  map<string, PerPlayerStat> per_player;
  map<string, PerPlayerStat> per_player_changed_games;

  map<int, PerGameId> per_game_id;

  int game_id = 0;
  for (const auto& game : games) {
    auto starting_fen = game.starting_fen.value_or(Board::initial_fen());
    if (game.moves.empty()) { continue; }
    bail_unit(board->set_fen(starting_fen));

    double white_score_d = game.white_score.value_or(0.0);
    double black_score_d = game.black_score.value_or(0.0);

    per_player[game.white.name].add_game(white_score_d);
    per_player[game.black.name].add_game(black_score_d);

    auto changed = has_different_game_results(starting_fen);
    if (changed) {
      per_player_changed_games[game.white.name].add_game(white_score_d);
      per_player_changed_games[game.black.name].add_game(black_score_d);
    }

    auto& psp = per_game_id[*game.id];
    psp.add_game(
      game.white.name, game.black.name, white_score_d, black_score_d);

    string white_score = format(white_score_d);
    string black_score = format(black_score_d);

    int is_draw = white_score_d == 0.5 && black_score_d == 0.5 ? 1 : 0;

    game_id++;
    vector<pair<string, string>> params = {
      {"game_id", csv_fmt(game_id)},
      {"white", game.white.name},
      {"black", game.black.name},
      {"white_score", white_score},
      {"black_score", black_score},
      {"starting_fen", starting_fen},
      {"num_moves", csv_fmt(game.moves.size())},
      {"is_draw", csv_fmt(is_draw)},
      {"changed", csv_fmt(changed)},
      {format("$_score", game.white.name), white_score},
      {format("$_score", game.black.name), black_score},
    };
    for (const auto& param : game.params) {
      params.emplace_back(param.name, param.value);
    }

    PlayerPair<uint32_t> total_nodes(0);
    PlayerPair<Span> total_time(Span::zero());

    PlayerPair<Stat> depth_stat;

    PlayerPair<string> player_name(game.white.name, game.black.name);

    auto& moves = game.moves;
    int position_idx = 0;
    for (int i = 0; i < int(moves.size()) - 1; i++) {
      auto turn = board->turn;
      auto& m1 = moves[i];
      auto& m2 = moves[i + 1];

      total_nodes.get(turn) += m1.nodes.value_or(0);
      total_time.get(turn) += m1.think_time.value_or(Span::zero());
      depth_stat.get(turn).add(m1.depth.value_or(0));

      auto eval1 = m1.evaluation;
      auto eval2 = m2.evaluation;

      string position_fen = board->to_fen();
      string pm = Rules::pretty_move(*board, m1.move);

      board->move(m1.move);

      auto nm = game.moves[i + 1].move;
      string npm = Rules::pretty_move(*board, nm);

      if (eval1.has_value() && eval2.has_value()) {
        auto eval_before = eval1->to_pawns();
        auto eval_after = eval2->to_pawns();

        if (turn == Color::Black) {
          eval_before = -eval_before;
          eval_after = -eval_after;
        }

        auto eval_before_clamped = Statistics::clamp(eval_before, -10, 10);
        auto eval_after_clamped = Statistics::clamp(eval_after, -10, 10);

        auto evaluation_change =
          std::abs(eval_before_clamped - eval_after_clamped);

        position_idx++;
        optional<double> time_secs = nullopt;
        if (m1.think_time.has_value()) {
          time_secs = m1.think_time->to_seconds();
        }

        position_records.push_back(gr::Position{
          .fen = position_fen,
          .move_taken = m1,
          .next_move_taken = m2,
          .white = game.white,
          .black = game.black,
          .white_score = game.white_score,
          .black_score = game.black_score,
          .game_result = game.game_result,
          .params = game.params,
        });

        vector<pair<string, string>> position({
          {"position_id", csv_fmt(position_idx)},
          {"evaluation_change", csv_fmt(evaluation_change)},
          {"eval_before", csv_fmt(eval_before)},
          {"eval_after", csv_fmt(eval_after)},
          {"eval_before_clamped", csv_fmt(eval_before_clamped)},
          {"eval_after_clamped", csv_fmt(eval_after_clamped)},
          {"move", csv_fmt(pm)},
          {"next_move", csv_fmt(npm)},
          {"position_fen", csv_fmt(position_fen)},
          {"time", csv_fmt(time_secs)},
          {"nodes", csv_fmt(m1.nodes)},
          {"depth", csv_fmt(m1.depth)},
          {"turn_color", csv_fmt(turn)},
          {"turn_player", csv_fmt(player_name.get(turn))},
        });
        for (const auto& p : params) { position.emplace_back(p); }
        positions.push_back(std::move(position));
      }
    }

    auto fn_id = [](const auto& v) { return v; };

    auto add_stat = [&](const string& name, auto v, auto f) {
      params.emplace_back(
        format("$_$", game.white.name, name), format(f(v.white())));
      params.emplace_back(
        format("$_$", game.black.name, name), format(f(v.black())));
    };

    add_stat("total_time", total_time, std::mem_fn(&Span::to_seconds));
    add_stat("total_nodes", total_nodes, fn_id);
    add_stat("avg_depth", depth_stat, std::mem_fn(&Stat::average));

    summary.push_back(std::move(params));
  }

  auto show_summary = [](const map<string, PerPlayerStat>& per_player) {
    for (const auto& pp : per_player) {
      print_line(
        "player:$ avg_score:$(±$) delta:$(±$) num_games:$",
        pp.first,
        format_float_3decimals(pp.second.avg_score() * 100.0),
        format_float_3decimals(pp.second.avg_score_conf() * 100.0),
        format_float_3decimals(((pp.second.avg_score() - 0.5) * 2.0 * 100.0)),
        format_float_3decimals(pp.second.avg_score_conf() * 2.0 * 100.0),
        pp.second.game_count);
    }
  };

  print_line("all games:");
  show_summary(per_player);
  print_line("---------------------------------");
  print_line("changed games:");
  show_summary(per_player_changed_games);
  print_line("---------------------------------");

  if (summary_csv_filename.has_value()) {
    bail_unit(write_csv(summary, *summary_csv_filename));
  }

  if (positions_csv_filename.has_value()) {
    bail_unit(write_csv(positions, *positions_csv_filename));
  }

  if (positions_cof_filename.has_value()) {
    bail(
      writer,
      bee::FileWriter::create(
        bee::FilePath::of_string(*positions_cof_filename)));
    for (const auto& p : position_records) {
      auto data = yasf::Cof::serialize(p);
      data += '\n';
      bail_unit(writer->write(data));
    }
  }

  return bee::ok();
}

} // namespace

command::Cmd AnalyzeGames::command()
{
  using namespace command;
  using namespace command::flags;
  auto builder = CommandBuilder("Analyze games");
  auto games_filename = builder.required("--games-file", string_flag);
  auto summary_csv_filename = builder.optional("--summary-csv", string_flag);
  auto positions_csv_filename =
    builder.optional("--positions-csv", string_flag);
  auto positions_cof_filename =
    builder.optional("--positions-cof", string_flag);
  return builder.run([=]() {
    return analyze_games(
      *games_filename,
      *summary_csv_filename,
      *positions_csv_filename,
      *positions_cof_filename);
  });
}

} // namespace blackbit
