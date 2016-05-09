#include "Features.hpp"


Features::Features(const Board& b,Board::Coord p) {
    size_t x,y;
    std::tie(x,y) = p;
    bool black = b.blackToPlay;
    Board::Cell c = b.cells[y][x];
    auto myC = black ? Board::Black : Board::White;
    if(c == Board::Empty) {
        color = EMPTY;
        sensible = black ? b.bIsSensible[IX(p)]
            : b.wIsSensible[IX(p)];

        std::array<Board::Coord,7> myLibs;
        size_t nLibs = 0;
        size_t maxLiberties = 0;
        std::array<int,4> myGrps;
        std::array<int,4> oppGrps;
        size_t nMy = 0,nOpp = 0;
        b.for_neighbors(p,[&,this](Board::Coord q) {
            auto c2 = b.cells[q.second][q.first];
            if(c2 == Board::Empty) {
                ++maxLiberties;
                myLibs[nLibs++] = q;
            } else {
                auto grp = b.groupForPoint(q.first,q.second);
                if(c2 == myC) {
                    if(!std::count(&myGrps[0],&myGrps[0]+nMy,
                            grp)) {
                        myGrps[nMy++] = grp;
                        maxLiberties += b.liberties[grp]-1;
                    }
                } else if(b.liberties[grp] == 1) {
                    if(!std::count(&oppGrps[0],&oppGrps[0]+nOpp,
                            grp)) {
                        captureSize += b.stones[grp];
                        oppGrps[nOpp++] = grp;
                    }
                    myLibs[nLibs++];
                    ++maxLiberties;
                }
            }
        });

        if(nMy) {
            for(size_t i=0;i<nMy;++i) {
                b.for_chain(ixToCoord(myGrps[i]),
                    [&](Board::Coord q,Board::Cell c2) {
                        if(q != p && nLibs < 8) {
                            bool isLib = (c2 == Board::Empty);
                            if(!isLib && c2 != myC) {
                                auto grp = b.groupForPoint(
                                    q.first, q.second);
                                if(b.liberties[grp] <= 1) {
                                    isLib = true;
                                }
                            }

                            if(isLib) {
                                if(!std::count(&myLibs[0],
                                        &myLibs[0] + nLibs,
                                        q)) {
                                    if(nLibs < 7) {
                                        myLibs[nLibs++] = q;
                                    } else {
                                        nLibs = 8;
                                    }
                                }
                            }
                        }
                    });
            }
        }
        libertiesAfter = nLibs;

        if(libertiesAfter == 1) {
            selfAtariSize = 1;
            for(size_t i=0;i<nMy;++i) {
                selfAtariSize += b.stones[myGrps[i]];
            }
        }
        captureSize = std::min<size_t>(8,captureSize);
        selfAtariSize = std::min<size_t>(8,selfAtariSize);
        libertiesAfter = std::min<size_t>(8,libertiesAfter);
    } else {
        color = (c == myC) ? MINE : ENEMY;
        turnsSince = std::min<size_t>(8,
                1 + b.moveNumber - b.turnPlayed[y][x]);
        auto grp = b.groupForPoint(x,y);
        liberties = std::min<size_t>(8,
                b.liberties[grp]);
        if(liberties <= 1) {
            selfAtariSize = std::min<size_t>(8,b.stones[grp]);
        }
    }
}

std::array<uint8_t,2> Features::pack() const {
    std::array<uint8_t,2> ret;

    if(color == EMPTY) {
        // 0
        ret[0] = 0;
        // 1
        ret[0] |= (sensible ? 1 : 0)<<1;
        // 2,3,4,5
        ret[0] |= (captureSize&0xF)<<2;
        // 6,7
        ret[0] |= (selfAtariSize&0x3)<<6;
        // 0,1
        ret[1] = (selfAtariSize>>2)&0x3;
        // 2,3,4,5
        ret[1] |= (libertiesAfter&0xF)<<2;
    } else {
        // 0
        ret[0] = 1;
        // 1
        ret[0] |= (color == MINE ? 1 : 0)<<1;
        // 2,3,4,5
        ret[0] |= (turnsSince&0xF)<<2;
        // 6,7
        ret[0] |= (liberties&0x3)<<6;
        // 0,1
        ret[1] = (liberties>>2)&0x3;

        ret[1] |= (selfAtariSize&0xF)<<2;
    }

    return ret;
}

void convertToTCNNInput(const std::array<Features,19*19>& feats,
                               tiny_cnn::vec_t& out) {
    auto channels = Features::NUM_CHANNELS;
    out.reserve(feats.size()*channels);
    out.clear();
    out.assign(feats.size(),1);
    for(size_t i = 1; i < channels; ++i) {
        for(auto f: feats) {
            bool on = false;

            if(i < 4) {
                on = ((i == 1) && (f.color == EMPTY)) ||
                     ((i == 2) && (f.color == MINE)) ||
                     ((i == 3) && (f.color == ENEMY));
            } else if(i < 11) {
                on = (f.liberties == (i-3));
            } else if(i == 11) {
                on = (f.liberties >= 8);
            } else if(i < 19) {
                on = (f.turnsSince == (i-11));
            } else if(i == 19) {
                on = (f.turnsSince >= 8);
            } else if(i < 27) {
                on = (f.captureSize == (i - 19));
            } else if(i == 27) {
                on = (f.captureSize >= 8);
            } else if(i < 35) {
                on = (f.selfAtariSize == (i - 27));
            } else if(i == 35) {
                on = (f.selfAtariSize >= 8);
            } else if(i < 43) {
                on = (f.libertiesAfter == (i - 35));
            } else if(i == 43) {
                on = (f.libertiesAfter >= 8);
            } else if(i == 44) {
                on = f.sensible;
            }

            out.push_back(on ? 1 : 0);
        }
    }
}



