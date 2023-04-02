#include "eval.hpp"

#include "bitboard.hpp"
#include "color.hpp"
#include "eval_scratch.hpp"
#include "experiment_framework.hpp"
#include "pieces.hpp"
#include "rules.hpp"

using std::array;

namespace blackbit {
namespace {

////////////////////////////////////////////////////////////////////////////////
// Experiment flags
//

// auto king_safety_from_queen_score_flag = ExperimentFlag::register_flag(
//   "king_rough_safety_from_queen_score", 697, 697, 450);

// auto attack_multiplier_flag =
//   ExperimentFlag::register_flag("attack_multiplier", 71, 71, 250);

// auto king_rough_safety_from_queen_with_pawns_score_flag =
//   ExperimentFlag::register_flag(
//     "king_rough_safety_from_queen_with_pawns_score", 0, 1000, 0);

// auto pawn_on_second_last_row_score_flag =
//   ExperimentFlag::register_flag("pawn_on_second_last_row_score", 375, 375,
//   375);

auto king_threat_from_pieces_flag =
  ExperimentFlag::register_flag("king_threat_from_pieces", -1000, 1000, 0);

auto king_threat_from_queen_flag =
  ExperimentFlag::register_flag("king_threat_from_queen", 1000, 1000, 0);

auto king_threat_from_bishop_flag =
  ExperimentFlag::register_flag("king_threat_from_bishop", 1000, 1000, 0);

auto king_threat_from_rook_flag =
  ExperimentFlag::register_flag("king_threat_from_rook", 0, 0, 0);

auto king_threat_from_knight_flag =
  ExperimentFlag::register_flag("king_threat_from_knight", 0, 0, 0);

auto king_threat_from_pieces_enabled_flag =
  ExperimentFlag::register_flag("king_threat_from_pieces_enabled", 0, 0, 0);

////////////////////////////////////////////////////////////////////////////////
// Constants
//

constexpr Score p(double pawns) { return Score::of_pawns(pawns); }

struct C {
  // King safety

  static constexpr Score king_safety_from_queen_score = p(0.271);

  constexpr static int king_safety_from_queen_rows = 5;

  constexpr static Score king_rough_safety_from_queen_score = p(0.247);

  constexpr static Score king_rough_safety_from_queen_with_pawns_score = p(0.3);

  constexpr static Score king_is_being_attacked_value = p(-0.274);

  // Attacks

  constexpr static Score knight_attack_multiplier = p(1.0);
  constexpr static Score bishop_attack_multiplier = p(1.0);
  constexpr static Score rook_attack_multiplier = p(1.0);
  constexpr static Score queen_attack_multiplier = p(1.0);

  constexpr static Score attack_multiplier = p(0.309);

  // Mobility

  constexpr static Score rook_mobility_multiplier = p(1.0);
  constexpr static Score knight_mobility_multiplier = p(0.83);
  constexpr static Score bishop_mobility_multiplier = p(1.32);
  constexpr static Score mobility_multiplier = p(1.839);

  constexpr static PieceTypeArray<Score> mobility_score{{
    p(0.0),  // none
    p(0.0),  // pawn
    p(0.04), // knight
    p(0.03), // bishop
    p(0.02), // rook
    p(0.0),  // queen
    p(0.0),  // king
    p(0.0),  // buffer
  }};

  // Pawn structure

  constexpr static Score doubled_pawn_score = p(0.0);
  constexpr static Score isolated_pawn_score = p(-0.16);

  constexpr static double passed_pawn_multiplier = 0.641;
  constexpr static array<Score, 8> passed_pawn_score = {
    p(0.0),
    p(0.50),
    p(0.55),
    p(0.61),
    p(0.68),
    p(0.76),
    p(0.85),
    p(0.0),
  };

  constexpr static ColorArray<int> first_row = {7, 0};

  constexpr static ColorArray<BitBoard> good_paws_king_side_1 =
    BitBoard::mirrored_pair(BitBoard({
      Place::of_line_of_col(1, 5),
      Place::of_line_of_col(1, 6),
      Place::of_line_of_col(1, 7),
    }));

