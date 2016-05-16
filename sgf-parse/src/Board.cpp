#include "Board.hpp"
#include <iostream>
#include <queue>

#define FAIL return nullptr
#define MUST(x) { if(!(x)) { FAIL; } }

using namespace std;

ptr<Board> readBoard(stream nextCh) {
    Board b;
    spaces(nextCh);
    MUST(expect(nextCh,"Move "));
    size_t move = readInt(nextCh);
    b.moveNumber_ = move;

    if(move > 0) {
        MUST(expect(nextCh," ("));
        auto color = anyOf(nextCh,"BW");
        MUST(color == "W" || color == "B");
        b.lastMove_.black = (color == "B");
        MUST(expect(nextCh," "));
        if(nextCh(false) == 'P') {
            MUST(expect(nextCh,"Pass"));
            b.lastMove_.pass = true;
        } else {
            MUST(expect(nextCh,"<"));
            b.lastMove_.pass = false;
            b.lastMove_.pt.first = readInt(nextCh);
            MUST(expect(nextCh,","));
            b.lastMove_.pt.second = readInt(nextCh);
            MUST(expect(nextCh,">"));
        }
        MUST(expect(nextCh,")"));
    }

    MUST(expect(nextCh,": "));
    auto toMove = anyOf(nextCh,"BW");
    MUST(toMove == "W" || toMove == "B");
    b.blackToPlay_ = (toMove == "B");
    MUST(expect(nextCh," to move\n"));
    MUST(expect(nextCh,"B: "));
    b.whiteCaptives_ = readInt(nextCh);
    MUST(expect(nextCh,"\nW: "));
    b.blackCaptives_ = readInt(nextCh);
    MUST(expect(nextCh,"\n"));

    for(int i = 0; i < 21; ++i) {
        MUST(expect(nextCh,"#"));
    }
    MUST(expect(nextCh,"\n"));

    size_t y = 0;
    for(auto& row: b.cells_) {
        MUST(expect(nextCh,"#"));
        size_t x = 0;
        for(auto& cell: row) {
            auto space = oneOf(nextCh,"x.o*");
            MUST(space);
            if(space == 'x') {
                MUST(!b.ko_);
                b.ko_ = true;
                b.koX_ = x;
                b.koY_ = y;
            }
            cell = (space == 'o' ? Board::Black :
                    space == '*' ? Board::White : Board::Empty);
            if(cell != Board::Empty) {
                b.stones_[IX({x,y})] = 1;
            }
            ++x;
        }
        MUST(expect(nextCh,"#\n"));
        ++y;
    }
    for(int i = 0; i < 21; ++i) {
        MUST(expect(nextCh,"#"));
    }
    MUST(expect(nextCh,"\n"));

    constexpr auto dig = Board::MAX_MOVE_DIGITS;
    for(int i = 0; i < 1+21*(dig+1); ++i) {
        MUST(expect(nextCh,"#"));
    }
    MUST(expect(nextCh,"\n"));

    for(auto& row: b.turnPlayed_) {
        for(int i = 0; i < dig+1; ++i) {
            MUST(expect(nextCh,"#"));
        }
        for(auto& t: row) {
            MUST(expect(nextCh," "));
            auto val = readInt(nextCh);
            t = val;
        }
        MUST(expect(nextCh," "));
        for(int i = 0; i < dig+1; ++i) {
            MUST(expect(nextCh,"#"));
        }
        MUST(expect(nextCh,"\n"));
    }
    for(int i = 0; i < 1+21*(dig+1); ++i) {
        MUST(expect(nextCh,"#"));
    }

    for(size_t y = 0; y < 19; ++y) {
        for(size_t x = 0; x < 19; ++x) {
            auto ix = doFind(b.groups_,IX({x,y}));
            if(b.cells[y][x] == Board::Empty) {
                continue;
            }
            b.for_neighbors({x,y},[&](Board::Coord p) {
                auto ix2 = doFind(b.groups_,IX(p));
                if(ix == ix2) {
                    return;
                }
                auto other = b.cells[p.second][p.first];
                if(b.cells[y][x] == other) {
                    auto g1 = ix;
                    auto g2 = ix2;
                    ix = doUnion(b.groups_,ix,ix2);
                    ix2 = (ix == g1 ? g2 : g1);
                    b.stones_[ix] += b.stones_[ix2];
                    b.stones_[ix2] = 0;
                }
            });
        }
    }

    for(size_t i = 0; i < b.groups_.size(); ++i) {
        int ix = i;
        auto p = ixToCoord(ix);
        if(b.cells_[p.second][p.first] != Board::Empty) {
            if(doFind(b.groups_,ix) == ix) {
                size_t libs = 0;
                size_t chain = 0;
                Board::Cell color = b.cells_[p.second][p.first];
                b.for_chain(p,[&,color](Board::Coord p,
                                        Board::Cell c) {
                    if(c == Board::Empty) {
                        ++libs;
                        if(b.isSolidEye(color,p)) {
                            ++b.solidEyes_[ix];
                        }
                    } else if(c == color) {
                        ++chain;
                    }
                });
                b.liberties_[ix] = libs;
                assert(chain == b.stones[ix]);
            }
        }
    }

    std::set<Board::Coord> bIll,wIll;
    MUST(expect(nextCh,"\nB illegal:"));
    for(int i = 0; i < 2; ++i) {
        while(nextCh(false) == ' ') {
            MUST(expect(nextCh," "));
            char c = nextCh(false);
            MUST(c >= 'a' && c < 'a' + 19);
            size_t x = c - 'a';
            nextCh(true);
            c = nextCh(false);
            MUST(c >= 'a' && c < 'a' + 19);
            size_t y = c - 'a';
            nextCh(true);
            (i == 0 ? bIll : wIll).insert({x,y});
        }
        if(i == 0) {
            MUST(expect(nextCh,"\nW illegal:"));
        }
    }

    for(size_t y = 0; y < 19; ++y) {
        for(size_t x = 0; x < 19; ++x) {
            b.updateSensible_({x,y});
            bool filled = b.cells[y][x] != Board::Empty;
            bool bLegal = !bIll.count({x,y}) && !filled;
            bool wLegal = !wIll.count({x,y}) && !filled;
            /* std::cout << "Checking (" << x << "," << y << "): " */
            /*           << bLegal << "," << wLegal << std::endl; */
            /* std::cout << "Board says: " << b.isLegal(Board::Black,x,y) */
            /*           << "," << b.isLegal(Board::White,x,y) */
            /*           << std::endl; */
            MUST(bLegal == b.isLegal(Board::Black,x,y));
            MUST(wLegal == b.isLegal(Board::White,x,y));
        }
    }
    b.correctSensible_();

    return ptr<Board>(new Board(std::move(b)));
}

