#include "board.hpp"

#include "bee/error.hpp"
#include "bee/string_util.hpp"

#include <cinttypes>
#include <iostream>
#include <optional>

using std::string;
using std::vector;

namespace blackbit {
namespace {

constexpr PieceTypeArray<Score> material_table{{
  Score::of_pawns(0.0), // none
  Score::of_pawns(1.0), // pawn
  Score::of_pawns(3.0), // knight
  Score::of_pawns(3.0), // bishop
  Score::of_pawns(5.0), // rook
  Score::of_pawns(9.0), // queen
  Score::of_pawns(0.0), // king
  Score::of_pawns(0.0), // buffer
}};

constexpr Score p(double pawns) { return Score::of_pawns(pawns); }

constexpr PieceTypeArray<BoardArray<Score>> piece_location_score = {{
  {
    // Clear
    p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0),
    p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0),
    p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0),
    p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0),
    p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0),
    p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0),
  },
  {
    // Pawn
    p(0.000000),  p(0.000000),  p(0.000000),  p(0.000000),  p(0.000000),
    p(0.000000),  p(0.000000),  p(0.000000),  p(-0.183270), p(-0.165620),
    p(-0.158978), p(-0.149708), p(-0.144136), p(-0.002163), p(0.031990),
    p(-0.085895), p(-0.199866), p(-0.155908), p(-0.095887), p(-0.119898),
    p(-0.058346), p(-0.022496), p(0.034081),  p(-0.060829), p(-0.276293),
    p(-0.215227), p(-0.138424), p(-0.028819), p(-0.076160), p(-0.090703),
    p(-0.138658), p(-0.159779), p(-0.156443), p(-0.125331), p(-0.097511),
    p(-0.122652), p(-0.033274), p(-0.058208), p(-0.071922), p(-0.056839),
    p(0.388053),  p(0.377633),  p(0.408884),  p(0.244804),  p(0.343166),
    p(0.469694),  p(0.522583),  p(0.352011),  p(1.766293),  p(1.691135),
    p(1.670812),  p(1.695391),  p(1.585103),  p(1.389139),  p(1.283397),
    p(1.310646),  p(0.000000),  p(0.000000),  p(0.000000),  p(0.000000),
    p(0.000000),  p(0.000000),  p(0.000000),  p(0.000000),
  },
  {
    // Knight
    p(-0.047250), p(-0.313779), p(-0.051933), p(-0.041038), p(-0.018022),
    p(0.041332),  p(-0.307854), p(-0.051615), p(-0.096289), p(-0.070517),
    p(-0.080569), p(-0.086450), p(-0.078702), p(0.027000),  p(-0.010348),
    p(-0.034498), p(-0.145305), p(-0.048223), p(-0.068241), p(0.038685),
    p(0.109477),  p(0.031698),  p(0.054172),  p(-0.051211), p(-0.115789),
    p(-0.002830), p(0.061914),  p(0.073665),  p(0.155878),  p(0.122404),
    p(0.103592),  p(-0.064961), p(-0.007701), p(0.056056),  p(0.094166),
    p(0.316146),  p(0.194380),  p(0.318483),  p(0.156513),  p(0.103360),
    p(-0.033960), p(0.002369),  p(0.151819),  p(0.210337),  p(0.269071),
    p(0.318468),  p(0.126093),  p(0.029965),  p(-0.129863), p(-0.092316),
    p(0.110534),  p(0.073681),  p(0.140864),  p(0.177843),  p(-0.024951),
    p(-0.006174), p(-0.153507), p(-0.004509), p(0.018349),  p(0.014565),
    p(0.016768),  p(-0.025351), p(-0.005991), p(-0.071165),
  },
  {
    // Bishop
    p(0.074759),  p(0.130314), p(0.032035),  p(0.063864),  p(0.079214),
    p(-0.000057), p(0.059399), p(0.080042),  p(0.135364),  p(0.147360),
    p(0.110616),  p(0.051297), p(0.100435),  p(0.150540),  p(0.267604),
    p(0.081882),  p(0.033110), p(0.210920),  p(0.187945),  p(0.124765),
    p(0.126169),  p(0.214565), p(0.170142),  p(0.114130),  p(-0.005661),
    p(0.063684),  p(0.101429), p(0.205041),  p(0.173268),  p(0.041612),
    p(0.059135),  p(0.030401), p(0.017331),  p(-0.058035), p(0.108423),
    p(0.199718),  p(0.173929), p(0.138425),  p(-0.053859), p(0.037372),
    p(0.043259),  p(0.095606), p(0.095243),  p(0.073118),  p(0.146513),
    p(0.274761),  p(0.207495), p(0.222312),  p(-0.013353), p(-0.001357),
    p(-0.015073), p(0.007373), p(-0.009400), p(0.136278),  p(0.046017),
    p(0.059147),  p(0.024650), p(0.017945),  p(0.022572),  p(-0.016470),
    p(0.000949),  p(0.005768), p(0.029391),  p(0.015887),
  },
  {
    // Rook
    p(-0.118848), p(-0.031606), p(-0.043079), p(0.024786),  p(0.049267),
    p(0.061385),  p(0.036771),  p(-0.170274), p(-0.086899), p(-0.053150),
    p(-0.049382), p(-0.043349), p(-0.035828), p(0.000329),  p(0.015977),
    p(-0.077244), p(-0.067734), p(-0.075061), p(-0.085867), p(-0.096279),
    p(-0.080625), p(-0.035022), p(0.015476),  p(-0.050178), p(-0.060414),
    p(-0.071246), p(-0.085887), p(-0.089837), p(-0.082126), p(-0.064526),
    p(-0.031400), p(-0.050844), p(-0.029012), p(-0.045742), p(-0.055608),
    p(-0.096298), p(-0.068367), p(-0.008125), p(0.005980),  p(-0.013922),
    p(-0.008441), p(0.022493),  p(-0.000165), p(-0.012998), p(0.007715),
    p(0.047989),  p(0.074788),  p(0.023608),  p(0.064473),  p(0.121046),
    p(0.140523),  p(0.154077),  p(0.156546),  p(0.164165),  p(0.203958),
    p(0.159611),  p(0.124190),  p(0.125829),  p(0.138868),  p(0.122757),
    p(0.141482),  p(0.130202),  p(0.102364),  p(0.122007),
  },
  {
    // Queen
    p(-0.022231), p(-0.101294), p(-0.102912), p(-0.114849), p(-0.040369),
    p(-0.033617), p(-0.072371), p(-0.043515), p(-0.056483), p(-0.039576),
    p(-0.035184), p(-0.039625), p(-0.026863), p(-0.021952), p(-0.065222),
    p(-0.036640), p(-0.105491), p(-0.064325), p(-0.037773), p(-0.016450),
    p(-0.054100), p(-0.042833), p(-0.029934), p(-0.022920), p(-0.178297),
    p(-0.107892), p(-0.038686), p(-0.030835), p(0.098204),  p(-0.030884),
    p(0.032179),  p(-0.029705), p(-0.061189), p(-0.124364), p(-0.027012),
    p(0.004477),  p(0.220470),  p(0.190830),  p(0.135790),  p(0.115241),
    p(-0.033862), p(0.062253),  p(0.064015),  p(0.052052),  p(0.301141),
    p(0.455071),  p(0.479116),  p(0.414807),  p(-0.007097), p(-0.097307),
    p(0.155693),  p(0.195318),  p(0.219033),  p(0.469471),  p(0.236526),
    p(0.416528),  p(0.163233),  p(0.269083),  p(0.303390),  p(0.337703),
    p(0.372667),  p(0.299379),  p(0.225105),  p(0.287546),

  },
  {
    // King
    p(-0.110618), p(0.006345),  p(-0.059654), p(-0.268808), p(-0.169693),
    p(-0.304488), p(-0.147268), p(-0.221150), p(0.032380),  p(0.010649),
    p(-0.010036), p(-0.092148), p(-0.117530), p(-0.102604), p(-0.022384),
    p(-0.014102), p(-0.022798), p(0.019712),  p(-0.012412), p(-0.060708),
    p(-0.072765), p(-0.054976), p(-0.018097), p(-0.096542), p(-0.105688),
    p(0.022235),  p(-0.023753), p(-0.037929), p(-0.080316), p(-0.047019),
    p(-0.031339), p(-0.175208), p(-0.086539), p(0.011241),  p(0.027848),
    p(0.026649),  p(0.028241),  p(0.012278),  p(-0.008283), p(-0.142300),
    p(0.056182),  p(0.180163),  p(0.183138),  p(0.183815),  p(0.182984),
    p(0.180700),  p(0.183771),  p(0.032713),  p(0.043611),  p(0.193290),
    p(0.193508),  p(0.146139),  p(0.164519),  p(0.183261),  p(0.161898),
    p(0.060184),  p(-0.013586), p(0.039432),  p(0.022986),  p(0.018527),
    p(0.018565),  p(0.026177),  p(0.046014),  p(0.013593),
  },
  {
    // Padding
    p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0),
    p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0),
    p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0),
    p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0),
    p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0),
    p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0), p(0),
  },
}};