  constexpr static ColorArray<BitBoard> good_paws_king_side_2 =
    BitBoard::mirrored_pair(BitBoard({
      Place::of_line_of_col(1, 5),
      Place::of_line_of_col(1, 6),
      Place::of_line_of_col(2, 7),
    }));

  constexpr static ColorArray<BitBoard> good_paws_queen_side_1 =
    BitBoard::mirrored_pair(BitBoard({
      Place::of_line_of_col(1, 0),
      Place::of_line_of_col(1, 1),
      Place::of_line_of_col(1, 2),
    }));

  constexpr static ColorArray<BitBoard> good_paws_queen_side_2 =
    BitBoard::mirrored_pair(BitBoard({
      Place::of_line_of_col(2, 0),
      Place::of_line_of_col(1, 1),
      Place::of_line_of_col(1, 2),
    }));

  // Rook

  constexpr static Score rook_on_open_file_score = p(0.171);

  // Bishop

  constexpr static Score bishop_pair_value = p(0.2);
};

struct E {
  ////////////////////////////////////////////////////////////////////////////////
  // helpers
  //

  static inline int count_attacks(
    const Board& board, Color color, BitBoard bb, const Experiment&)
  {
    bb &= (~board.bbPeca[oponent(color)][PieceType::PAWN]);
    return bb.pop_count();
  }

  ////////////////////////////////////////////////////////////////////////////////
  // Knight
  //

  static BitBoard knight_moves_bb(const Board& board, Color color, Place place)
  {
    return BitBoard::get_knight_moves(place) & ~(board.bb_blockers[color]);
  }

  static BitBoard knight_attacks_bb(
    const Board& board, Color color, Place place)
  {
    return BitBoard::get_knight_moves(place) &
           board.bb_blockers[oponent(color)];
  }

  static int count_knight_moves(const Board& board, Color color, Place place)
  {
    return knight_moves_bb(board, color, place).pop_count();
  }

  static int count_knight_attacks(
    const Board& board, Color color, Place place, const Experiment& exp)
  {
    return count_attacks(
      board, color, knight_attacks_bb(board, color, place), exp);
  }

  ////////////////////////////////////////////////////////////////////////////////
  // Bishop
  //

  static BitBoard bishop_attacks_bb(
    const Board& board, Color color, Place place)
  {
    return BitBoard::get_bishop_moves(place, board.get_blockers()) &
           board.bb_blockers[oponent(color)];
  }

  static int count_bishop_moves(const Board& board, Color color, Place place)
  {
    BitBoard block =
      ((board.bb_blockers[color] ^ board.bbPeca[color][PieceType::BISHOP]) ^
       board.bbPeca[color][PieceType::QUEEN]);
    BitBoard dest = BitBoard::get_bishop_moves(
                      place, block | board.bb_blockers[oponent(color)]) &
                    (~block);
    return dest.pop_count();
  }

  static int count_bishop_attacks(
    const Board& board, Color color, Place place, const Experiment& exp)
  {
    return count_attacks(
      board, color, bishop_attacks_bb(board, color, place), exp);
  }

  static Score eval_bishop_pair(
    const Board& board, Color color, const Experiment&)
  {
    if (board.pieces(color, PieceType::BISHOP).size() >= 2) {
      return C::bishop_pair_value;
    } else {
      return Score::zero();
    }
  }

  ////////////////////////////////////////////////////////////////////////////////
  // Rook

  static BitBoard rook_attacks_bb(const Board& board, Color color, Place place)
  {
    return BitBoard::get_rook_moves(place, board.get_blockers()) &
           board.bb_blockers[oponent(color)];
  }

  static int count_rook_moves(const Board& board, Color color, Place place)
  {
    BitBoard block =
      ((board.bb_blockers[color] ^ board.bbPeca[color][PieceType::ROOK]) ^
       board.bbPeca[color][PieceType::QUEEN]);
    BitBoard dest = BitBoard::get_rook_moves(
                      place, block | board.bb_blockers[oponent(color)]) &
                    (~block);
    return dest.pop_count();
  }

