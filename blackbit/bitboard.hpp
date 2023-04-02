#pragma once

#include <algorithm>
#include <array>
#include <cstdio>
#include <stdint.h>

#include "board_array.hpp"
#include "color_array.hpp"
#include "debug.hpp"
#include "pieces.hpp"
#include "place.hpp"

namespace blackbit {

constexpr uint32_t col_mask = 0x01010101u;

constexpr uint32_t col_rotate_code = 0x10204080u;

constexpr uint32_t diag1_mask[][2] = {
  {0x00000001u, 0x00000000u},
  {0x00000102u, 0x00000000u},
  {0x00010204u, 0x00000000u},
  {0x01020408u, 0x00000000u},
  {0x02040810u, 0x00000001u},
  {0x04081020u, 0x00000102u},
  {0x08102040u, 0x00010204u},
  {0x10204080u, 0x01020408u},
  {0x20408000u, 0x02040810u},
  {0x40800000u, 0x04081020u},
  {0x80000000u, 0x08102040u},
  {0x00000000u, 0x10204080u},
  {0x00000000u, 0x20408000u},
  {0x00000000u, 0x40800000u},
  {0x00000000u, 0x80000000u}};

const static uint32_t diag2_mask[][2] = {
  {0x00000080u, 0x00000000u},
  {0x00008040u, 0x00000000u},
  {0x00804020u, 0x00000000u},
  {0x80402010u, 0x00000000u},
  {0x40201008u, 0x00000080u},
  {0x20100804u, 0x00008040u},
  {0x10080402u, 0x00804020u},
  {0x08040201u, 0x80402010u},
  {0x04020100u, 0x40201008u},
  {0x02010000u, 0x20100804u},
  {0x01000000u, 0x10080402u},
  {0x00000000u, 0x08040201u},
  {0x00000000u, 0x04020100u},
  {0x00000000u, 0x02010000u},
  {0x00000000u, 0x01000000u}};

constexpr BoardArray<uint32_t> diag1_number{
  {0, 1,  2,  3,  4, 5, 6, 7, 1,  2,  3,  4,  5, 6, 7, 8,  2,  3,  4,  5, 6, 7,
   8, 9,  3,  4,  5, 6, 7, 8, 9,  10, 4,  5,  6, 7, 8, 9,  10, 11, 5,  6, 7, 8,
   9, 10, 11, 12, 6, 7, 8, 9, 10, 11, 12, 13, 7, 8, 9, 10, 11, 12, 13, 14}};

constexpr BoardArray<uint32_t> diag2_number{
  {7,  6,  5,  4,  3, 2, 1, 0, 8,  7,  6,  5,  4,  3, 2, 1,
   9,  8,  7,  6,  5, 4, 3, 2, 10, 9,  8,  7,  6,  5, 4, 3,
   11, 10, 9,  8,  7, 6, 5, 4, 12, 11, 10, 9,  8,  7, 6, 5,
   13, 12, 11, 10, 9, 8, 7, 6, 14, 13, 12, 11, 10, 9, 8, 7}};

constexpr uint32_t diag_rotate_code = 0x01010101;

class BitBoard {
 private:
  constexpr uint64_t from_places(const std::initializer_list<Place>& p)
  {
    uint64_t v = 0;
    for (Place p : p) { v |= 1ll << p.to_int(); }
    return v;
  }

 public:
  constexpr inline BitBoard() : m_board64(0) {}

  constexpr inline explicit BitBoard(uint64_t value) : m_board64(value) {}

  constexpr inline explicit BitBoard(uint32_t lower, uint32_t upper)
      : m_lower(lower), m_upper(upper)
  {}

  constexpr inline explicit BitBoard(const std::initializer_list<Place>& places)
      : m_board64(from_places(places))
  {}

  constexpr inline BitBoard operator|(const BitBoard& bb) const
  {
    return BitBoard(m_board64 | bb.m_board64);
  }

  constexpr inline BitBoard mirror() const
  {
    BitBoard out;
    for (int i = 0; i < 8; i++) { out |= get_row(i) << ((7 - i) * 8); }
    return out;
  }

  constexpr inline static ColorArray<BitBoard> mirrored_pair(
    const BitBoard& white)
  {
    return ColorArray<BitBoard>({white.mirror(), white});
  }

  constexpr inline BitBoard& operator|=(const BitBoard& bb)
  {
    m_board64 |= bb.m_board64;
    return *this;
  }

