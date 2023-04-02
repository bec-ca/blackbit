#include "rules.hpp"

#include "eval_scratch.hpp"
#include "pieces.hpp"

#include "bee/format_optional.hpp"
#include "bee/format_vector.hpp"

using std::make_unique;
using std::optional;
using std::string;
using std::vector;

namespace blackbit {
namespace {

template <class T> struct PieceRules {
 public:
  BitBoard get_blockers(const Board& board) const
  {
    return impl.adjust_blockers(board, board.get_blockers());
  }

  BitBoard moves_bb(
    const Board& board,
    Color color,
    Place place,
    BitBoard attacked,
    BitBoard rooks) const
  {
    return impl.moves_bb(
             color,
             place,
             get_blockers(board),
             attacked,
             rooks,
             board.castle_flags) &
           ~(board.bb_blockers[color]);
  }

  BitBoard attacks_bb(const Board& board, Color color, Place place) const
  {
    return impl.attacks_bb(color, place, get_blockers(board)) &
           ~(board.bb_blockers[color]);
  }

  BitBoard takes_bb(const Board& board, Color color, Place place) const
  {
    return attacks_bb(board, color, place) & board.bb_blockers[oponent(color)];
  }

  void list_moves(
    const Board& board,
    Color color,
    Place place,
    BitBoard attacked,
    BitBoard rooks,
    MoveVector& list) const
  {
    int first = list.size();
    pop_moves(place, moves_bb(board, color, place, attacked, rooks), list);
    impl.set_promos(first, list);
  }

  void list_takes(
    const Board& board, Color color, Place place, MoveVector& list) const
  {
    int first = list.size();
    BitBoard bb = takes_bb(board, color, place);
    pop_moves(place, bb, list);
    impl.set_promos(first, list);
  }

  int count_attacking_squares(
    const Board& board, Color color, Place place) const
  {
    return attacks_bb(board, color, place).pop_count();
  }

  int count_takes(const Board& board, Color color, Place place) const
  {
    // TODO filter out pawns maybe?
    BitBoard bb = takes_bb(board, color, place);
    return bb.pop_count();
  }

  const PieceVector& pieces(const Board& board, Color color) const
  {
    return board.pieces(color, impl.type());
  }

  bool is_valid_move(
    const Board& board, const Move& m, BitBoard attacked, BitBoard rooks) const
  {
    return impl.additional_move_validation(m) &&
           moves_bb(board, board[m.o].owner, m.o, attacked, rooks).is_set(m.d);
  }

  void list_all_moves(
    const Board& board,
    Color color,
    BitBoard attacked,
    BitBoard rooks,
    MoveVector& list) const
  {
    for (const auto& place : pieces(board, color)) {
      list_moves(board, color, place, attacked, rooks, list);
    }
  }

  BitBoard all_attacks_bb(const Board& board, Color color) const
  {
    BitBoard out;
    for (const auto& place : pieces(board, color)) {
      out |= attacks_bb(board, color, place);
    }
    return out;
  }

  void list_all_takes(const Board& board, Color color, MoveVector& list) const
  {
    for (const auto& place : pieces(board, color)) {
      list_takes(board, color, place, list);
    }
  }

 private:
  T impl;

  static void pop_moves(Place o, BitBoard b, MoveVector& list)
  {
    while (not b.empty()) {
      Place d = b.pop_place();
      list.push_back(Move(o, d, PieceType::CLEAR));
    }
  }
};

struct PawnImpl {
  BitBoard adjust_blockers(const Board& board, BitBoard bb) const
  {
    if (board.passan_place.is_valid()) { bb.set(board.passan_place); }
    return bb;
  }

  void set_promos(int first, MoveVector& list) const
  {
    for (; first < list.size(); first++) {
      auto& m = list[first];
      int line = m.dl();
      if (line == 0 || line == 7) { m.set_promotion(PieceType::QUEEN); }
    }
  }

