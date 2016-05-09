#ifndef FEATURES_HPP
#define FEATURES_HPP

#include <algorithm>
#include <array>
#include <tiny_cnn/util/util.h>
#include "Board.hpp"

constexpr size_t NUM_PTS = 19*19;

enum FeatureColor {
    EMPTY,
    MINE,
    ENEMY
};

struct Features {
    // 44 + ones + zeros
    static constexpr size_t NUM_CHANNELS = (3 + 5*8 + 3);
    FeatureColor color = EMPTY; // 3
    size_t      turnsSince = 0; // 8
    size_t      liberties = 0; // 8
    size_t      captureSize = 0; // 8
    size_t      selfAtariSize = 0; // 8
    size_t      libertiesAfter = 0; // 8
    bool        sensible = false; // 1

    bool operator==(const Features& other) {
        return color == other.color &&
               turnsSince == other.turnsSince &&
               liberties == other.liberties &&
               captureSize == other.captureSize &&
               selfAtariSize == other.selfAtariSize &&
               libertiesAfter == other.libertiesAfter &&
               sensible == other.sensible;
    }
    bool operator!=(const Features& other) {
        return !(*this == other);
    }

    Features() {}
    Features(std::array<uint8_t,2> packed) {
        if(packed[0]&1) { // occupied
            color = packed[0]&(1<<1) ? MINE : ENEMY;
            turnsSince = std::min<size_t>(8,(packed[0]&(0xF<<2))>>2);
            liberties = (packed[0])>>6;
            liberties |= (packed[1]&0x3)<<2;
            liberties = std::min<size_t>(8,liberties);
            selfAtariSize = std::min<size_t>(8,(packed[1]>>2)&0xF);
        } else { // empty
            sensible = packed[0]&(1<<1);
            captureSize = std::min<size_t>(8,(packed[0]&(0xF<<2))>>2);
            selfAtariSize = (packed[0])>>6;
            selfAtariSize |= (packed[1]&0x3)<<2;
            selfAtariSize = std::min<size_t>(8,selfAtariSize);
            libertiesAfter = std::min<size_t>(8,(packed[1]&((0xF)<<2))>>2);
        }
    }

    Features(const Board& b,Board::Coord p);

    std::array<uint8_t,2> pack() const;
};

void convertToTCNNInput(const std::array<Features,19*19>& feats,
                               tiny_cnn::vec_t& out);

inline Board::Coord invdihedral(Board::Coord p,size_t sym) {
    size_t x,y;
    std::tie(x,y) = p;
    bool flip = (sym&1);
    size_t rots = (sym&(3<<1))>>1;
    if(flip) {
        y = 18-y;
    }
    for(size_t i=0;i<rots;++i) {
        std::tie(x,y) = Board::Coord{18-y,x};
    }
    return {x,y};
}

inline Board::Coord dihedral(Board::Coord p,size_t sym) {
    size_t x = p.first,y = p.second;
    bool flip = (sym&1);
    size_t rots = (sym&(3<<1))>>1;
    for(size_t i=0;i<(4-rots)%4;++i) {
        auto newX = 18 - y,
             newY = x;
        x = newX;
        y = newY;
    }
    if(flip) {
        y = 18-y;
    }
    return {x,y};
}

template<class T>
void dihedralTranspose(const std::array<T,19*19>& in,
                       std::array<T,19*19>& out,size_t sym) {
    for(size_t i = 0; i < in.size(); ++i) {
        auto p = ixToCoord(i);
        auto outInd = IX(dihedral(p,sym));
        out[outInd] = in[i];
    }
}

inline void extractFeatures(const Board& b,std::array<Features,19*19>& out){
    for(size_t i = 0; i < out.size(); ++i) {
        out[i] = Features(b,ixToCoord(i));
    }
}

/* struct FeaturePlanes { */
/*     std::array<Board::Cell,19*19> color; // 3 */
/*     std::array<size_t,19*19>      turnsSince; // 8 */
/*     std::array<size_t,19*19>      liberties; // 8 */
/*     std::array<size_t,19*19>      captureSize; // 8 */
/*     std::array<size_t,19*19>      selfAtariSize; // 8 */
/*     std::array<size_t,19*19>      libertiesAfter; // 8 */
/*     std::array<bool,19*19>        sensible; // 1 */
/* }; */


/*

   Stone colour             3   Player stone / opponent stone / empty
   Ones                     1   A constant plane filled with 1
   Turns since              8   How many turns since a move was played
   Liberties                8   Number of liberties (empty adjacent points)
   Capture size             8   How many opponent stones would be captured
   Self-atari size          8   How many of own stones would be captured
   Liberties after move     8   Number of liberties after this move is played
   Ladder capture           1   Whether a move at this point is a successful ladder capture
   Ladder escape            1   Whether a move at this point is a successful ladder escape
   Sensibleness             1   Whether a move is legal and does not fill its own eyes
   Zeros                    1   A constant plane filled with 0

   */

#endif

