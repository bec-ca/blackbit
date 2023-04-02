#include "bee/format.hpp"
#include "blackbit/game_result.hpp"
#include "board.hpp"
#include "generated_game_record.hpp"
#include "pgn_parser.hpp"
#include "random.hpp"
#include "rules.hpp"
#include "yasf/cof.hpp"

#include <unordered_set>

namespace gr = generated_game_record;
using bee::print_err_line;
using bee::print_line;

namespace blackbit {
namespace {

int run(int argc, char** argv)
{
  if (argc < 2) { return 1; }

  must(reader, PGNFileReader::create(argv[1]));

  int longest_game = 0;

  int count = 0;
  while (true) {
    must(pgn_or_err, reader->next());
    if (!pgn_or_err.has_value()) { break; }
    auto pgn = std::move(*pgn_or_err);

    count++;

    longest_game = std::max(longest_game, int(pgn.moves.size()));

    Board board;
    board.set_initial();

    std::vector<gr::MoveInfo> moves;

    for (const auto& m_str : pgn.moves) {
      auto m = Rules::parse_pretty_move(board, m_str);
      if (m.is_error()) {
        print_line("Invalid move: $ $", m_str, m.error());
        print_line("Board:\n $", board);
        return 1;
      }

      if (!Rules::is_legal_move(board, Rules::make_scratch(board), *m)) {
        print_err_line("Got illegal move: $($)", m_str, m);
        break;
      }

      board.move(*m);

      if (board.is_history_full()) break;

      moves.push_back(gr::MoveInfo{.move = *m});

      if (Rules::is_game_over_slow(board)) { break; }
    }

    auto game = gr::Game{
      .id = count,
      .moves = moves,
      .white = {.name = pgn.tag("White").value_or("-")},
      .black = {.name = pgn.tag("Black").value_or("-")},
      .params = {},
      .game_result = Rules::result(board, Rules::make_scratch(board)),
    };
    print_line(yasf::Cof::serialize(game));
  }

  return 0;
}

} // namespace
} // namespace blackbit

int main(int argc, char** argv) { return blackbit::run(argc, argv); }