inline Score piece_value(Place place, Color color, PieceType type)
{
  if (color == Color::Black) { place = place.mirror(); }
  return material_table[type] + piece_location_score[type][place];
}

} // namespace

Score get_piece_location_score(PieceType piece_type, Place place)
{
  return piece_location_score[piece_type][place];
}

Board::Board() : _score({{Score::zero(), Score::zero()}}) { clear(); }

void Board::clear()
{
  _squares.clear(Pos());
  passan_place = Place::invalid();

  for (auto& t : _pieces_table)
    for (auto& p : t) p.clear();

  turn = Color::White;
  _score.clear(Score::zero());
  _hash_key = 0;
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 8; ++j) {
      bbPeca[Color(i)][PieceType(j)] = BitBoard::zero();
    }
  }
  history.clear();
  _base_ply = 0;
  _last_irreversible_move = 0;

  bb_blockers[Color::Black] = bb_blockers[Color::White] = BitBoard::zero();

  castle_flags = CastleFlags::none();
}

bee::OrError<bee::Unit> Board::set_fen(const std::string& fen)
{
  PieceType type = PieceType::CLEAR;
  Color owner = Color::None;

  vector<string> parts = bee::split_space(fen);

  if (parts.size() <= 1) {
    return bee::Error("Invalid fen, should have at least two parts");
  }

  this->clear();

  // board
  int linha = 7, coluna = 0;
  for (char c : parts[0]) {
    if (c == '/') {
      --linha, coluna = 0;
    } else if (isdigit(c)) {
      coluna += c - '0';
    } else {
      owner = isupper(c) ? Color::White : Color::Black;
      type = letter_to_piece(tolower(c));
      insert_piece(Place::of_line_of_col(linha, coluna), type, owner);
      ++coluna;
    }
  }

  // turn
  const auto& turn_part = parts[1];
  if (turn_part.size() != 1) {
    return bee::Error(
      "Invalid fen, second component should have a single letter 'w' or 'b'");
  }
  turn = (tolower(turn_part.front()) == 'w') ? Color::White : Color::Black;
  if (turn == Color::Black) { _hash_key ^= hash_code_turn; }

  // castle flags
  if (!(2 < parts.size())) return bee::unit;
  const auto& castle_part = parts[2];
  if (castle_part != "-") {
    for (char c : castle_part) {
      Color color = isupper(c) ? Color::White : Color::Black;
      c = tolower(c);
      if (c == 'q') {
        castle_flags.set_queen(color);
      } else if (c == 'k') {
        castle_flags.set_king(color);
      } else {
        return bee::Error(
          "Invalid fen, got unexpected character on castle part");
      }
    }
  }
  _hash_key ^= castle_flags.hash();

  // passant
  if (!(3 < parts.size())) return bee::unit;
  const auto& passant_part = parts[3];
  if (passant_part != "-") {
    bail(p, Place::of_string(passant_part));
    passan_place = p;
  }

  // ignore half move
  if (!(4 < parts.size())) return bee::unit;

  // full move
  if (!(5 < parts.size())) return bee::unit;
  int full_move = std::stoi(parts[5]);
  _base_ply = (full_move - 1) * 2 + (turn == Color::Black ? 1 : 0);

  return bee::unit;
}

