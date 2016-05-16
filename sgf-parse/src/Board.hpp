#ifndef BOARD_HPP
#define BOARD_HPP

#include "Util.hpp"
#include "Sgf.hpp"
#include <vector>
#include <array>
#include <set>
#include <map>
#include <cassert>
#include <iomanip>
#include <algorithm>

static constexpr size_t IX(std::pair<size_t,size_t> p) {
    return p.first + 19*p.second;
}

static std::pair<size_t,size_t> ixToCoord(size_t i) {
    return {(i%19),(i/19)};
}

struct Board {
    using Coord = std::pair<size_t,size_t>;
    static constexpr int MAX_MOVE_DIGITS = 3; // 1000 is still excessive

    enum Cell { Empty, Black, White };
    struct Move {
        bool black;
        bool pass;
        Coord pt;
    };

    struct CoordIota {
        Coord coord;

        CoordIota(Coord p={0,0}) : coord(p) {}

        operator Coord() {
            return coord;
        }

        CoordIota& operator++() {
            ++coord.first;
            if(coord.first >= 19) {
                coord.first = 0;
                ++coord.second;
            }
            return *this;
        }
    };

private:

    std::array<std::array<Cell,19>,19> cells_;
    std::array<std::array<size_t,19>,19> turnPlayed_;
    bool ko_ = false;
    size_t koX_ = 0,koY_ = 0;
    std::array<int,19*19> groups_; // union-find
    std::array<size_t,19*19> liberties_;
    std::array<size_t,19*19> stones_;
    std::array<size_t,19*19> solidEyes_;
    /* std::array<bool,19*19> isEyePt_; */
    std::array<bool,19*19> bIsSensible_,wIsSensible_;
    std::vector<Coord> bSensibleMoves_,wSensibleMoves_;
    size_t whiteCaptives_ = 0,blackCaptives_ = 0;
    bool blackToPlay_ = true;
    size_t handicap_ = 0;
    size_t moveNumber_ = 0;
    Move lastMove_ = { false,false,{0,0} };

    void capture(Coord p,std::function<void (Coord)>);

    void playMove_(Cell color,size_t x,size_t y);

    void updateSensible_(Board::Coord p);
    void correctSensible_();

public:

    const std::array<std::array<Cell,19>,19>& cells = cells_;
    const std::array<std::array<size_t,19>,19>&
        turnPlayed = turnPlayed_;
    const std::array<int,19*19>& groups = groups_;
    const bool& ko = ko_;
    const std::array<size_t,19*19>& liberties = liberties_;
    const std::array<size_t,19*19>& stones = stones_;
    const std::array<size_t,19*19>& solidEyes = solidEyes_;
    /* const std::array<bool,19*19>& isEyePt = isEyePt_; */
    const std::array<bool,19*19>& bIsSensible = bIsSensible_;
    const std::array<bool,19*19>& wIsSensible = wIsSensible_;
    const std::vector<Coord>& bSensibleMoves = bSensibleMoves_;
    const std::vector<Coord>& wSensibleMoves = wSensibleMoves_;
    const size_t& koX = koX_;
    const size_t& koY = koY_;
    const size_t& whiteCaptives = whiteCaptives_;
    const size_t& blackCaptives = blackCaptives_;
    const bool& blackToPlay = blackToPlay_;
    const size_t& handicap = handicap_;
    const size_t& moveNumber = moveNumber_;
    const Move& lastMove = lastMove_;

    friend ptr<Board> readBoard(stream);

    Board() {
        reset();
    }

    Board(const Board& other) {
        *this = other;
    }
    Board(Board&&);

    Board& operator=(const Board&);
    Board& operator=(Board&&);

    void reset() {
        cells_[0].fill(Empty);
        cells_.fill(cells_[0]);
        turnPlayed_[0].fill(0);
        turnPlayed_.fill(turnPlayed_[0]);
        groups_.fill(-1);
        liberties_.fill(0);
        stones_.fill(0);
        solidEyes_.fill(0);

        bIsSensible_.fill(true);
        wIsSensible_.fill(true);

        bSensibleMoves_.reserve(19*19);
        wSensibleMoves_.reserve(19*19);
        for(size_t i = 0; i < 19*19; ++i) {
            bSensibleMoves_.push_back(ixToCoord(i));
            wSensibleMoves_.push_back(ixToCoord(i));
        }

        ko_ = false;
        koX_ = koY_ = whiteCaptives_ = blackCaptives_ = 0;
        blackToPlay_ = true;
        handicap_ = 0;
        moveNumber_ = 0;
        lastMove_.black = lastMove_.pass = false;
        lastMove_.pt = { 0,0 };
    }