Board& Board::operator=(const Board& other) {
    cells_ = other.cells_;
    turnPlayed_ = other.turnPlayed_;
    ko_ = other.ko_;
    koX_ = other.koX_;
    koY_ = other.koY_;
    groups_ = other.groups_;
    liberties_ = other.liberties_;
    stones_ = other.stones_;
    bIsSensible_ = other.bIsSensible_;
    wIsSensible_ = other.wIsSensible_;
    bSensibleMoves_ = other.bSensibleMoves_;
    wSensibleMoves_ = other.wSensibleMoves_;
    whiteCaptives_ = other.whiteCaptives_;
    blackCaptives_ = other.blackCaptives_;
    blackToPlay_ = other.blackToPlay_;
    handicap_ = other.handicap_;
    moveNumber_ = other.moveNumber_;
    lastMove_ = other.lastMove_;

    return *this;
}

Board::Board(Board&& other) :
    cells_(move(other.cells_)),
    turnPlayed_(move(other.turnPlayed_)),
    ko_(move(other.ko_)),
    koX_(move(other.koX_)),
    koY_(move(other.koY_)),
    groups_(move(other.groups_)),
    liberties_(move(other.liberties_)),
    stones_(move(other.stones_)),
    bIsSensible_(move(other.bIsSensible_)),
    wIsSensible_(move(other.wIsSensible_)),
    bSensibleMoves_(move(other.bSensibleMoves_)),
    wSensibleMoves_(move(other.wSensibleMoves_)),
    whiteCaptives_(move(other.whiteCaptives_)),
    blackCaptives_(move(other.blackCaptives_)),
    blackToPlay_(move(other.blackToPlay_)),
    handicap_(move(other.handicap_)),
    moveNumber_(move(other.moveNumber_)),
    lastMove_(move(other.lastMove_)) {}