  static int count_rook_attacks(
    const Board& board, Color color, Place place, const Experiment& exp)
  {
    return count_attacks(
      board, color, rook_attacks_bb(board, color, place), exp);
  }

  static inline Score eval_rooks_on_open_file(
    const Board& board, Color color, const Experiment&)
  {
    auto& list = board.pieces(color);
    auto pawns_mask = board.bbPeca[color][PieceType::PAWN] |
                      board.bbPeca[oponent(color)][PieceType::PAWN];
    Score total = Score::zero();

    for (const auto& p : list[PieceType::ROOK]) {
      if ((pawns_mask & BitBoard::column_ahead(color, p)).empty()) {
        total += C::rook_on_open_file_score;
      }
    }
    return total;
  }

  ////////////////////////////////////////////////////////////////////////////////
  // Queen
  //

  static BitBoard queen_moves_bb(const Board& board, Place place)
  {
    return BitBoard::get_queen_moves(place, board.get_blockers());
  }

  static BitBoard queen_attacks_bb(const Board& board, Color color, Place place)
  {
    return queen_moves_bb(board, place) & board.bb_blockers[oponent(color)];
  }

  static int count_queen_attacks(
    const Board& board, Color color, Place place, const Experiment& exp)
  {
    return count_attacks(
      board, color, queen_attacks_bb(board, color, place), exp);
  }

  ////////////////////////////////////////////////////////////////////////////////
  // King
  //

  static inline Score eval_king_safe_from_queen(const Board& board, Color color)
  {
    auto is_king_safe_from_queen = [&]() {
      Color op = oponent(color);
      if (board.pieces(op, PieceType::QUEEN).empty()) { return true; }

      const int rows = C::king_safety_from_queen_rows;
      for (const auto& p : board.pieces(color, PieceType::KING)) {
        BitBoard queen_moves =
          queen_moves_bb(board, p) & ~(board.bb_blockers[color]);
        if (queen_moves.first_n_rows(op, rows).empty()) { return true; }
      }
      return false;
    };

    if (is_king_safe_from_queen()) {
      return C::king_safety_from_queen_score;
    } else {
      return Score::zero();
    }
  }

  static inline Score eval_king_rough_safe_from_queen(
    const Board& board, Color color)
  {
    auto is_king_roughly_safe_from_queen = [&]() {
      Color op = oponent(color);
      if (board.pieces(op, PieceType::QUEEN).empty()) { return true; }

      for (auto p : board.pieces(color, PieceType::KING)) {
        if (color == Color::Black) { p = p.mirror(); }

        if (p.line() != 0) return false;
        if (p.col() >= 3 && p.col() <= 5) return false;
        return true;
      }
      return false;
    };
    if (is_king_roughly_safe_from_queen()) {
      return C::king_rough_safety_from_queen_score;
    } else {
      return Score::zero();
    }
  }