    bool isSensible(Cell color,size_t x,size_t y) const {
        assert(color != Empty);
        auto ix = IX({x,y});
        if(ix >= bIsSensible_.size()) {
            return false;
        }
        auto ret = (color == Black ? bIsSensible[ix] : wIsSensible[ix]);
        assert(ret == isSensible_(color,x,y));
        return ret;
    }

    bool isSensible_(Cell color,size_t x,size_t y) const {
        if(!isLegal(color,x,y) || cells[y][x] != Board::Empty) {
            return false;
        }

        auto otherColor = (color == Black ? White : Black);
        bool cap = false;
        for_neighbors({x,y},[&,this](Coord p) {
            auto c2 = cells[p.second][p.first];
            if(c2 == otherColor) {
                auto grp = groupForPoint(p.first,p.second);
                if(liberties[grp] <= 1) {
                    cap = true;
                }
            }
        });

        if(cap) { return true; }
        if(isLegal(otherColor,x,y)) {
            return true;
        }
        bool canBeEye = true;
        for_neighbors({x,y},[&,this](Coord p) {
            auto c2 = cells[p.second][p.first];
            if(c2 == otherColor) {
                canBeEye = false;
            } else if(c2 != color) {
                canBeEye = false;
            } else {
                if(solidEyes[groupForPoint(p.first,p.second)] > 2){
                    canBeEye = false;
                }
            }
        });

        return !canBeEye;
    }

    void for_neighbors(Coord p,std::function<void (Coord)> f) const {
	size_t x = p.first,y = p.second;
	for(size_t i = 0; i < 4; ++i) {
	    int dx = 0,dy = 0;
	    int offset = (i&2) ? -1 : 1;
	    if(i&1) {
		dx = offset;
	    } else {
		dy = -offset;
	    }
	    if((x == 0 && dx < 0) || (y == 0 && dy < 0) ||
		(x >= 18 && dx > 0) || (y >= 18 && dy > 0)) {
		continue;
	    }
	    f({x+dx,y+dy});
	}
    }

    void for_chain(Coord p,std::function<void (Coord,Cell)> f) const {
        std::array<bool,19*19> visited;
        visited.fill(false);
        Cell color = cells[p.second][p.first];
        std::function<void (Coord)> traverse;
        traverse = [&,this,color](Coord p) {
            bool done = false;
            while(!done) {
                done = true;
                for_neighbors(p,[&,this](Coord n) {
                    if(!visited[IX(n)]) {
                        visited[IX(n)] = true;
                        auto other = cells[n.second][n.first];
                        f(n,other);
                        if(other == color) {
                            if(done) {
                                p = n;
                                done = false;
                            } else {
                                traverse(n);
                            }
                        }
                    }
                });
            }
        };

        visited[IX(p)] = true;
        f(p,color);
        traverse(p);
    }

    void setHandicap(size_t numStones);

    bool isLegal(Cell color,size_t x,size_t y) const {
        if(color == Empty || x >= 19 || y >= 19) {
            return false;
        }
        if(cells[y][x] != Empty) {
            return false;
        }
        if(ko && koX_ == x && koY_ == y) {
            return false;
        }
        size_t libs = 0;
        for_neighbors({x,y},[&,this](Coord p) {
            Cell neighbor = cells[p.second][p.first];
            auto nGrp = doFind(groups_,IX(p));
            if(neighbor == Empty) {
                libs++;
            } else if(neighbor == color) {
                if(liberties[nGrp] > 1) {
                    libs = 2;
                }
            } else {
                if(liberties[nGrp] <= 1) {
                    libs = 2;
                }
            }
        });
        return libs != 0;
    }

    void playLegalMove(size_t x,size_t y) {
        Cell color = blackToPlay ? Black : White;
        assert(isLegal(color,x,y));
        playMove_(color,x,y);
    }

    bool playMoveIfLegal(size_t x,size_t y) {
        Cell color = blackToPlay ? Black : White;
        if(isLegal(color,x,y)) {
            playMove_(color,x,y);
            return true;
        }
        return false;
    }
    void pass();

    int groupForPoint(size_t x,size_t y) const {
        return doFind(groups_,IX({x,y}));
    }
    int groupForPoint(size_t x,size_t y) {
        return doFind(groups_,IX({x,y}));
    }

    std::pair<size_t,size_t> score() const;

