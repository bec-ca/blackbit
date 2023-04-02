#pragma once

#include "bee/error.hpp"
#include "bitboard.hpp"
#include "board_array.hpp"
#include "castle_flags.hpp"
#include "color_array.hpp"
#include "debug.hpp"
#include "experiment_framework.hpp"
#include "generated_board_hashes.hpp"
#include "move.hpp"
#include "piece_type_array.hpp"
#include "pieces.hpp"
#include "score.hpp"
#include "static_vector.hpp"

#include <array>
#include <cassert>
#include <string.h>

namespace blackbit {

struct Peca {
  Place place;
  int8_t type;
  int8_t owner;
};

struct Pos {
  Pos() : id(0), type(PieceType::CLEAR), owner(Color::None), __fill__(0) {}

  int8_t id;
  PieceType type;
  Color owner;
  int8_t __fill__;

  bool is_empty() const { return type == PieceType::CLEAR; }
};

struct MoveInfo {
  MoveInfo() {}

  CastleFlags castle_flags = CastleFlags::none();
  bool passan = false;
  bool capturou = false;
  bool promoveu = false;
  bool castled = false;
  Place passan_place = Place::invalid();
  int last_irreversible_move;
  Pos p;
};

using MoveVector = StaticVector<Move, 256>;

using PieceVector = StaticVector<Place, 10>;

Score get_piece_location_score(PieceType piece_type, Place place);

struct Board {
  ColorArray<BitBoard> bb_blockers;
  ColorArray<PieceTypeArray<BitBoard>> bbPeca;

  Color turn;
  Place passan_place;

  StaticVector<uint64_t, 1024> history;

  CastleFlags castle_flags = CastleFlags::none();

  Board();

  void clear();

  static const std::string& initial_fen();

  bee::OrError<bee::Unit> set_fen(const std::string& fen);

  void set_initial();

  bool is_history_full() const { return history.full(); }

  int ply() const { return _base_ply + history.size(); }

  void erase_piece2(Place place);

  void erase_piece(Place place);

  void insert_piece2(Place place, PieceType type, Color owner);

  void insert_piece(Place place, PieceType type, Color owner);

  void move_piece(const Move& m);

  void set_piece_type(Place place, PieceType type);

  MoveInfo move(Move m);

  void undo(Move m, const MoveInfo& mi);

  MoveInfo move_null();

  void undo_null(const MoveInfo& mi);

  std::string to_fen() const;

  std::string to_string() const;

  bool checkBoard();

  BitBoard get_blockers() const
  {
    return this->bb_blockers[Color::White] | this->bb_blockers[Color::Black];
  }

  int moves_since_last_catpure_or_pawn_move() const;

  bool check_hash_key();

  bool repeated() const
  {
    for (auto i = history.begin() + _last_irreversible_move; i < history.end();
         i++) {
      if (*i == _hash_key) { return true; }
    }
    return false;
  }

  bee::OrError<Move> parse_xboard_move_string(
    const std::string& move_str) const;

  Score material_score(Color color) const { return _score[color]; }

  const PieceTypeArray<PieceVector>& pieces(Color color) const
  {
    return _pieces_table[color];
  }

  const PieceVector& pieces(Color color, PieceType type) const
  {
    return pieces(color)[type];
  }

  Place king(Color color) const { return pieces(color, PieceType::KING)[0]; }

  const Pos& at(Place place) const { return _squares[place]; }
  const Pos& operator[](Place place) const { return at(place); }

  inline uint64_t hash_key() const { return _hash_key; }

 private:
  PieceVector& mutable_pieces(Color color, PieceType type)
  {
    return _pieces_table[color][type];
  }

  ColorArray<Score> _score;
  int _base_ply;
  int _last_irreversible_move;
  ColorArray<PieceTypeArray<PieceVector>> _pieces_table;
  BoardArray<Pos> _squares;
  uint64_t _hash_key;
};

} // namespace blackbit