Board& Board::operator=(Board&& other) {
    cells_ = move(other.cells_);
    turnPlayed_ = move(other.turnPlayed_);
    ko_ = move(other.ko_);
    koX_ = move(other.koX_);
    koY_ = move(other.koY_);
    groups_ = move(other.groups_);
    liberties_ = move(other.liberties_);
    stones_ = move(other.stones_);
    bIsSensible_ = move(other.bIsSensible_);
    wIsSensible_ = move(other.wIsSensible_);
    bSensibleMoves_ = move(other.bSensibleMoves_);
    wSensibleMoves_ = move(other.wSensibleMoves_);
    whiteCaptives_ = move(other.whiteCaptives_);
    blackCaptives_ = move(other.blackCaptives_);
    blackToPlay_ = move(other.blackToPlay_);
    handicap_ = move(other.handicap_);
    moveNumber_ = move(other.moveNumber_);
    lastMove_ = move(other.lastMove_);
    return *this;
}


void Board::updateSensible_(Board::Coord p) {
    auto ix = IX(p);
    bool bWas = bIsSensible[ix],
         wWas = wIsSensible[ix];
    bIsSensible_[ix] = isSensible_(Black,p.first,p.second);
    wIsSensible_[ix] = isSensible_(White,p.first,p.second);
    if(bIsSensible_[ix] && !bWas) {
        bSensibleMoves_.push_back(p);
    }
    if(wIsSensible_[ix] && !wWas) {
        wSensibleMoves_.push_back(p);
    }
}

void Board::correctSensible_() {
    bSensibleMoves_.erase(
        std::remove_if(bSensibleMoves_.begin(),bSensibleMoves_.end(),
            [this](Coord p) {
                return IX(p) >= 19*19 || !bIsSensible_[IX(p)];
            }),
        bSensibleMoves_.end());
    wSensibleMoves_.erase(
        std::remove_if(wSensibleMoves_.begin(),wSensibleMoves_.end(),
            [this](Coord p) {
                return IX(p) >= 19*19 || !wIsSensible_[IX(p)];
            }),
        wSensibleMoves_.end());
}

std::pair<size_t,size_t> Board::score() const {
    std::vector<std::pair<int,int>> edges;
    std::vector<Coord> spaces;
    size_t bScore = whiteCaptives;
    size_t wScore = blackCaptives;

    for(size_t y = 0; y < 19; ++y) {
        for(size_t x = 0; x < 19; ++x) {
            switch(cells[y][x]) {
                case Black:
                    ++bScore;
                    break;
                case White:
                    ++wScore;
                    break;
                default:
                    if(x > 0 && cells[y][x-1] == Empty) {
                        edges.push_back({spaces.size()-1,
                                         spaces.size()});
                    }
                    if(y > 0 && cells[y-1][x] == Empty) {
                        Coord other = {x,y-1};
                        size_t ix = spaces.size() < 20 ? 0 :
                                 spaces.size()-20;
                        ix = std::distance(spaces.cbegin(),
                                std::find(spaces.cbegin()+ix,
                                    spaces.cend(),other));
                        edges.push_back({ix,spaces.size()});
                        assert(0 <= ix && ix < spaces.size());
                    }
                    spaces.push_back({x,y});
            }
        }
    }

    std::vector<int> chains;
    std::vector<int> colors; // -1: no one, 0: unknown, 1: B, 2: W
    chains.assign(spaces.size(),-1);
    colors.assign(spaces.size(),0);

    assert(chains.size() == spaces.size() && colors.size() ==
            spaces.size());

    for(size_t i = 0; i < spaces.size();++i) {
        Coord s = spaces[i];
        int val = 0;
        for_neighbors({s.first,s.second},[&,this](Coord p) {
            auto c = cells[p.second][p.first];
            if(c != Empty) {
                int newVal = c == Black ? 1 : 2;
                if(val != 0 && val != newVal) {
                    val = -1;
                } else {
                    val = newVal;
                }
            }
        });
        colors[i] = val;
    }

    for(auto e: edges) {
        int g1 = doFind(chains,e.first);
        int g2 = doFind(chains,e.second);
        assert(0 <= g1 && (size_t)g1 <= colors.size());
        assert(0 <= g2 && (size_t)g2 <= colors.size());
        int c1 = colors[g1];
        int c2 = colors[g2];
        if(c1 == -1 || c2 == -1 || (c1 != 0 && c2 != 0 && c1 != c2)) {
            colors[g1] = colors[g2] = -1;
        } else {
            if(c1 == 0) {
                colors[g1] = c2;
            } else {
                colors[g2] = c1;
            }
        }
        doUnion(chains,g1,g2);
    }

    for(size_t i = 0; i < colors.size();++i) {
        int ix = doFind(chains,i);
        assert(0 <= ix && (size_t)ix <= colors.size());
        auto c = colors[ix];
        if(c == 1) {
            ++bScore;
        } else if(c == 2) {
            ++wScore;
        }
    }

    return std::make_pair(2*bScore,2*wScore+15);
}

