#include "bitboard.hpp"

using std::array;
using std::string;

namespace blackbit {

uint8_t BitBoard::pop_count_table[1 << 16];
uint8_t BitBoard::ctz_table[1 << 16];

ColorArray<BoardArray<BitBoard>> BitBoard::pawn_moves;
ColorArray<BoardArray<BitBoard>> BitBoard::pawn_moves2;
ColorArray<BoardArray<BitBoard>> BitBoard::pawn_captures;
ColorArray<BoardArray<BitBoard>> BitBoard::pawn_promotion;
ColorArray<BoardArray<BitBoard>> BitBoard::pawn_passed_mask;

ColorArray<array<BitBoard, 9>> BitBoard::first_n_rows_mask;

BoardArray<BitBoard> BitBoard::neighbor_col;

BitBoard BitBoard::rook_lin_moves[8][256];
BitBoard BitBoard::rook_col_moves[8][256];

BoardArray<BitBoard[256]> BitBoard::bishop_diag1_moves;
BoardArray<BitBoard[256]> BitBoard::bishop_diag2_moves;

BoardArray<BitBoard> BitBoard::knight_moves;

BoardArray<BitBoard> BitBoard::king_moves;

inline bool is_valid_place(int lin, int col)
{
  return lin >= 0 and lin < 8 and col >= 0 and col < 8;
}

void init_bitboard()
{
  using namespace std;
  /* init_popcount table */
  BitBoard::pop_count_table[0] = 0;
  for (int i = 1; i < (1 << 16); ++i) {
    BitBoard::pop_count_table[i] = BitBoard::pop_count_table[i & (i - 1)] + 1;
  }

  /* init ctz table */
  for (int i = 0; i < (1 << 16); ++i) {
    BitBoard::ctz_table[i] = __builtin_ctz(i);
  }

  /* init pawn tables */
  for (int o = 0; o < 2; ++o) {
    for (auto& place : PlaceIterator()) {
      BitBoard::pawn_moves[Color(o)][place].clear();
      BitBoard::pawn_moves2[Color(o)][place].clear();
      BitBoard::pawn_captures[Color(o)][place].clear();
      BitBoard::pawn_promotion[Color(o)][place].clear();
      BitBoard::pawn_passed_mask[Color(o)][place].clear();
    }
  }

  for (int p = 8; p < 64 - 8; ++p) {
    Place place = Place::of_int(p);
    int col = place.col();
    int lin = place.line();
    /* movimento para frente */
    BitBoard::pawn_moves[Color::Black][place].set(place.down());
    BitBoard::pawn_moves[Color::White][place].set(place.up());

    if (col > 0) {
      /* captura para esquerda */
      BitBoard::pawn_captures[Color::Black][place].set(place.down().left());
      BitBoard::pawn_captures[Color::White][place].set(place.up().left());
    }

    if (col < 7) {
      /* captura para a direita */
      BitBoard::pawn_captures[Color::Black][place].set(place.down().right());
      BitBoard::pawn_captures[Color::White][place].set(place.up().right());
    }

    if (lin == 1) {
      // black promotion
      BitBoard::pawn_promotion[Color::Black][place].set(place.down());
      // white double move
      BitBoard::pawn_moves2[Color::White][place].set(place.up().up());
    }

    if (lin == 6) {
      BitBoard::pawn_promotion[Color::White][place].set(place.up());
      BitBoard::pawn_moves2[Color::Black][place].set(place.down().down());
    }

    /* set passed mask */
    for (int c1 = max(0, col - 1); c1 <= min(7, col + 1); ++c1) {
      for (int l1 = lin + 1; l1 < 8; ++l1) {
        BitBoard::pawn_passed_mask[Color::White][place].set(
          Place::of_line_of_col(l1, c1));
      }
      for (int l1 = lin - 1; l1 >= 0; --l1) {
        BitBoard::pawn_passed_mask[Color::Black][place].set(
          Place::of_line_of_col(l1, c1));
      }
    }
  }

  /* knight tables */
  static const int ndl[] = {2, 1, -1, -2, -2, -1, 1, 2};
  static const int ndc[] = {1, 2, 2, 1, -1, -2, -2, -1};
  for (int lin = 0; lin < 8; ++lin) {
    for (int col = 0; col < 8; ++col) {
      Place p = Place::of_line_of_col(lin, col);
      BitBoard::knight_moves[p].clear();
      for (int i = 0; i < 8; ++i) {
        int lin1 = lin + ndl[i], col1 = col + ndc[i];
        if (is_valid_place(lin1, col1)) {
          BitBoard::knight_moves[p].set(Place::of_line_of_col(lin1, col1));
        }
      }
    }
  }

  /* king tables */
  static const int kdl[] = {1, 1, 1, 0, -1, -1, -1, 0};
  static const int kdc[] = {1, 0, -1, -1, -1, 0, 1, 1};
  for (int lin = 0; lin < 8; ++lin) {
    for (int col = 0; col < 8; ++col) {
      Place p = Place::of_line_of_col(lin, col);
      BitBoard::king_moves[p].clear();
      for (int i = 0; i < 8; ++i) {
        int lin1 = lin + kdl[i], col1 = col + kdc[i];
        if (is_valid_place(lin1, col1)) {
          BitBoard::king_moves[p].set(Place::of_line_of_col(lin1, col1));
        }
      }
    }
  }

  /* rook tables */
  static const int rd[] = {1, -1};
  int tmp[8];
  for (int i = 0; i < 8; ++i) {
    for (int m = 0; m < (1 << 8); ++m) {
      /* unpack mask */
      for (int j = 0; j < 8; ++j) { tmp[j] = (m >> j) & 1; }
      BitBoard bb1(0), bb2(0);
      for (int j = 0; j < 2; ++j) {
        int c = 0;
        for (c = i + rd[j]; c < 8 and c >= 0 and tmp[c] == 0; c += rd[j]) {
          bb1.set(Place::of_line_of_col(0, c));
          bb2.set(Place::of_line_of_col(c, 0));
        }
        if (c < 8 and c >= 0) {
          bb1.set(Place::of_line_of_col(0, c));
          bb2.set(Place::of_line_of_col(c, 0));
        }
      }
      BitBoard::rook_lin_moves[i][m] = bb1;
      BitBoard::rook_col_moves[i][m] = bb2;
    }
  }

  /* bishop tables */
  static const int bdl1[] = {1, -1};
  static const int bdc1[] = {-1, 1};
  static const int bdl2[] = {1, -1};
  static const int bdc2[] = {1, -1};
  for (int lin = 0; lin < 8; ++lin) {
    for (int col = 0; col < 8; ++col) {
      Place p = Place::of_line_of_col(lin, col);
      for (int m = 0; m < 256; ++m) {
        /* unpack mask */
        for (int i = 0; i < 8; ++i) { tmp[i] = (m >> i) & 1; }

        int c = 0, l = 0;
        BitBoard bb(0);

        for (int i = 0; i < 2; ++i) {
          for (c = col + bdc1[i], l = lin + bdl1[i];
               is_valid_place(l, c) and tmp[c] == 0;
               c += bdc1[i], l += bdl1[i]) {
            bb.set(Place::of_line_of_col(l, c));
          }
          if (is_valid_place(l, c)) { bb.set(Place::of_line_of_col(l, c)); }
        }
        BitBoard::bishop_diag1_moves[p][m] = bb;

        bb = BitBoard::zero();
        for (int i = 0; i < 2; ++i) {
          for (c = col + bdc2[i], l = lin + bdl2[i];
               is_valid_place(l, c) and tmp[c] == 0;
               c += bdc2[i], l += bdl2[i]) {
            bb.set(Place::of_line_of_col(l, c));
          }
          if (is_valid_place(l, c)) { bb.set(Place::of_line_of_col(l, c)); }
        }
        BitBoard::bishop_diag2_moves[p][m] = bb;
      }
    }
  }

  /* init neighbor col table */
  for (int lin = 0; lin < 8; ++lin) {
    for (int col = 0; col < 8; ++col) {
      Place p = Place::of_line_of_col(lin, col);
      BitBoard bb;
      for (int l = 0; l < 8; ++l) {
        if (col > 0) bb.set(Place::of_line_of_col(l, col - 1));
        if (col < 7) bb.set(Place::of_line_of_col(l, col + 1));
      }
      BitBoard::neighbor_col[p] = bb;
    }
  }

  // Init rows masks
  BitBoard::first_n_rows_mask[Color::White][0] = BitBoard::zero();
  BitBoard::first_n_rows_mask[Color::Black][0] = BitBoard::zero();
  for (int i = 0; i < 8; i++) {
    BitBoard::first_n_rows_mask[Color::White][1].set(
      Place::of_line_of_col(0, i));

    BitBoard::first_n_rows_mask[Color::Black][1].set(
      Place::of_line_of_col(7, i));
  }
  for (int rows = 2; rows <= 8; rows++) {
    {
      BitBoard prevw = BitBoard::first_n_rows_mask[Color::White][rows - 1];
      BitBoard::first_n_rows_mask[Color::White][rows] = prevw | (prevw << 8);
    }

    {
      BitBoard prevb = BitBoard::first_n_rows_mask[Color::Black][rows - 1];
      BitBoard::first_n_rows_mask[Color::Black][rows] = prevb | (prevb >> 8);
    }
  }
}

string BitBoard::to_string()
{
  string out;
  for (int l = 7; l >= 0; --l) {
    for (int c = 0; c < 8; ++c) {
      out += '0' + this->is_set(Place::of_line_of_col(l, c));
    }
    out += '\n';
  }
  return out;
}

namespace {
struct __init_bitboard_t__ {
  __init_bitboard_t__() { init_bitboard(); }
} __init_bitboard__;
} // namespace

} // namespace blackbit