const string& Board::initial_fen()
{
  static string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -";
  return fen;
}

void Board::set_initial() { this->set_fen(initial_fen()); }

string Board::to_fen() const
{
  std::string output;

  int empties = 0;

  auto flush = [&]() {
    if (empties > 0) {
      output += '0' + empties;
      empties = 0;
    }
  };

  auto add_char = [&](char c) {
    flush();
    output += c;
  };

  auto cell_letter = [](const Pos& p) {
    char letter = piece_to_letter(p.type);
    if (p.owner == Color::Black) { letter = tolower(letter); }
    return letter;
  };

  for (int l = 0; l < 8; ++l) {
    for (int c = 0; c < 8; ++c) {
      Place place = Place::of_line_of_col(7 - l, c);
      auto& cell = _squares[place];
      if (cell.type == PieceType::CLEAR) {
        empties++;
      } else {
        add_char(cell_letter(cell));
      }
    }
    if (l < 7) {
      add_char('/');
    } else {
      flush();
    }
  }

  // tun
  output += ' ';
  output += (turn == Color::White) ? 'w' : 'b';

  // castle flags
  output += " ";
  if (castle_flags.is_clear()) {
    output += "-";
  } else {
    if (castle_flags.can_castle_king_side(Color::White)) { output += 'K'; }
    if (castle_flags.can_castle_queen_side(Color::White)) { output += 'Q'; }
    if (castle_flags.can_castle_king_side(Color::Black)) { output += 'k'; }
    if (castle_flags.can_castle_queen_side(Color::Black)) { output += 'q'; }
  }

  // passant flag
  output += " ";
  if (passan_place.is_valid()) {
    output += passan_place.to_string();
  } else {
    output += "-";
  }

  // half move clock
  output += " 0";

  // full move clock
  output += bee::format(" $", ply() / 2 + 1);

  return output;
}