  static inline Score eval_king_threat_from_pieces(
    const Board& board, Color color, const Experiment& exp)
  {
    auto make_castle_area = [&](bool king_side) {
      BitBoard castle_area = BitBoard::zero();

      for (int i = 1; i <= 2; i++) {
        if (king_side) {
          // castle_area.set(Place::of_line_of_col(i, 5));
          castle_area.set(Place::of_line_of_col(i, 6));
          castle_area.set(Place::of_line_of_col(i, 7));
        } else {
          castle_area.set(Place::of_line_of_col(i, 0));
          castle_area.set(Place::of_line_of_col(i, 1));
          castle_area.set(Place::of_line_of_col(i, 2));
          // castle_area.set(Place::of_line_of_col(i, 3));
        }
      }

      if (color == Color::Black) { castle_area = castle_area.mirror(); }

      return castle_area;
    };

    auto count_attacks = [&](BitBoard castle_area) {
      const auto& list = board.pieces(oponent(color));
      Score score = Score::zero();
      auto blockers = board.bbPeca[Color::White][PieceType::PAWN] |
                      board.bbPeca[Color::Black][PieceType::PAWN];

      for (Place p : list[PieceType::KNIGHT]) {
        auto bb = BitBoard::get_knight_moves(p);
        if (bb.intersects(castle_area)) {
          score +=
            Score::of_milli_pawns(king_threat_from_knight_flag.value(exp));
        }
      }
      for (Place p : list[PieceType::ROOK]) {
        auto bb = BitBoard::get_rook_moves(p, blockers);
        if (bb.intersects(castle_area)) {
          score += Score::of_milli_pawns(king_threat_from_rook_flag.value(exp));
        }
      }
      for (Place p : list[PieceType::BISHOP]) {
        auto bb = BitBoard::get_bishop_moves(p, blockers);
        if (bb.intersects(castle_area)) {
          score +=
            Score::of_milli_pawns(king_threat_from_bishop_flag.value(exp));
        }
      }
      for (Place p : list[PieceType::QUEEN]) {
        auto bb = BitBoard::get_queen_moves(p, blockers);
        if (bb.intersects(castle_area)) {
          score +=
            Score::of_milli_pawns(king_threat_from_queen_flag.value(exp));
        }
      }

      return score *
             Score::of_milli_pawns(king_threat_from_pieces_flag.value(exp));
    };

    if (king_threat_from_pieces_enabled_flag.value(exp) == 0) {
      return Score::zero();
    }

    auto p = board.king(color);
    if (color == Color::Black) { p = p.mirror(); }
    if (p.line() >= 2 || (p.col() > 3 && p.col() < 5)) { return Score::zero(); }

    BitBoard castle_area = make_castle_area(p.col() > 4);
    return count_attacks(castle_area);
  }

  static inline Score eval_king_is_being_attacked(
    const Board& board, const EvalScratch& scratch, Color color)
  {
    Color op = oponent(color);
    Score ret = Score::zero();

    auto attacks = scratch.attacks_bb.get(op);
    auto kp = board.king(color);
    if (attacks.is_set(kp)) { ret += C::king_is_being_attacked_value; }

    return ret;
  }

  static inline Score eval_king_rough_safe_from_queen_with_pawns(
    const Board& board, Color color)
  {
    auto is_king_safe_from_queen_with_pawns = [&]() {
      Color op = oponent(color);
      if (board.pieces(op, PieceType::QUEEN).empty()) { return true; }
      auto& king = board.pieces(color, PieceType::KING);
      if (king.empty()) return false;
      for (auto p : board.pieces(color, PieceType::KING)) {
        auto bb = board.bbPeca[color][PieceType::PAWN];
        if (p.line() != C::first_row[color]) return false;
        if (p.col() > 5) {
          return bb.is_all_set(C::good_paws_king_side_1[color]) ||
                 bb.is_all_set(C::good_paws_king_side_2[color]);
        } else if (p.col() < 3) {
          return bb.is_all_set(C::good_paws_queen_side_1[color]) ||
                 bb.is_all_set(C::good_paws_queen_side_2[color]);
        } else {
          return false;
        }
      }
      return true;
    };
    if (is_king_safe_from_queen_with_pawns()) {
      return C::king_rough_safety_from_queen_with_pawns_score;
    } else {
      return Score::zero();
    }
  }

  static inline Score eval_king_safety(
    const Board& board,
    const EvalScratch& scratch,
    Color color,
    const Experiment& exp)
  {
    return eval_king_safe_from_queen(board, color) +
           eval_king_rough_safe_from_queen(board, color) +
           eval_king_rough_safe_from_queen_with_pawns(board, color) +
           eval_king_is_being_attacked(board, scratch, color) +
           eval_king_threat_from_pieces(board, color, exp);
  }

  ////////////////////////////////////////////////////////////////////////////////
  // Pawn
  //

