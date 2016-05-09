#include <stack>
#include "MonteCarloTree.hpp"

std::atomic<size_t> MonteCarloNode::allocedNodes(0);

const std::vector<Board::Coord>& sensibleMoves(const Board& b) {
    return b.blackToPlay ? b.bSensibleMoves : b.wSensibleMoves;
}

/* static inline Board::Cell cellForColor(bool black) { */
/*     return black ? Board::Black : Board::White; */
/* } */

MonteCarloNode::MonteCarloNode(const Board& b,int sym) :
        board(b), moves(sensibleMoves(b)), isBlack(b.blackToPlay),
        orientation(sym),probabilities(moves.size()),
        outcomes(moves.size()),values(moves.size()),
        visits(moves.size()),children(moves.size()) {

    allocedNodes++;

    if(!moves.empty()) {
        for(auto& p: probabilities) {
            p.store(1.0/(moves.size()));
        }
        for(auto& o: outcomes) {
            o.store(0);
        }
        for(auto& v: values) {
            v.store(0);
        }
        for(auto& v: visits) {
            v.store(0);
        }
        for(auto& c: children) {
            c.store(nullptr);
        }
    }

    searches.store(0);
    pass.store(nullptr);
    sentForPolicy.store(false);
}

MonteCarloNode::~MonteCarloNode() {
    for(size_t i = 0; i < moves.size(); ++i) {
        auto child = children[i].exchange(nullptr);
        if(child) {
            delete child;
        }
    }

    auto passChild = pass.exchange(nullptr);
    if(passChild) {
        delete passChild;
    }
    allocedNodes--;
}

Board::Move MonteCarloNode::mostVisits(std::mt19937& rng) const {
    if(children.empty()) {
        return {isBlack,true,{0,0}};
    }
    int maxVisits = visits[0].load();
    std::vector<size_t> choices;
    choices.push_back(0);
    for(size_t i = 1; i < moves.size(); ++i) {
        int vis = visits[i].load();
        if(vis > maxVisits) {
            choices.clear();
        }
        if(vis >= maxVisits) {
            choices.push_back(i);
            maxVisits = vis;
        }
    }
    auto ix = rng();
    auto ind = choices[ix%choices.size()];
    return {isBlack,false,moves[ind]};
}

Board::Move MonteCarloNode::bestOutcome(std::mt19937& rng) const {
    DBOUT(std::cerr << allocedNodes.load() << " Nodes"
                     << std::endl);
    if(children.empty()) {
        return {isBlack,true,{0,0}};
    }
    double maxVal = values[0].load();
    std::vector<size_t> choices;
    choices.push_back(0);
    for(size_t i = 1; i < moves.size(); ++i) {
        double val = values[i].load();
        if(val > maxVal + 0.01) {
            choices.clear();
        }
        if(val >= maxVal) {
            choices.push_back(i);
            maxVal = val;
        }
    }

    size_t ind = rng();
    ind %= moves.size();
    return {isBlack,false,moves[ind]};
}

void MonteCarloNode::newOutcome(size_t edge,int val) {
    assert(edge < outcomes.size());
    if(!isBlack) {
        val = -val;
    }

    outcomes[edge] += val + virtLoss;
    visits[edge] += 1 - virtLoss;
    totalVisits += 1 - virtLoss;
    double vis = visits[edge].load();
    values[edge].store(outcomes[edge].load()/vis);
}