/* bool Board::isSelfAtari(Cell color,size_t x, size_t y) const { */
/*     auto libs = liberties[groupForPoint(x,y)]; */
/*     for_neighbors({x,y},[&,this](Coord p) { */
/*         if( */
/*         int grp = groupForPoint(p.first,p.second); */
/*         const auto& grpLibs = liberties[grp]; */
/*         libs.insertAll(grpLibs); */
/*     } */
/*     libs.remove({x,y}); */
/*     return libs.size() <= 1; */
/* } */

void Board::setHandicap(size_t numStones) {
    assert(moveNumber == 0);
    assert(numStones <= 9);
    handicap_ = numStones;
    reset();
    blackToPlay_ = false;
    size_t even = numStones&(~1);
    bool isOdd = (numStones&1);

    static std::array<Coord,8> pts = {{
        { 19-4,3 },    { 3,19-4 },
        { 19-4,19-4 }, { 3,3 },
        { 3, 10 },     { 19-3,10 },
        { 10, 3 },     { 10,19-3 }
    }};

    for(size_t i = 0; i < even; ++i) {
        auto p = pts[i];
        cells_[p.second][p.first] = Black;
    }
    if(isOdd) {
        cells_[9][9] = Black;
    }
}

void Board::pass() {
    bool wasKo = ko_;
    ko_ = false;
    lastMove_.black = blackToPlay_;
    lastMove_.pass = true;
    moveNumber_++;
    blackToPlay_ = !blackToPlay_;

    if(wasKo) {
        Coord k = {koX,koY};
        updateSensible_(k);
        correctSensible_();
    }

}