  BitBoard moves_bb(
    Color color,
    Place place,
    BitBoard blockers,
    BitBoard,
    BitBoard,
    const CastleFlags&) const
  {
    return BitBoard::get_pawn_moves(color, place, blockers);
  }

  BitBoard attacks_bb(Color color, Place place, BitBoard blockers) const
  {
    return BitBoard::get_pawn_capture_promotion_moves(color, place, blockers);
  }

  bool additional_move_validation(const Move& m) const
  {
    PieceType promo = m.promotion();
    int dl = m.dl();
    if (promo != PieceType::CLEAR) {
      if (promo == PieceType::PAWN || promo == PieceType::KING) {
        return false;
      }
      if (dl != 0 && dl != 7) { return false; }
    } else if (dl == 0 || dl == 7) {
      return false;
    }
    return true;
  }

  PieceType type() const { return PieceType::PAWN; }
};

struct KnightImpl {
  BitBoard adjust_blockers(
    __attribute__((unused)) const Board& board, BitBoard bb) const
  {
    return bb;
  }

  void set_promos(
    __attribute__((unused)) int first,
    __attribute__((unused)) MoveVector& list) const
  {}

  BitBoard moves_bb(
    Color, Place place, BitBoard, BitBoard, BitBoard, const CastleFlags&) const
  {
    return BitBoard::get_knight_moves(place);
  }

  BitBoard attacks_bb(
    __attribute__((unused)) Color color,
    Place place,
    __attribute__((unused)) BitBoard blockers) const
  {
    return BitBoard::get_knight_moves(place);
  }

  bool additional_move_validation(const Move& m) const
  {
    return m.promotion() == PieceType::CLEAR;
  }

  PieceType type() const { return PieceType::KNIGHT; }
};

struct BishopImpl {
  BitBoard adjust_blockers(
    __attribute__((unused)) const Board& board, BitBoard bb) const
  {
    return bb;
  }

  void set_promos(int, MoveVector&) const {}

  BitBoard moves_bb(
    Color,
    Place place,
    BitBoard blockers,
    BitBoard,
    BitBoard,
    const CastleFlags&) const
  {
    return BitBoard::get_bishop_moves(place, blockers);
  }

  BitBoard attacks_bb(
    __attribute__((unused)) Color color, Place place, BitBoard blockers) const
  {
    return BitBoard::get_bishop_moves(place, blockers);
  }

  bool additional_move_validation(const Move& m) const
  {
    return m.promotion() == PieceType::CLEAR;
  }

  PieceType type() const { return PieceType::BISHOP; }
};

struct RookImpl {
  BitBoard adjust_blockers(
    __attribute__((unused)) const Board& board, BitBoard bb) const
  {
    return bb;
  }

  void set_promos(
    __attribute__((unused)) int first,
    __attribute__((unused)) MoveVector& list) const
  {}

  BitBoard moves_bb(
    __attribute__((unused)) Color color,
    Place place,
    BitBoard blockers,
    __attribute__((unused)) BitBoard attacked,
    __attribute__((unused)) BitBoard rooks,
    __attribute__((unused)) const CastleFlags& castle_flags) const
  {
    return BitBoard::get_rook_moves(place, blockers);
  }

  BitBoard attacks_bb(
    __attribute__((unused)) Color color, Place place, BitBoard blockers) const
  {
    return BitBoard::get_rook_moves(place, blockers);
  }

  bool additional_move_validation(const Move& m) const
  {
    return m.promotion() == PieceType::CLEAR;
  }

  PieceType type() const { return PieceType::ROOK; }
};

struct QueenImpl {
  BitBoard adjust_blockers(
    __attribute__((unused)) const Board& board, BitBoard bb) const
  {
    return bb;
  }

  void set_promos(
    __attribute__((unused)) int first,
    __attribute__((unused)) MoveVector& list) const
  {}