string Board::to_string() const
{
  string output;
  const char tabela[7] = {' ', 'P', 'N', 'B', 'R', 'Q', 'K'};
  const char tabela2[3] = {'B', 'W', ' '};
  std::string linha;
  linha += '.';
  linha += '-';
  linha += '-';
  for (int i = 1; i < 8; i++) { linha += "---"; }
  linha += '.';
  output += linha;
  output += '\n';
  linha.clear();
  for (int l = 7;; l--) {
    for (int c = 0; c < 8; c++) {
      Place place = Place::of_line_of_col(l, c);
      linha += '|';
      if (_squares[place].type == PieceType::CLEAR) {
        linha += "  ";
      } else {
        linha += tabela[(int)_squares[place].type];
        linha += tabela2[int(_squares[place].owner)];
      }
    }
    linha += '|';
    output += linha;
    output += '\n';
    linha.clear();
    linha += "|--";
    if (l == 0) break;
    for (int i = 1; i < 8; i++) {
      linha += '+';
      linha += "--";
    }
    linha += '|';
    output += linha;
    output += '\n';
    linha.clear();
  }

  linha.clear();
  linha += "*--";
  for (int i = 1; i < 8; i++) { linha += "---"; }
  linha += '*';
  output += linha;
  output += '\n';
  output += to_fen();
  output += '\n';

  return output;
}

