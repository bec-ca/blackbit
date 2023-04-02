#include "bee/format.hpp"
#include "random.hpp"

using bee::print_line;

namespace blackbit {
namespace {

int main()
{
  print_line("#pragma once");
  print_line("");
  print_line("#include \"board_array.hpp\"");
  print_line("#include \"piece_type_array.hpp\"");
  print_line("#include \"color_array.hpp\"");
  print_line("");
  print_line("namespace blackbit {");
  print_line("");
  print_line("constexpr BoardArray<PieceTypeArray<ColorArray<uint64_t>>> "
             "hash_code = {{");
  for (int p = 0; p < 64; ++p) {
    print_line("{{");
    for (int o = 0; o < 8; ++o) {
      print_line("{{");
      for (int t = 0; t < 2; ++t) { print_line("$ull,", rand64()); }
      print_line("}},");
    }
    print_line("}},");
  }
  print_line("}};");
  print_line("");
  print_line("constexpr BoardArray<uint64_t> "
             "passant_hash = {{");
  for (int p = 0; p < 64; ++p) { print_line("$ull,", rand64()); }
  print_line("}};");
  print_line("");
  print_line("constexpr std::array<uint64_t, 16> "
             "castle_hash = {{");
  for (int p = 0; p < 16; ++p) { print_line("$ull,", p == 0 ? 0 : rand64()); }
  print_line("}};");
  print_line("");
  print_line("constexpr uint64_t hash_code_turn = $ull;", rand64());
  print_line("");
  print_line("}");

  return 0;
}

} // namespace
} // namespace blackbit

int main() { return blackbit::main(); }