void Board::playMove_(Cell color,size_t x,size_t y) {
    assert(color != Empty);
    assert(blackToPlay == (color == Black));

    assert(cells[y][x] == Empty);

    cells_[y][x] = color;
    turnPlayed_[y][x] = moveNumber+1;

    Coord coord(x,y);

    lastMove_.black = (color == Black);
    lastMove_.pass = false;

    lastMove_.pt = coord;

    int grp = IX({x,y});
    groups_[grp] = -1;
    stones_[grp] = 1;

    size_t precaptures = (color == Black ? whiteCaptives_ :
                                           blackCaptives_);
    std::array<Coord,4> stoneLiberties;
    std::array<int,4> oppChains;
    size_t numStoneLiberties = 0;
    size_t numSolidEyes = 0;
    size_t numOppChains = 0;

    size_t combinedGrps = 0;
    size_t baseLiberties = 0;

    std::vector<Coord> sensibleUpdates;

    /* size_t eyes = 0; */
    size_t numNeighbors = 0;
    for_neighbors({x,y},[&,this](Coord p) {
        Cell neighbor = cells[p.second][p.first];
        auto nIdx = IX(p);
        auto nGrp = doFind(groups_,nIdx);
        if(neighbor == Empty) {
            sensibleUpdates.push_back(p);
            stoneLiberties[numStoneLiberties++] = p;
            if(isSolidEye(color,p)) {
                numSolidEyes++;
            }
        } else if(neighbor == color) {
            if(nGrp != grp) {
                auto newGrp = doUnion(groups_,grp,nGrp);
                if(newGrp == nGrp) {
                    nGrp = grp;
                    grp = newGrp;
                }
                baseLiberties += liberties_[nGrp];
                stones_[grp] += stones[nGrp];
                ++combinedGrps;
                liberties_[nGrp] = 0;
                solidEyes_[nGrp] = 0;
                stones_[nGrp] = 0;
            }
            numNeighbors++;
        } else {
            if(!std::count(oppChains.begin(),
                           oppChains.begin()+numOppChains,nGrp)) {
                --liberties_[nGrp];
                oppChains[numOppChains++] = nGrp;
            }
        }
    });

    if(!combinedGrps) {
        liberties_[grp] = numStoneLiberties;
        solidEyes_[grp] = numSolidEyes;
    /* } else if(combinedGrps == 1) { */
    /*     for(auto l: stoneLiberties) { */
    /*         bool doubled = false; */
    /*         for_neighbors({l.first,l.second},[&,this](Coord p) { */
    /*             if(p != coord && doFind(groups_,IX(p)) == grp) { */
    /*                 doubled = true; */
    /*                 break; */
    /*             } */
    /*         } */
    /*         if(!doubled) { */
    /*             ++baseLiberties; */
    /*         } */
    /*     } */
    /*     liberties_[grp] = baseLiberties - 1; */
    } else {
        size_t libs = 0;
        size_t eyes = 0;
        size_t chainStones = 0;

        for_chain(coord,[&,color](Coord p,Cell c) {
            if(c == Empty) {
                sensibleUpdates.push_back(p);
                ++libs;
                if(isSolidEye(color,p)) {
                    ++eyes;
                }
            } else if(c == color) {
                ++chainStones;
            }
        });

        liberties_[grp] = libs;
        solidEyes_[grp] = eyes;
        assert(chainStones == stones[grp]);
    }
   /* eyes_[grp] = eyes; */

    std::vector<std::pair<int,size_t>> myGroups;

    if(ko_) {
        sensibleUpdates.push_back({koX,koY});
        for_neighbors({koX,koY},[&sensibleUpdates,this](Coord p) {
            if(cells[p.second][p.first] == Empty) {
                sensibleUpdates.push_back(p);
            }
            for_neighbors(p,[&sensibleUpdates,this](Coord q) {
                if(cells[q.second][q.first] == Empty) {
                    sensibleUpdates.push_back(q);
                }
            });
        });
    }

    for(size_t i = 0; i < numOppChains; ++i) {
        auto o = oppChains[i];
        if(liberties[o] <= 2) {
            sensibleUpdates.reserve(sensibleUpdates.size()+3*stones[o]);
            size_t eyes = 0;
            for_chain(ixToCoord(o),[&,this](Coord p,Cell c) {
                if(c == color) {
                    auto g = doFind(groups_,IX(p));
                    myGroups.push_back({g,liberties[g]});
                } else {
                    if(c == Empty) {
                        if(isSolidEye(color == Black ? White : Black,
                                      p)) {
                            ++eyes;
                        }
                        sensibleUpdates.push_back(p);
                    }
                    for_neighbors(p,[&,this](Coord n) {
                        if(cells[n.second][n.first] == Empty) {
                            sensibleUpdates.push_back(n);
                        }
                    });
                }
            });
            solidEyes_[o] = eyes;
        }

        if(!liberties[o]) {
            capture(ixToCoord(o),[&,this](Coord p) {
                sensibleUpdates.push_back(p);
                for_neighbors(p,[&,this](Coord n) {
                    if(cells[n.second][n.first] == Empty) {
                        sensibleUpdates.push_back(n);
                    }
                });
            });
        }
    }

    assert(liberties[grp]);
    size_t postcaptures = (color == Black ? whiteCaptives_ :
                                            blackCaptives_);
    size_t captures = postcaptures - precaptures;
    if(captures == 1 && numNeighbors == 0 &&
       liberties[grp] == 1) {
        ko_ = true;
        for_neighbors({x,y},[&,this](Coord n) {
            if(cells[n.second][n.first] == Empty) {
                tie(koX_,koY_) = n;
            }
        });
    } else {
        ko_ = false;
    }

    blackToPlay_ = !blackToPlay_;
    moveNumber_++;

    std::array<bool,19*19> corrected;
    corrected.fill(false);

    auto correct = [&corrected,this](Coord p) {
        auto ix = IX(p);
        if(!corrected[ix]) {
            corrected[ix] = true;
            updateSensible_(p);
        }
    };

    auto end = myGroups.end();
    for(auto i = myGroups.begin(); i != end; ++i) {
        if(i->first == grp) {
            continue;
        }
        if(i->second <= 2 || liberties[i->first] <= 2) {
            for_chain(ixToCoord(i->first),
                      [&,this](Coord p,Cell c) {
                if(cells[p.second][p.first] == Empty) {
                    correct(p);
                }
                for_neighbors(p,[&,this](Coord n) {
                    if(cells[n.second][n.first] == Empty) {
                        correct(n);
                    }
                });
            });
            end = std::remove_if(i+1,end,[i](std::pair<int,size_t> p){
                return p.first == i->first;
            });
        }
    }

    if(ko_) {
        correct({koX,koY});
        for_neighbors({koX,koY},[&,this](Coord p) {
            if(cells[p.second][p.first] == Empty) {
                correct(p);
            }
            for_neighbors(p,[&,this](Coord q) {
                if(cells[q.second][q.first] == Empty) {
                    correct(q);
                }
            });
        });
    }
    correct(coord);
    for(auto c:sensibleUpdates) {
        correct(c);
    }

    correctSensible_();
}