  constexpr inline BitBoard operator&(const BitBoard& bb) const
  {
    return BitBoard(m_board64 & bb.m_board64);
  }
  constexpr inline BitBoard& operator&=(const BitBoard& bb)
  {
    m_board64 &= bb.m_board64;
    return *this;
  }

  constexpr inline BitBoard operator^(const BitBoard& bb) const
  {
    return BitBoard(m_board64 ^ bb.m_board64);
  }
  constexpr inline BitBoard& operator^=(const BitBoard& bb)
  {
    m_board64 ^= bb.m_board64;
    return *this;
  }

  constexpr inline BitBoard operator>>(int shift) const
  {
    return BitBoard(m_board64 >> shift);
  }
  constexpr inline BitBoard& operator>>=(int shift)
  {
    m_board64 >>= shift;
    return *this;
  }

  constexpr inline BitBoard operator<<(int shift) const
  {
    return BitBoard(m_board64 << shift);
  }
  constexpr inline BitBoard& operator<<=(int shift)
  {
    m_board64 <<= shift;
    return *this;
  }

  constexpr inline BitBoard operator~() const { return BitBoard(~m_board64); }

  constexpr inline bool operator==(const BitBoard& bb) const
  {
    return m_board64 == bb.m_board64;
  }

  constexpr inline bool operator!=(const BitBoard& bb) const
  {
    return m_board64 != bb.m_board64;
  }

  constexpr inline BitBoard operator-(const BitBoard& bb) const
  {
    return *this & (~bb);
  }

  constexpr inline BitBoard& set(Place place)
  {
    m_board64 |= (1ull << place.to_int());
    return *this;
  }

  constexpr inline BitBoard& clear(Place place)
  {
    m_board64 &= ~(1ull << place.to_int());
    return *this;
  }

  constexpr inline BitBoard& invert(Place place)
  {
    m_board64 ^= (1ull << place.to_int());
    return *this;
  }

  constexpr inline void clear() { m_board64 = 0; }

  constexpr bool inline empty() const { return m_board64 == 0; }

  constexpr bool inline not_empty() const { return m_board64 != 0; }

  constexpr bool inline intersects(const BitBoard& other) const
  {
    return (m_board64 & other.m_board64) != 0;
  }

  constexpr inline bool is_set(Place place) const
  {
    return m_board64 & (1ull << place.to_int());
  }

  constexpr inline bool is_all_set(const BitBoard& mask) const
  {
    return ((*this) & mask) == mask;
  }

  inline bool is_not_set(Place place) const { return !is_set(place); }

  inline int pop_count() const { return pop_count64(m_board64); }

  inline int get_line(int line) const
  {
    return (m_board64 >> (line * 8)) & 0xff;
  }

  inline int get_col(int col) const
  {
    return ((((m_lower >> col) & col_mask) * col_rotate_code) >> 28) |
           ((((m_upper >> col) & col_mask) * col_rotate_code) >> 24);
  }

  constexpr inline BitBoard get_row(int row) const
  {
    return BitBoard((m_board64 >> (row * 8)) & 0xff);
  }

  inline int get_col_pop(int col) const { return pop_count8(get_col(col)); }

  inline int get_diag1(int d) const
  {
    return (((m_lower & diag1_mask[d][0]) * diag_rotate_code) >> 24) |
           (((m_upper & diag1_mask[d][1]) * diag_rotate_code) >> 24);
  }

  inline int get_diag2(int d) const
  {
    return (((m_lower & diag2_mask[d][0]) * diag_rotate_code) >> 24) |
           (((m_upper & diag2_mask[d][1]) * diag_rotate_code) >> 24);
  }

  inline int get_one_place_int() const
  {
    if (m_lower) {
      if (m_lower & 0xffff)
        return ctz_table[m_lower & 0xffff];
      else
        return ctz_table[m_lower >> 16] + 16;
    } else {
      if (m_upper & 0xffff)
        return ctz_table[m_upper & 0xffff] + 32;
      else
        return ctz_table[m_upper >> 16] + 48;
    }
    // return __builtin_ctzll(m_board64);
  }

  inline Place get_one_place() const
  {
    return Place::of_int(get_one_place_int());
  }

  inline Place pop_place()
  {
    Place p = get_one_place();
    invert(p);
    return p;
  }

  static uint8_t pop_count_table[1 << 16];
  static uint8_t ctz_table[1 << 16];

  static inline int pop_count8(uint32_t n) { return pop_count_table[n]; }