  BitBoard moves_bb(
    __attribute__((unused)) Color color,
    Place place,
    BitBoard blockers,
    __attribute__((unused)) BitBoard attacked,
    __attribute__((unused)) BitBoard rooks,
    __attribute__((unused)) const CastleFlags& castle_flags) const
  {
    return BitBoard::get_queen_moves(place, blockers);
  }

  BitBoard attacks_bb(
    __attribute__((unused)) Color color, Place place, BitBoard blockers) const
  {
    return BitBoard::get_queen_moves(place, blockers);
  }

  bool additional_move_validation(const Move& m) const
  {
    return m.promotion() == PieceType::CLEAR;
  }

  PieceType type() const { return PieceType::QUEEN; }
};

struct KingImpl {
  BitBoard adjust_blockers(const Board&, BitBoard bb) const { return bb; }

  void set_promos(int, MoveVector&) const {}

  void maybe_add_castle(
    Color color,
    Place place,
    int line,
    const CastleFlags& castle_flags,
    BitBoard blockers,
    BitBoard attacked,
    BitBoard rooks,
    BitBoard& moves) const
  {
    if (attacked.is_set(place)) return;

    if (place.line() == line) {
      // TODO: check if rook is there
      // FIXME: incomplete castle rules
      // TODO: add maskes for blockes and attacked
      if (
        castle_flags.can_castle_king_side(color) &&
        rooks.is_set(Place::of_line_of_col(line, 7)) &&
        blockers.is_not_set(Place::of_line_of_col(line, 5)) &&
        blockers.is_not_set(Place::of_line_of_col(line, 6)) &&
        attacked.is_not_set(Place::of_line_of_col(line, 5)) &&
        attacked.is_not_set(Place::of_line_of_col(line, 6))) {
        moves.set(Place::of_line_of_col(line, 6));
      }
      if (
        castle_flags.can_castle_queen_side(color) &&
        rooks.is_set(Place::of_line_of_col(line, 0)) &&
        blockers.is_not_set(Place::of_line_of_col(line, 1)) &&
        blockers.is_not_set(Place::of_line_of_col(line, 2)) &&
        blockers.is_not_set(Place::of_line_of_col(line, 3)) &&
        attacked.is_not_set(Place::of_line_of_col(line, 2)) &&
        attacked.is_not_set(Place::of_line_of_col(line, 3))) {
        moves.set(Place::of_line_of_col(line, 2));
      }
    }
  }

  BitBoard moves_bb(
    Color color,
    Place place,
    BitBoard blockers,
    BitBoard attacked,
    BitBoard rooks,
    const CastleFlags& castle_flags) const
  {
    BitBoard moves = BitBoard::get_king_moves(place);
    if (castle_flags.can_castle(color)) {
      if (color == Color::White) {
        maybe_add_castle(
          Color::White,
          place,
          0,
          castle_flags,
          blockers,
          attacked,
          rooks,
          moves);
      } else if (color == Color::Black) {
        maybe_add_castle(
          Color::Black,
          place,
          7,
          castle_flags,
          blockers,
          attacked,
          rooks,
          moves);
      } else {
        assert(false);
      }
    }
    return moves;
  }

  BitBoard attacks_bb(
    __attribute__((unused)) Color color,
    Place place,
    __attribute__((unused)) BitBoard blockers) const
  {
    return BitBoard::get_king_moves(place);
  }

  bool additional_move_validation(const Move& m) const
  {
    return m.promotion() == PieceType::CLEAR;
  }