void Board::capture(Coord p,std::function<void (Board::Coord)> f) {
    Cell color = cells[p.second][p.first];
    Cell other = (color == Black ? White : Black);
    assert(color != Empty);

    auto grp = doFind(groups_,IX(p));
    groups_[grp] = -1;

    assert(!liberties[grp]);
    assert(!liberties[grp]);

    size_t captureSize = 0;

    for_chain(p,[&,this,color,other](Coord p,Cell c) {
        assert(cells[p.second][p.first] == c);
        if(c == color) {
            ++captureSize;
            cells_[p.second][p.first] = Empty;
            f(p);
            groups_[IX(p)] = -1;
            std::vector<int> oppGrps;
            for_neighbors(p,[&,this](Coord n) {
                if(cells[n.second][n.first] == other) {
                    auto grp = doFind(groups_,IX(n));
                    if(!std::count(oppGrps.begin(),oppGrps.end(),
                                   grp)) {
                        oppGrps.push_back(grp);
                    }
                }
            });
            bool eye = isSolidEye(other,p);
            for(auto g:oppGrps) {
                ++liberties_[g];
                if(eye) {
                    ++solidEyes_[g];
                }
            }
        }
    });

    assert(captureSize == stones_[grp]);

    stones_[grp] = 0;
    solidEyes_[grp] = 0;

    if(color == White) {
        whiteCaptives_ += captureSize;
    } else {
        blackCaptives_ += captureSize;
    }
}

bool Board::playSgfMove(const SgfNode& node) {
    const auto& props = node.properties;
    Cell color = Black;
    auto p = props.find("B");
    if(p == props.end()) {
        color = White;
        p = props.find("W");
    }
    if(p == props.end()) {
        return false;
    }
    if(blackToPlay != (color == Black)) {
        return false;
    }

    auto locs = p->second;
    if(locs.size() != 1) {
        return false;
    }
    auto loc = locs[0];
    // pass
    if(loc == "" || loc == "tt") {
        pass();
        return true;
    }

    if(loc.size() != 2) {
        return false;
    }
    char col = loc[0], row = loc[1];
    if(col < 'a' || col >= ('a' + 19)) {
        return false;
    }
    if(row < 'a' || row >= ('a' + 19)) {
        return false;
    }
    return playMoveIfLegal(col-'a',row-'a');
}