void MonteCarloNode::runSimulation(std::mt19937& rng) {
    /* std::cerr << "RUNNING SIMULATION" << std::endl; */
    MonteCarloNode* node = this;
    constexpr static size_t maxDepth = 500;
    std::vector<std::pair<MonteCarloNode*,int>> path;
    std::vector<int> justInds;

    Board::Coord move;
    const Board* finalBoard = nullptr;
    bool passed = false;

    size_t firstMove = board.moveNumber;
    size_t maxMove = firstMove + maxDepth;

    MonteCarloNode* parent = nullptr;
    while(node && node->board.moveNumber < maxMove &&
            path.size() < maxDepth) {
        /* std::cerr << node->board << std::endl; */
        int moveVal = -1;
        node->searches++;
        parent = node;
        std::atomic<MonteCarloNode*>* child = nullptr;
        MonteCarloNode* next = nullptr;

        if(!node->values.empty()) {

            double bonus = sqrt(std::max(0,totalVisits.load()));
            passed = false;
            double maxVal = 0;
            {
                double rawVal = node->values[0].load();
                double vis = node->visits[0].load();
                maxVal = rawVal +
                    5*node->probabilities[0].load()*bonus/(1+vis);
            }

            std::vector<size_t> maxInds;
            maxInds.push_back(0);
            for(size_t i = 1; i < node->values.size(); ++i) {
                double rawVal = node->values[i].load();
                double vis = node->visits[i].load();
                double val = rawVal +
                    5*node->probabilities[i].load()*bonus/(1+vis);
                if(val > maxVal + 0.01) {
                    maxInds.clear();
                    maxVal = val;
                }
                if(val + 0.01 > maxVal) {
                    maxInds.push_back(i);
                }
            }
            size_t ind = maxInds[((size_t)rng())%maxInds.size()];

            assert(ind < node->values.size());
            // virtual loss
            double vis = (node->visits[ind] += virtLoss);
            node->totalVisits += virtLoss;
            double out = (node->outcomes[ind] -= virtLoss);
            node->values[ind].store(out/vis);
            move = node->moves[ind];

            child = &node->children[ind];
            moveVal = ind;

        } else {
            if(passed) {
                finalBoard = &node->board;
                break;
            }
            child = &node->pass;
            passed = true;
        }

        assert(child);
        next = child->load();

        if(allocedNodes.load() < maxNodeCount && !next &&
                (moveVal < 0 ||
                     node->visits[moveVal].load() > visitThresh)) {
            Board b = node->board;
            if(moveVal >= 0) {
                b.playLegalMove(move.first,move.second);
            } else {
                b.pass();
            }

            MonteCarloNode* expected = nullptr;
            MonteCarloNode* newNode = new MonteCarloNode(
                                            b,(size_t)(rng())%8);
            if(!child->compare_exchange_strong(expected,newNode)) {
                delete newNode;
            }
            next = child->load();
        }

        justInds.push_back(moveVal);
        path.push_back({node,moveVal});
        node = next;
    }

    int outcome = 0;
    Board b;

    if(!finalBoard) {
        b = parent->board;
        auto firstMove = b.moveNumber;
        if(b.playMoveIfLegal(move.first,move.second)) {


            size_t passCount = 0;
            size_t koCount = 0;
            size_t maxMove = firstMove + 500;

            while(passCount < 2 && koCount < 4 &&
                    b.moveNumber < maxMove) {

                bool pass = false;
                Board::Coord move = {0,0};
                auto& availMoves = sensibleMoves(b);
                if(availMoves.empty()) {
                    pass = true;
                } else {
                    size_t ix = rng();
                    move = availMoves[ix%availMoves.size()];
                }

                /* std::cerr << passCount << " passes, " << koCount */
                        /* << " kos\n" << *b << std::endl; */

                if(pass) {
                    ++passCount;
                    b.pass();

                } else {
                    b.playLegalMove(move.first,move.second);
                }

                if(b.ko) {
                    ++koCount;
                } else {
                    koCount = 0;
                }
            }
        }

        finalBoard = &b;
    }
    assert(finalBoard);

    size_t bScore,wScore;
    std::tie(bScore,wScore) = finalBoard->score();

    outcome = (bScore > wScore) ? 1 : -1;

    while(!path.empty()) {
        MonteCarloNode* n;
        int moveVal;
        std::tie(n,moveVal) = path.back();
        path.pop_back();

        if(moveVal >= 0) {
            n->newOutcome((size_t)moveVal,outcome);
        }
        n->searches--;
    }
}