bool Board::checkBoard()
{
  for (int l = 0; l < 8; ++l) {
    for (int c = 0; c < 8; ++c) {
      Place place = Place::of_line_of_col(l, c);
      PieceType type = _squares[place].type;
      Color owner = _squares[place].owner;
      int id = _squares[place].id;
      if (
        (this->bb_blockers[Color::Black].is_set(place) &&
         (owner != Color::Black)) or
        (not this->bb_blockers[Color::Black].is_set(place) &&
         (owner == Color::Black))) {
        // fprintf(
        //   log_file,
        //   "blockers bitboard ta errado em (%d,%d) %d\n",
        //   l,
        //   c,
        //   place.to_int());
        // this->get_blockers().print(log_file);
        return false;
      }
      if (type == PieceType::CLEAR && owner != Color::None) {
        // fprintf(log_file, "%d %d dono de posicao vazia\n", l, c);
        // fprintf(log_file, "%s", to_string().data());
        return false;
      }
      if (type != PieceType::CLEAR && owner == Color::None) {
        // fprintf(log_file, "%d %d peca sem dono\n", l, c);
        // fprintf(log_file, "%s", to_string().data());
        return false;
      }
      if (type == PieceType::CLEAR) continue;
      if (pieces(owner, type)[id] != place) {
        // fprintf(log_file, "2 - peca_table nao bate\n");
        // fprintf(
        //   log_file,
        //   "peca_table[%d][%d][%d] = (l=%d,c=%d) b = "
        //   "(type=%d,owner=%d,id=%d)\n",
        //   int(owner),
        //   int(type),
        //   id,
        //   l,
        //   c,
        //   int(type),
        //   int(owner),
        //   id);
        // fprintf(log_file, "%s", to_string().data());
        return false;
      }
      if (type == PieceType::PAWN && (l == 0 || l == 7)) {
        bee::print_err_line("Pawn on the wrong line, $\n$", l, to_string());
        return false;
      }
    }
  }
  for (int owner2 = 0; owner2 < 2; ++owner2) {
    Color owner = Color(owner2);
    for (int t = 1; t < 7; ++t) {
      PieceType type = PieceType(t);
      auto& list = pieces(owner, type);
      for (const auto& p : list) {
        if (
          _squares[p].owner != owner or _squares[p].type != type ||
          list[_squares[p].id] != p) {
          // int l = p.line();
          // int c = p.col();
          // fprintf(log_file, "1 - peca_table nao bate\n");
          // fprintf(
          //   log_file,
          //   "peca_table[%d][%d][%d] = (l=%d,c=%d) "
          //   "b(type=%d,owner=%d,id=%d)\n",
          //   int(owner),
          //   int(type),
          //   b[p].id,
          //   l,
          //   c,
          //   int(b[p].type),
          //   int(b[p].owner),
          //   b[p].id);
          // fprintf(log_file, "%s", to_string().data());
          // this->get_blockers().print(log_file);
          return false;
        }
      }
    }
  }
  return true;
}

bee::OrError<Move> Board::parse_xboard_move_string(
  const std::string& move_str) const
{
  if (move_str == "o-o-o" or move_str == "O-O-O" or move_str == "0-0-0") {
    if (turn == Color::White) {
      return Move::of_string("e1c1");
    } else {
      return Move::of_string("e8c8");
    }
  } else if (move_str == "o-o" or move_str == "O-O" or move_str == "0-0") {
    if (turn == Color::White) {
      return Move::of_string("e1g1");
    } else {
      return Move::of_string("e8g8");
    }
  }
  return Move::of_string(move_str);
}

bool Board::check_hash_key()
{
  uint64_t hashk = 0;
  for (int p = 0; p < 64; ++p) {
    Place place = Place::of_int(p);
    if (_squares[place].owner != Color::None) {
      hashk ^= hash_code[place][_squares[place].type][_squares[place].owner];
    }
  }
  if (hashk != _hash_key) { return false; }
  return true;
}

void Board::erase_piece2(Place place)
{
  Pos p = _squares[place];
  auto& list = mutable_pieces(p.owner, p.type);
  int last = list.size() - 1;
  if (p.id != last) {
    list[p.id] = list[last];
    _squares[list[p.id]].id = p.id;
  }
  list.pop_back();
}

void Board::erase_piece(Place place)
{
  assert(_squares[place].type != PieceType::CLEAR);

  Color owner = _squares[place].owner;
  PieceType type = _squares[place].type;

  /* update material count */
  _score[owner] -= piece_value(place, owner, type);

  /* update hahs code */
  _hash_key ^= hash_code[place][type][owner];

  /* erase from piece list */
  erase_piece2(place);

  /* erase from matrix */
  _squares[place].type = PieceType::CLEAR;
  _squares[place].owner = Color::None;

  /* erase from bit board */
  bb_blockers[owner].invert(place);
  bbPeca[owner][type].invert(place);
}

void Board::insert_piece2(Place place, PieceType type, Color owner)
{
  auto& list = mutable_pieces(owner, type);
  int id = list.size();
  list.push_back(place);
  _squares[place].id = id;
}

void Board::insert_piece(Place place, PieceType type, Color owner)
{
  /* update material count */
  _score[owner] += piece_value(place, owner, type);

  /* update hash code */
  _hash_key ^= hash_code[place][type][owner];

  /* insrt to piece list */
  insert_piece2(place, type, owner);

  /* insrt to matrix */
  _squares[place].type = type;
  _squares[place].owner = owner;

  /* insert to bitboard */
  bb_blockers[owner].invert(place);
  bbPeca[owner][type].invert(place);
}