    bool isSelfAtari(Cell color,size_t x, size_t y) const;

    bool playSgfMove(const SgfNode& node);

    bool isSolidEye(Cell color,Coord p) const {
        if(cells[p.second][p.first] != Empty) {
            return false;
        }

        if((p.first  > 0  && cells[p.second][p.first-1] != color) |
           (p.first  < 18 && cells[p.second][p.first+1] != color) |
           (p.second > 0  && cells[p.second-1][p.first] != color) |
           (p.second < 18 && cells[p.second+1][p.first] != color)) {
            return false;
        }

        auto isSafeCorner = [=] (Coord p) {
            auto c = cells[p.second][p.first];
            return c == color;
        };

        size_t numAnyCorners = 0;
        size_t numCorners = 0;

        if(p.first > 0) {
            if(p.second > 0) {
                ++numAnyCorners;
                if(isSafeCorner({p.first-1,p.second-1})) {
                    ++numCorners;
                }
            }
            if(p.second < 18) {
                ++numAnyCorners;
                if(isSafeCorner({p.first-1,p.second+1})) {
                    ++numCorners;
                }
            }
        }

        if(p.first < 18) {
            if(p.second > 0) {
                ++numAnyCorners;
                if(isSafeCorner({p.first+1,p.second-1})) {
                    ++numCorners;
                }
            }
            if(p.second < 18) {
                ++numAnyCorners;
                if(isSafeCorner({p.first+1,p.second+1})) {
                    ++numCorners;
                }
            }
        }

        return (numCorners == numAnyCorners || numCorners >= 3);
    }

    template<class OS> friend OS& operator<<(OS&,const Board&);
};

ptr<Board> readBoard(stream);

template<class OS>
OS& operator<<(OS& os,const Board& b) {
    os << "Move " << b.moveNumber;
    if(b.moveNumber > 0) {
        os << " (" << (b.lastMove.black ? 'B' : 'W') << " ";
        if(b.lastMove.pass) {
            os << "Pass";
        } else {
            os << "<" << b.lastMove.pt.first << ","
               << b.lastMove.pt.second << ">";
        }
        os << ")";
    }

    os << ": " << (b.blackToPlay ? 'B' : 'W') << " to move\n"
       << "B: " << b.whiteCaptives
       << "\nW: " << b.blackCaptives
       << std::endl;
    for(int i = 0; i < 21; ++i) {
        os << "#";
    }
    os << "\n";

    size_t y = 0;
    for(const auto& row: b.cells) {
        os << "#";
        size_t x = 0;
        for(auto p: row) {
            if(b.ko && b.koX == x && b.koY == y) {
                os << 'x';
            } else {
                char c = (p == Board::Empty ? '.' :
                          p == Board::Black ? 'o' : '*');
                os << c;
            }
            ++x;
        }
        os << "#\n";
        ++y;
    }
    for(int i = 0; i < 21; ++i) {
        os << "#";
    }
    os << "\n";

    constexpr auto dig = Board::MAX_MOVE_DIGITS;
    for(int i = 0; i < 1+21*(dig+1); ++i) {
        os << "#";
    }
    os << "\n";
    for(const auto& row: b.turnPlayed) {
        for(int i = 0; i < (dig+1); ++i) {
            os << "#";
        }

        for(auto t: row) {
            char s[dig+1];
            os << " ";
            auto digits = snprintf(s,sizeof(s),"%lu",t);
            if(digits < 0) {
                s[0] = '\0';
                digits = 0;
            } else {
                digits = std::min(dig,digits);
            }

            for(size_t i = digits; i < dig; ++i) {
                os << "0";
            }
            os << s;
        }

        os << " ";
        for(int i = 0; i < (dig+1); ++i) {
            os << "#";
        }
        os << "\n";
    }
    for(int i = 0; i < 1+21*(dig+1); ++i) {
        os << "#";
    }

    std::vector<Board::Coord> wIll;
    os << "\nB illegal:";
    for(size_t y=0;y<19;++y) {
        for(size_t x=0;x<19;++x) {
            if(b.cells[y][x] != Board::Empty) {
                continue;
            }
            if(!b.isLegal(Board::Black,x,y)) {
                os << " " << (char)('a' + x) << (char)('a' + y);
            }
            if(!b.isLegal(Board::White,x,y)) {
                wIll.emplace_back(x,y);
            }
        }
    }
    os << "\nW illegal:";
    for(auto c:wIll) {
        os << " " << (char)('a' + c.first) << (char)('a' + c.second);
    }
    return os;
}

#endif