  static inline int pop_count16(uint32_t n) { return pop_count_table[n]; }

  static inline int pop_count32(uint32_t n)
  {
    return pop_count_table[n & 0xffff] + pop_count_table[n >> 16];
  }

  static inline int pop_count64(uint64_t n)
  {
    return pop_count32(n) + pop_count32(n >> 32);
  }

  static ColorArray<BoardArray<BitBoard>> pawn_moves;
  static ColorArray<BoardArray<BitBoard>> pawn_moves2;
  static ColorArray<BoardArray<BitBoard>> pawn_captures;
  static ColorArray<BoardArray<BitBoard>> pawn_promotion;
  static ColorArray<BoardArray<BitBoard>> pawn_passed_mask;

  static ColorArray<std::array<BitBoard, 9>> first_n_rows_mask;

  inline BitBoard first_n_rows(Color color, int num_rows)
  {
    return (*this) & first_n_rows_mask[color][num_rows];
  }

  static inline BitBoard column_ahead(Color color, Place place)
  {
    if (color == Color::White) {
      return BitBoard(0x0101010101010101u << place.to_int());
    } else {
      return BitBoard(0x8080808080808080u >> (63 - place.to_int()));
    }
  }

  static BoardArray<BitBoard> neighbor_col;

  static BitBoard rook_lin_moves[8][256];
  static BitBoard rook_col_moves[8][256];

  static BoardArray<BitBoard[256]> bishop_diag1_moves;
  static BoardArray<BitBoard[256]> bishop_diag2_moves;

  static BoardArray<BitBoard> knight_moves;

  static BoardArray<BitBoard> king_moves;

  static inline BitBoard get_pawn_noncapture_moves(
    Color color, Place place, BitBoard blockers)
  {
    BitBoard resp = pawn_moves[color][place] & (~blockers);
    if (resp.not_empty()) { resp |= pawn_moves2[color][place] & (~blockers); }
    return resp;
  }

  static inline BitBoard get_pawn_capture_moves(
    Color color, Place place, BitBoard blockers)
  {
    return pawn_captures[color][place] & blockers;
  }

  static inline BitBoard get_pawn_capture_promotion_moves(
    Color color, Place place, BitBoard blockers)
  {
    return (pawn_captures[color][place] & blockers) |
           (pawn_promotion[color][place] & (~blockers));
  }

  static inline BitBoard get_pawn_moves(
    Color color, Place place, BitBoard blockers)
  {
    return get_pawn_noncapture_moves(color, place, blockers) |
           get_pawn_capture_moves(color, place, blockers);
  }

  static inline BitBoard get_knight_moves(Place place)
  {
    return knight_moves[place];
  }

  static inline BitBoard get_bishop_moves(Place place, BitBoard blockers)
  {
    int diag1 = diag1_number[place], diag2 = diag2_number[place];
    int diag1_code = blockers.get_diag1(diag1);
    int diag2_code = blockers.get_diag2(diag2);

    return bishop_diag1_moves[place][diag1_code] |
           bishop_diag2_moves[place][diag2_code];
  }

  // Includes moves to blockes but not over blockers, the functions for other
  // piece moves also do
  static inline BitBoard get_rook_moves(Place place, BitBoard blockers)
  {
    int lin = place.line(), col = place.col();
    int lin_code = blockers.get_line(lin);
    int col_code = blockers.get_col(col);

    return (rook_lin_moves[col][lin_code] << (lin * 8)) |
           (rook_col_moves[lin][col_code] << col);
  }

  static inline BitBoard get_queen_moves(Place place, BitBoard blockers)
  {
    return get_rook_moves(place, blockers) | get_bishop_moves(place, blockers);
  }

  static inline BitBoard get_king_moves(Place place)
  {
    return king_moves[place];
  }

  static inline BitBoard get_passed_pawn_mask(Color color, Place place)
  {
    return pawn_passed_mask[color][place];
  }

  static inline BitBoard get_neighbor_col_mask(Place place)
  {
    return neighbor_col[place];
  }

  static inline BitBoard get_col_mask(Place place)
  {
    return BitBoard(col_mask << place.col(), col_mask << place.col());
  }

  constexpr static inline BitBoard zero() { return BitBoard(0); }

  std::string to_string();

 private:
  union {
    struct {
      uint32_t m_lower, m_upper;
    };
    uint64_t m_board64;
  };
};

} // namespace blackbit