  static inline Score eval_pawns(const Board& board, Color t, const Experiment&)
  {
    Score pawn_score = Score::zero();

    Color op = oponent(t);
    auto bb = board.bbPeca[t][PieceType::PAWN];
    for (auto p : board.pieces(t, PieceType::PAWN)) {
      int row = t == Color::White ? p.line() : p.mirror().line();
      if (
        (BitBoard::get_passed_pawn_mask(t, p) &
         board.bbPeca[op][PieceType::PAWN]) == BitBoard::zero()) {
        pawn_score += C::passed_pawn_score[row] * C::passed_pawn_multiplier;
      }
      if ((BitBoard::get_neighbor_col_mask(p) & bb) == BitBoard::zero()) {
        pawn_score += C::isolated_pawn_score;
      }
      if (((BitBoard::get_col_mask(p) & bb).invert(p)) != BitBoard::zero()) {
        pawn_score += C::doubled_pawn_score;
      }
    }

    return pawn_score;
  }

  ////////////////////////////////////////////////////////////////////////////////
  // Eval
  //

  static inline Score eval_attacks(
    const Board& board, Color c, const Experiment& exp)
  {
    Score attack_points = Score::zero();

    auto& list = board.pieces(c);

    {
      int knight_attacks = 0;
      for (const auto& p : list[PieceType::KNIGHT]) {
        knight_attacks += count_knight_attacks(board, c, p, exp);
      }
      attack_points += (C::knight_attack_multiplier * knight_attacks);
    }

    {
      int bishop_attacks = 0;
      for (const auto& p : list[PieceType::BISHOP]) {
        bishop_attacks += count_bishop_attacks(board, c, p, exp);
      }
      attack_points += (C::bishop_attack_multiplier * bishop_attacks);
    }

    {
      int rook_attacks = 0;
      for (const auto& p : list[PieceType::ROOK]) {
        rook_attacks += count_rook_attacks(board, c, p, exp);
      }
      attack_points += (C::rook_attack_multiplier * rook_attacks);
    }

    {
      int queen_attacks = 0;
      for (const auto& p : list[PieceType::QUEEN]) {
        queen_attacks = count_queen_attacks(board, c, p, exp);
      }
      attack_points += (C::queen_attack_multiplier * queen_attacks);
    }

    return attack_points * C::attack_multiplier;
  }

  static inline Score eval_mob(const Board& board, Color c, const Experiment&)
  {
    Score mob_score = Score::zero();
    auto& list = board.pieces(c);
    int knight_moves = 0;
    for (const auto& p : list[PieceType::KNIGHT]) {
      knight_moves += count_knight_moves(board, c, p);
    }
    mob_score +=
      (C::mobility_score[PieceType::KNIGHT] * knight_moves *
       C::knight_mobility_multiplier);

    int bishop_moves = 0;
    for (const auto& p : list[PieceType::BISHOP]) {
      bishop_moves += count_bishop_moves(board, c, p);
    }
    mob_score +=
      (C::mobility_score[PieceType::BISHOP] * bishop_moves *
       C::bishop_mobility_multiplier);

    int rook_moves = 0;
    for (const auto& p : list[PieceType::ROOK]) {
      rook_moves += count_rook_moves(board, c, p);
    }
    mob_score +=
      (C::mobility_score[PieceType::ROOK] * rook_moves *
       C::rook_mobility_multiplier);

    return mob_score * C::mobility_multiplier;
  }