inline void Board::move_piece(const Move& m)
{
  Color owner = _squares[m.o].owner;
  PieceType type = _squares[m.o].type;

  _score[owner] +=
    piece_value(m.d, owner, type) - piece_value(m.o, owner, type);

  if (!(owner == Color::Black || owner == Color::White)) {
    bee::print_line(m);
    bee::print_line(to_string());
    assert(false);
  }
  assert(type != PieceType::CLEAR);

  int id = _squares[m.o].id;

  /* update hash code */
  _hash_key ^= hash_code[m.o][type][owner];
  _hash_key ^= hash_code[m.d][type][owner];

  /* update piece list */
  mutable_pieces(owner, type)[id] = m.d;

  /* update matrix */
  _squares[m.d] = _squares[m.o];
  _squares[m.o].type = PieceType::CLEAR;
  _squares[m.o].owner = Color::None;

  /* update bitboard */
  bb_blockers[owner].invert(m.o);
  bbPeca[owner][type].invert(m.o);
  bb_blockers[owner].invert(m.d);
  bbPeca[owner][type].invert(m.d);
}

void Board::set_piece_type(Place place, PieceType type)
{
  auto owner = _squares[place].owner;
  auto prev_type = _squares[place].type;
  /* update material */
  _score[owner] +=
    piece_value(place, owner, type) - piece_value(place, owner, prev_type);

  /* update hash code */
  _hash_key ^= hash_code[place][type][owner];
  _hash_key ^= hash_code[place][prev_type][owner];

  /* update list */
  erase_piece2(place);
  insert_piece2(place, type, owner);

  /* update the bitboard */
  bbPeca[owner][prev_type].invert(place);
  bbPeca[owner][type].invert(place);

  /* update matrix */
  _squares[place].type = type;
}

MoveInfo Board::move(Move m)
{
  MoveInfo mi;

  assert(m.is_valid());
  ASSERT(checkBoard());

  /* update the history */
  history.push_back(_hash_key);

  /* get the type of the piece to be moved */
  PieceType type = _squares[m.o].type;
  assert(type != PieceType::CLEAR);
  assert(_squares[m.o].owner == turn);
  PieceType taking_type = _squares[m.d].type;
  auto op = oponent(turn);

  /* copia peca capturada */
  if (_squares[m.d].type != PieceType::CLEAR) {
    mi.p = _squares[m.d];
    mi.capturou = true;
    erase_piece(m.d);
  } else {
    mi.capturou = false;
  }

  mi.last_irreversible_move = _last_irreversible_move;
  if (mi.capturou || (type == PieceType::PAWN)) {
    _last_irreversible_move = history.size();
  }

  /* faz en passan */
  if (type == PieceType::PAWN && (!mi.capturou) && m.dc() != m.oc()) {
    Place p = Place::of_line_of_col(m.ol(), m.dc());
    if (_squares[p].type != PieceType::PAWN) {
      bee::print_err_line(
        "En passan on not a pawn $ $ $ $ $\n$",
        type,
        _squares[p].type,
        p,
        m,
        passan_place,
        to_string());
      assert(false);
    }
    mi.p = _squares[p];
    mi.passan = true;
    erase_piece(p);
  } else {
    mi.passan = false;
  }

  /* movimenta a peca */
  move_piece(m);

  /* promote */
  if (m.promotion() != PieceType::CLEAR) {
    if (type != PieceType::PAWN) {
      bee::print_err_line("Promoting non pawn: $ $ $", m, type, m.promotion());
      assert(false);
    }
    set_piece_type(m.d, m.promotion());
  }

  /* marca posição de en passan */
  if (passan_place.is_valid()) { _hash_key ^= passant_hash[passan_place]; }
  mi.passan_place = passan_place;
  if (
    type == PieceType::PAWN &&
    (m.d.to_int() - m.o.to_int() == 16 || m.d.to_int() - m.o.to_int() == -16)) {
    passan_place = Place::of_int((m.o.to_int() + m.d.to_int()) / 2);
    _hash_key ^= passant_hash[passan_place];
  } else {
    passan_place = Place::invalid();
  }

  ASSERT(checkBoard());

  // handle castle
  mi.castle_flags = castle_flags;
  mi.castled = false;
  if (type == PieceType::KING) {
    bool is_castle = false;
    if ((m.d.col() - m.o.col()) == 2) {
      move_piece(Move(m.d.right(), m.d.left(), PieceType::CLEAR));
      is_castle = true;
    } else if ((m.d.col() - m.o.col()) == -2) {
      move_piece(Move(m.d.left().left(), m.d.right(), PieceType::CLEAR));
      is_castle = true;
    }
    mi.castled = is_castle;
    if (is_castle || castle_flags.can_castle(turn)) {
      castle_flags.clear(turn);
    }
  } else if (type == PieceType::ROOK && castle_flags.can_castle(turn)) {
    if (m.o.col() == 0) {
      castle_flags.clear_queen(turn);
    } else if (m.o.col() == 7) {
      castle_flags.clear_king(turn);
    }
  }
  if (taking_type == PieceType::ROOK && (m.d.line() == 0 || m.d.line() == 7)) {
    if (m.d.col() == 0) {
      castle_flags.clear_queen(op);
    } else if (m.d.col() == 7) {
      castle_flags.clear_king(op);
    }
  }

  _hash_key ^= castle_flags.hash() ^ mi.castle_flags.hash();

  if (_squares[m.d].type != type && m.promotion() != _squares[m.d].type) {
    bee::print_err_line(
      "didnt move peca type, $ $ $", type, _squares[m.d].type, m);
    assert(false);
  }

  /* troca a vez */
  turn = op;
  _hash_key ^= hash_code_turn;

  ASSERT(checkBoard());

  return mi;
}