  PieceType type() const { return PieceType::KING; }
};

using PawnRules = PieceRules<PawnImpl>;
using KnightRules = PieceRules<KnightImpl>;
using BishopRules = PieceRules<BishopImpl>;
using RookRules = PieceRules<RookImpl>;
using QueenRules = PieceRules<QueenImpl>;
using KingRules = PieceRules<KingImpl>;

struct CombinedRules {
 public:
  BitBoard attacks_bb(const Board& board, Color color) const
  {
    BitBoard out;
    out |= pawn_rules.all_attacks_bb(board, color);
    out |= knight_rules.all_attacks_bb(board, color);
    out |= bishop_rules.all_attacks_bb(board, color);
    out |= rook_rules.all_attacks_bb(board, color);
    out |= queen_rules.all_attacks_bb(board, color);
    out |= king_rules.all_attacks_bb(board, color);
    return out;
  }

  void list_moves(
    const Board& board, const EvalScratch& scratch, MoveVector& moves) const
  {
    const Color color = board.turn;
    const BitBoard attacked = scratch.attacks_bb.get(oponent(color));
    BitBoard rooks = board.bbPeca[color][PieceType::ROOK];

    pawn_rules.list_all_moves(board, color, attacked, rooks, moves);
    knight_rules.list_all_moves(board, color, attacked, rooks, moves);
    bishop_rules.list_all_moves(board, color, attacked, rooks, moves);
    rook_rules.list_all_moves(board, color, attacked, rooks, moves);
    queen_rules.list_all_moves(board, color, attacked, rooks, moves);
    king_rules.list_all_moves(board, color, attacked, rooks, moves);
  }

  void list_piece_moves(
    const Board& b,
    const EvalScratch& scratch,
    MoveVector& list,
    PieceType type) const
  {
    const Color color = b.turn;
    const BitBoard attacked = scratch.attacks_bb.get(oponent(color));
    BitBoard rooks = b.bbPeca[color][PieceType::ROOK];
    switch (type) {
    case PieceType::PAWN:
      pawn_rules.list_all_moves(b, color, attacked, rooks, list);
      break;
    case PieceType::KNIGHT:
      knight_rules.list_all_moves(b, color, attacked, rooks, list);
      break;
    case PieceType::BISHOP:
      bishop_rules.list_all_moves(b, color, attacked, rooks, list);
      break;
    case PieceType::ROOK:
      rook_rules.list_all_moves(b, color, attacked, rooks, list);
      break;
    case PieceType::QUEEN:
      queen_rules.list_all_moves(b, color, attacked, rooks, list);
      break;
    case PieceType::KING:
      king_rules.list_all_moves(b, color, attacked, rooks, list);
      break;
    case PieceType::CLEAR:
      assert(false);
    }
  }

  void list_takes(const Board& board, MoveVector& moves)
  {
    const Color color = board.turn;

    pawn_rules.list_all_takes(board, color, moves);
    knight_rules.list_all_takes(board, color, moves);
    bishop_rules.list_all_takes(board, color, moves);
    rook_rules.list_all_takes(board, color, moves);
    queen_rules.list_all_takes(board, color, moves);
    king_rules.list_all_takes(board, color, moves);
  }

  bool is_king_under_attack(
    const Board& board, const EvalScratch& scratch, const Color color) const
  {
    const BitBoard attacked = scratch.attacks_bb.get(oponent(color));
    return !(board.bbPeca[color][PieceType::KING] & attacked).empty();
  }

  bool is_valid_move(
    const Board& board, const EvalScratch&, const Move& m) const
  {
    const Color color = board.turn;

    auto is_valid_by_general_rule = [&]() {
      const BitBoard attacked = attacks_bb(board, oponent(color));
      const BitBoard rooks = board.bbPeca[color][PieceType::ROOK];
      switch (board[m.o].type) {
      case PieceType::PAWN:
        return pawn_rules.is_valid_move(board, m, attacked, rooks);
      case PieceType::KNIGHT:
        return knight_rules.is_valid_move(board, m, attacked, rooks);
      case PieceType::BISHOP:
        return bishop_rules.is_valid_move(board, m, attacked, rooks);
      case PieceType::ROOK:
        return rook_rules.is_valid_move(board, m, attacked, rooks);
      case PieceType::QUEEN:
        return queen_rules.is_valid_move(board, m, attacked, rooks);
      case PieceType::KING:
        return queen_rules.is_valid_move(board, m, attacked, rooks);
      case PieceType::CLEAR:
        return false;
      }
      assert(false);
    }();

    if (!is_valid_by_general_rule) { return false; }

    {
      Board copy = board;
      copy.move(m);
      if (is_king_under_attack(copy, Rules::make_scratch(copy), color)) {
        return false;
      }
    }
    return true;
  }