  static inline PlayerFeatures player_features(
    const Board& board,
    const EvalScratch& scratch,
    Color color,
    const Experiment& exp)
  {
    auto material_points = board.material_score(color);

    auto attack_points = eval_attacks(board, color, exp);

    auto mobility_points = eval_mob(board, color, exp);

    auto pawn_points = eval_pawns(board, color, exp);

    auto rooks_on_open_file_points = eval_rooks_on_open_file(board, color, exp);

    auto bishop_pair_points = eval_bishop_pair(board, color, exp);

    auto king_safe_from_queen_points = eval_king_safe_from_queen(board, color);

    auto king_rough_safe_from_queen_points =
      eval_king_rough_safe_from_queen(board, color);

    auto king_rough_safe_from_queen_with_pawns_points =
      eval_king_rough_safe_from_queen_with_pawns(board, color);

    auto king_is_being_attacked_points =
      eval_king_is_being_attacked(board, scratch, color);

    auto king_threat_from_pieces =
      eval_king_threat_from_pieces(board, color, exp);

    auto current_eval = material_points + attack_points + mobility_points +
                        pawn_points + rooks_on_open_file_points +
                        bishop_pair_points + king_safe_from_queen_points +
                        king_rough_safe_from_queen_points +
                        king_rough_safe_from_queen_with_pawns_points +
                        king_is_being_attacked_points + king_threat_from_pieces;

    return PlayerFeatures{
      .current_eval = current_eval,
      .material_points = material_points,
      .attack_points = attack_points,
      .mobility_points = mobility_points,

      .pawn_points = pawn_points,

      .rooks_on_open_file_points = rooks_on_open_file_points,

      .bishop_pair_points = bishop_pair_points,

      .king_safe_from_queen_points = king_safe_from_queen_points,
      .king_rough_safe_from_queen_points = king_rough_safe_from_queen_points,
      .king_rough_safe_from_queen_with_pawns_points =
        king_rough_safe_from_queen_with_pawns_points,
      .king_is_being_attacked_points = king_is_being_attacked_points,
      .king_threat_from_pieces = king_threat_from_pieces,
    };
  }

  static inline Score eval_side(
    const Board& board,
    const EvalScratch& scratch,
    Color c,
    const Experiment& exp)
  {
    return player_features(board, scratch, c, exp).current_eval;
  }

  static Score default_eval_for_white(
    const Board& board, const EvalScratch& scratch, const Experiment& exp)
  {
    return eval_side(board, scratch, Color::White, exp) -
           eval_side(board, scratch, Color::Black, exp);
  }
};

} // namespace

////////////////////////////////////////////////////////////////////////////////
// Multipliers
//

double Multipliers::attack_multiplier()
{
  return C::attack_multiplier.to_pawns();
}

double Multipliers::mobility_multiplier()
{
  return C::mobility_multiplier.to_pawns();
}

double Multipliers::king_safety_from_queen_score()
{
  return C::king_safety_from_queen_score.to_pawns();
}

double Multipliers::king_rough_safety_from_queen_score()
{
  return C::king_rough_safety_from_queen_score.to_pawns();
}

double Multipliers::king_rough_safety_from_queen_with_pawns_score()
{
  return C::king_rough_safety_from_queen_with_pawns_score.to_pawns();
}

////////////////////////////////////////////////////////////////////////////////
// Evaluator
//

Score Evaluator::eval_for_white(
  const Board& board,
  const EvalScratch& scratch,
  const Experiment& exp,
  const EvalParameters& eval_params)
{
  if (eval_params.custom_eval != nullptr) {
    return eval_params.custom_eval(features(board, scratch, exp), board);
  } else {
    return E::default_eval_for_white(board, scratch, exp);
  }
}

Score Evaluator::eval_for_current_player(
  const Board& board,
  const EvalScratch& scratch,
  const Experiment& exp,
  const EvalParameters& eval_params)
{
  return eval_for_white(board, scratch, exp, eval_params)
    .neg_if(board.turn == Color::Black);
}

Features Evaluator::features(
  const Board& board, const EvalScratch& scratch, const Experiment& exp)
{
  return Features{
    E::player_features(board, scratch, Color::White, exp),
    E::player_features(board, scratch, Color::Black, exp),
  };
}

Score Evaluator::eval_king_safety(
  const Board& board,
  const EvalScratch& scratch,
  Color color,
  const Experiment& exp)
{
  return E::eval_king_safety(board, scratch, color, exp);
}

Score Evaluator::eval_rooks_on_open_file(
  const Board& board, Color color, const Experiment& exp)
{
  return E::eval_rooks_on_open_file(board, color, exp);
}

Score Evaluator::eval_pawns(
  const Board& board, Color color, const Experiment& exp)
{
  return E::eval_pawns(board, color, exp);
}

////////////////////////////////////////////////////////////////////////////////
// EvalParameters
//

EvalParameters EvalParameters::default_params() { return EvalParameters{}; }

EvalParameters EvalParameters::default_test_params()
{
  return EvalParameters{};
}

} // namespace blackbit