void Board::undo(Move m, const MoveInfo& mi)
{
  ASSERT(checkBoard());

  /* troca a vez */
  _hash_key ^= hash_code_turn;
  turn = oponent(turn);

  // undo castle
  if (mi.castled) {
    if ((m.d.to_int() - m.o.to_int()) == 2) {
      move_piece(Move(m.d.left(), m.d.right(), PieceType::CLEAR));
    } else if ((m.d.col() - m.o.col()) == -2) {
      move_piece(Move(m.d.right(), m.d.left().left(), PieceType::CLEAR));
    }
  }

  if (castle_flags != mi.castle_flags) {
    _hash_key ^= castle_flags.hash() ^ mi.castle_flags.hash();
    castle_flags = mi.castle_flags;
  }

  /* desfaz promocao */
  if (m.promotion() != PieceType::CLEAR) {
    set_piece_type(m.d, PieceType::PAWN);
  }

  /* desfaz passan */
  if (mi.passan) {
    insert_piece(Place::of_line_of_col(m.ol(), m.dc()), mi.p.type, mi.p.owner);
  }

  /* volta a posição de en passan */
  if (passan_place.is_valid()) { _hash_key ^= passant_hash[passan_place]; }
  passan_place = mi.passan_place;
  if (passan_place.is_valid()) { _hash_key ^= passant_hash[passan_place]; }

  /* volta a peca */
  move_piece(Move(m.d, m.o, PieceType::CLEAR));

  /* volta peca capturada */
  if (mi.capturou) { insert_piece(m.d, mi.p.type, mi.p.owner); }

  history.pop_back();
  _last_irreversible_move = mi.last_irreversible_move;

  ASSERT(checkBoard());
}

MoveInfo Board::move_null()
{
  MoveInfo mi;

  /* historico */
  history.push_back(_hash_key);

  /* marca coluna de en passan */
  mi.passan_place = passan_place;
  passan_place = Place::invalid();

  /* troca a vez */
  turn = oponent(turn);
  _hash_key ^= hash_code_turn;

  return mi;
}

void Board::undo_null(const MoveInfo& mi)
{
  /* volta coluna de en passan */
  passan_place = mi.passan_place;

  /* troca a vez */
  turn = oponent(turn);
  _hash_key ^= hash_code_turn;

  /* historico */
  history.pop_back();
}

int Board::moves_since_last_catpure_or_pawn_move() const
{
  return history.size() - _last_irreversible_move;
}

} // namespace blackbit
