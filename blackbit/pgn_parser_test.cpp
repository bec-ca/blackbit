#include "pgn_parser.hpp"

#include "bee/format_map.hpp"
#include "bee/format_vector.hpp"
#include "bee/testing.hpp"

using bee::print_line;
using std::string;

namespace blackbit {
namespace {

TEST(BASIC)
{
  string pgn_str = "[Event \"foo\"]\n"
                   "[Site \"over the rainbow\"]\n"
                   "[Date \"tomorrow\"]\n"
                   "[Round \"11\"]\n"
                   "[Result \"1-0\"]\n"
                   "\n"
                   "1. foo bar 2. bla { bad move } ha {good move} 3. bar foo\n";

  must(pgn, PGN::of_string(pgn_str));

  print_line(pgn.tags);
  print_line(pgn.moves);
}

TEST(BASIC_MULTI)
{
  string pgn_str =
    "[Event \"fo\\\\o\"]\n"
    "[Site \"over the rainbow\"]\n"
    "[Date \"tomo\\\"rrow\"]\n"
    "[Round \"11\"]\n"
    "[Result \"1-0\"]\n"
    "\n"
    "1. foo bar 2. bla { bad move } (2. bli blo) ha {good move} 3. bar foo\n"
    "\n"
    "[Event \"other\"]\n"
    "[Site \"under the rainbow\"]\n"
    "[Date \"yesterday\"]\n"
    "[Round \"-11\"]\n"
    "[Result \"1-1\"]\n"
    "\n"
    "1. bar { hmm } foo 2. baz { bad move } hi {good move} 3. ho foo\n";

  must(pgns, PGN::of_string_many(pgn_str));

  for (const auto& pgn : pgns) {
    print_line(pgn.tags);
    print_line(pgn.moves);
  }
}

TEST(WEIRD)
{
  string pgn_str =
    "[Event \"foo\"]\n"
    "[Site \"over the rainbow\"]\n"
    "[Date \"tomorrow\"]\n"
    "[Round \"11\"]\n"
    "[Result \"1-0\"]\n"
    "\n"
    "1. foo bar 2. bla { bad move } 2... ha {good move} 3. bar foo\n";

  must(pgn, PGN::of_string(pgn_str));

  print_line(pgn.tags);
  print_line(pgn.moves);
}

} // namespace
} // namespace blackbit