 private:
  PawnRules pawn_rules;
  KnightRules knight_rules;
  BishopRules bishop_rules;
  RookRules rook_rules;
  QueenRules queen_rules;
  KingRules king_rules;
};

enum class PiecesLeft {
  KingOnly,
  KingOneKnight,
  KingOneBishop,
  Other,
};

bool is_draw_by_insuficient_material(const Board& board)
{
  auto pieces_left = [&](Color color) {
    const auto& p = board.pieces(color);
    if (
      p[PieceType::QUEEN].size() > 0 || p[PieceType::ROOK].size() > 0 ||
      p[PieceType::PAWN].size() > 0 || p[PieceType::KING].size() != 1) {
      return PiecesLeft::Other;
    }

    if (p[PieceType::KNIGHT].size() == 0 && p[PieceType::BISHOP].size() == 0) {
      return PiecesLeft::KingOnly;
    }
    if (p[PieceType::KNIGHT].size() == 1 && p[PieceType::BISHOP].size() == 0) {
      return PiecesLeft::KingOneKnight;
    }
    if (p[PieceType::KNIGHT].size() == 0 && p[PieceType::BISHOP].size() == 1) {
      return PiecesLeft::KingOneBishop;
    }
    return PiecesLeft::Other;
  };

  auto white_left = pieces_left(Color::White);
  if (white_left == PiecesLeft::Other) { return false; }

  auto black_left = pieces_left(Color::Black);
  if (black_left == PiecesLeft::Other) { return false; }

  if (
    white_left == PiecesLeft::KingOnly || black_left == PiecesLeft::KingOnly) {
    return true;
  }

  return false;
}

bool has_legal_moves(const Board& board, const EvalScratch& scratch)
{
  MoveVector moves;
  Rules::list_moves(board, scratch, moves);
  for (auto m : moves) {
    if (Rules::is_legal_move(board, scratch, m)) { return true; }
  }
  return false;
}

} // namespace

void Rules::list_moves(
  const Board& board, const EvalScratch& scratch, MoveVector& moves)
{
  CombinedRules rules;
  rules.list_moves(board, scratch, moves);
}

void Rules::list_takes(const Board& board, MoveVector& moves)
{
  CombinedRules rules;
  rules.list_takes(board, moves);
}

bool Rules::is_legal_move(
  const Board& board, const EvalScratch& scratch, const Move& move)
{
  CombinedRules rules;
  return rules.is_valid_move(board, scratch, move);
}

bool Rules::is_king_under_attack(
  const Board& board, const EvalScratch& scratch, Color color)
{
  CombinedRules rules;
  return rules.is_king_under_attack(board, scratch, color);
}

bool Rules::is_check(const Board& board, const EvalScratch& scratch)
{
  CombinedRules rules;
  return rules.is_king_under_attack(board, scratch, board.turn);
}

bool Rules::is_mate(const Board& board, const EvalScratch& scratch)
{
  if (!is_check(board, scratch)) { return false; }
  return !has_legal_moves(board, scratch);
  return true;
}

GameResult Rules::result(const Board& board, const EvalScratch& scratch)
{
  bool check = is_check(board, scratch);

  bool legal_moves = has_legal_moves(board, scratch);

  if (check && !legal_moves) {
    // mate
    if (board.turn == Color::White) {
      return GameResult::BlackWon;
    } else {
      return GameResult::WhiteWon;
    }
  }

  if (!legal_moves) {
    // Draw by stalemate
    return GameResult::Draw;
  }

  if (is_draw_without_stalemate(board)) { return GameResult::Draw; }

  return GameResult::NotFinished;
}

bool Rules::is_draw_without_stalemate(const Board& board)
{
  if (
    board.repeated() || board.moves_since_last_catpure_or_pawn_move() >= 100) {
    return true;
  }

  return is_draw_by_insuficient_material(board);
}

BitBoard Rules::attacks_bb(const Board& board, Color color)
{
  CombinedRules rules;
  return rules.attacks_bb(board, color);
}

bool Rules::is_game_over_slow(const Board& b)
{
  return result_slow(b) != GameResult::NotFinished;
}

GameResult Rules::result_slow(const Board& b)
{
  return result(b, make_scratch(b));
}

EvalScratch Rules::make_scratch(const Board& b)
{
  return {
    .attacks_bb =
      {
        attacks_bb(b, Color::White),
        attacks_bb(b, Color::Black),
      },
  };
}

string Rules::pretty_move(const Board& b, Move m)
{
  PieceType piece = b.at(m.o).type;
  Color color = b.at(m.o).owner;

  if (piece == PieceType::KING) {
    if (m.o.col() == 4 && m.d.col() == 6) {
      return "O-O";
    } else if (m.o.col() == 4 && m.d.col() == 2) {
      return "O-O-O";
    }
  }

  bool captured = b.at(m.d).type != PieceType::CLEAR;

  // There are some bugs here when multiple pieces of the same type can go to
  // the same square.

  string output;
  if (piece != PieceType::PAWN) { output += piece_to_letter(piece); }

  bool has_other_piece = false;
  bool has_other_piece_same_rank = false;
  bool has_other_piece_same_file = false;
  auto scratch = make_scratch(b);
  for (auto& p : b.pieces(color, piece)) {
    if (p == m.o) continue;
    if (!is_legal_move(b, scratch, Move(p, m.d, m.promotion()))) continue;
    has_other_piece = true;
    if (m.o.col() == p.col()) { has_other_piece_same_file = true; }
    if (m.o.line() == p.line()) { has_other_piece_same_rank = true; }
  }

  bool include_orig_file =
    (piece == PieceType::PAWN && captured) ||
    (has_other_piece &&
     (!has_other_piece_same_file ||
      (has_other_piece_same_file && has_other_piece_same_rank)));

  bool include_orig_rank = has_other_piece_same_file;

  if (include_orig_file) { output += m.oc() + 'a'; }
  if (include_orig_rank) { output += m.ol() + '1'; }

  if (captured) { output += 'x'; }

  output += m.dc() + 'a';
  output += m.dl() + '1';

  if (m.promotion() != PieceType::CLEAR) {
    output += '=';
    output += piece_to_letter(m.promotion());
  }

  {
    auto copy = make_unique<Board>(b);
    copy->move(m);
    auto scratch = make_scratch(*copy);
    switch (result(*copy, scratch)) {
    case GameResult::BlackWon:
    case GameResult::WhiteWon:
      output += "#";
      break;
    case GameResult::Draw:
      output += "=";
      break;
    case GameResult::NotFinished:
      if (is_check(*copy, scratch)) { output += '+'; }
      break;
    }
  }

  return output;
}

bee::OrError<Move> Rules::parse_pretty_move(
  const Board& b, const std::string& m_in)
{
  PieceType type = PieceType::CLEAR;
  optional<int> from_line;
  optional<int> from_col;
  optional<Place> to;
  bool is_capture = false;
  auto color = b.turn;
  PieceType promotion = PieceType::CLEAR;

  string move_str = m_in;

  auto empty = [&]() -> bool { return move_str.empty(); };

  auto peek = [&]() -> char {
    assert(!empty());
    return move_str.back();
  };

  auto pop_char = [&]() -> char {
    char c = peek();
    move_str.pop_back();
    return c;
  };

  auto pop_line = [&]() -> int { return pop_char() - '1'; };

  auto pop_col = [&]() -> int { return pop_char() - 'a'; };

  auto pop_place = [&]() -> Place {
    int line = pop_line();
    int col = pop_col();
    return Place::of_line_of_col(line, col);
  };

  while (!empty()) {
    if (peek() == '!') {
      pop_char();
      continue;
    }
    if (peek() == '?') {
      pop_char();
      continue;
    }

    if (peek() == '+') {
      pop_char();
      continue;
    }
    if (peek() == '#') {
      pop_char();
      continue;
    }
    break;
  }

  if (move_str == "O-O") {
    if (b.turn == Color::White) {
      return Move(
        Place::of_line_of_col(0, 4),
        Place::of_line_of_col(0, 6),
        PieceType::CLEAR);
    } else if (b.turn == Color::Black) {
      return Move(
        Place::of_line_of_col(7, 4),
        Place::of_line_of_col(7, 6),
        PieceType::CLEAR);
    }
  } else if (move_str == "O-O-O") {
    if (b.turn == Color::White) {
      return Move(
        Place::of_line_of_col(0, 4),
        Place::of_line_of_col(0, 2),
        PieceType::CLEAR);
    } else if (b.turn == Color::Black) {
      return Move(
        Place::of_line_of_col(7, 4),
        Place::of_line_of_col(7, 2),
        PieceType::CLEAR);
    }
  }

  if (isupper(peek())) {
    promotion = letter_to_piece(pop_char());
    if (promotion == PieceType::CLEAR) {
      return bee::Error("Invalid promotion piece");
    }
    if (empty()) return bee::Error("Unexpected end of move");
    if (pop_char() != '=') {
      return bee::Error("Expected = before promotion letter");
    }
  }

  if (move_str.size() >= 2) {
    to = pop_place();
  } else {
    return bee::Error("Too short");
  }

  if (!empty() && peek() == 'x') {
    is_capture = true;
    pop_char();
  }

  if (!empty() && isdigit(peek())) { from_line = pop_line(); }

  if (!empty() && islower(peek())) { from_col = pop_col(); }

  if (!empty() && isupper(peek())) {
    type = letter_to_piece(pop_char());
  } else {
    type = PieceType::PAWN;
  }

  if (!empty()) { return bee::Error("Extra stuff"); }

  if (!to) { return bee::Error("No target"); }
  if (!to->is_valid()) { return bee::Error("Invalid target"); }

  auto moves = make_unique<MoveVector>();
  CombinedRules rules;
  auto scratch = make_scratch(b);
  rules.list_piece_moves(b, scratch, *moves, type);
  optional<Move> candidate;
  for (auto& m : *moves) {
    if (!is_legal_move(b, scratch, m)) { continue; }
    if (from_line.has_value() && *from_line != m.o.line()) { continue; }
    if (from_col.has_value() && *from_col != m.o.col()) { continue; }
    const auto& from_cell = b.at(m.o);
    if (from_cell.type != type) { continue; }
    assert(from_cell.owner == color);
    if (to.has_value() && *to != m.d) { continue; }

    const auto& to_cell = b.at(m.d);
    if (
      type != PieceType::PAWN &&
      ((is_capture && to_cell.type == PieceType::CLEAR) ||
       (!is_capture && to_cell.type != PieceType::CLEAR))) {
      return bee::Error(
        "Capturing empty spot or moving to ocupied spot without capture");
    }

    if (candidate) { return bee::Error("Ambiguous"); }

    candidate = m;
  }

  if (!candidate) {
    vector<Move> ms(moves->begin(), moves->end());
    return bee::Error::format(
      "No possible matching move, from:$ $ to:$ type:$ moves:$",
      from_line,
      from_col,
      to,
      type,
      ms);
  }

  candidate->set_promotion(promotion);
  return *candidate;
}

} // namespace blackbit
