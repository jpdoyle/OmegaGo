#include "Bot.hpp"
#include <thread>
#include <atomic>
#include <sstream>
#include <mutex>
#include <queue>
#include <string>
#include <random>
#include <condition_variable>

std::random_device Bot::r;

void Bot::run() {
    std::atomic<bool> done;
    done.store(false);
    std::queue<std::pair<std::string,std::string>> commands;
    std::mutex commandsLock;
    std::condition_variable availCommands;

    std::thread actionThread([&,this]() {

        while(!done.load()) {
            std::string line;
            std::string rest;
            {
                std::unique_lock<std::mutex> l(commandsLock);
                availCommands.wait(l,[&done,&commands]() {
                    return done.load() || !commands.empty();
                });

                if(commands.empty()) {
                    // we're done
                    continue;
                }
                std::tie(line,rest) = std::move(commands.front());
                commands.pop();
            }

            if(line == "?") {
                auto move = getMove();
                if(!move.pass) {
                    std::cout << move.pt.first << ' ' << move.pt.second;
                }

            } else if(line == "*") {
                DBOUT(std::cerr << "Reading new board" << std::endl);
                /* std::cerr << "'''" << rest << "'''"; */
                /* std::cerr << std::endl; */
                auto b = readBoard(makeStream(rest));

                if(b) {
                    newBoard(std::move(*b));
                }
                DBOUT(std::cerr << "Finished reading board: "
                                << b.get() << std::endl);
                std::cout << "+";

            } else if(line == "B") {
                setColor(true);
                std::cout << "B";

            } else if(line == "W") {
                setColor(false);
                std::cout << "W";
            }

            std::cout << std::endl;
        }
    });

    std::string line;
    while(std::getline(std::cin,line).good()) {
        if(line.empty()) {
            continue;
        }
        std::pair<std::string,std::string> item =
            { line,"" };
        if(line == "*") {
            std::string endPrefix = "W illegal:";
            std::stringstream rest("");
            bool first = true;
            while(std::getline(std::cin,line).good() &&
                    line.compare(0,endPrefix.size(),endPrefix)) {
                if(!first) {
                    rest << "\n";
                } else {
                    first = false;
                }
                rest << line;
            }
            rest << "\n" << line;
            item.second = rest.str();
        }

        {
            std::unique_lock<std::mutex> l(commandsLock);
            commands.push(std::move(item));
            availCommands.notify_all();
        }
    }

    done.store(true);
    {
        std::unique_lock<std::mutex> l(commandsLock);
        availCommands.notify_all();
    }
    actionThread.join();
}

Board::Move OneMoveBot::getMove() {
    Board::Move ret = {isBlack,true,{0,0}};
    if(b) {
        assert(b->blackToPlay == isBlack);
        auto color = isBlack ? Board::Black : Board::White;
        int maxScore = 0;
        std::vector<Board::Coord> bestMoves;
        for(size_t y = 0; y < 19; ++y) {
            for(size_t x = 0; x < 19; ++x) {
                if(b->isSensible(color,x,y)) {
                    Board orig = *b;
                    if(orig.playMoveIfLegal(x,y)) {
                        auto scores = orig.score();
                        int score = scores.first - scores.second;
                        if(!isBlack) {
                            score = -score;
                        }
                        if(bestMoves.empty() || score > maxScore) {
                            bestMoves.clear();
                            bestMoves.push_back({x,y});
                            maxScore = score;
                        } else if(score == maxScore) {
                            bestMoves.push_back({x,y});
                        }
                    }
                }
            }
        }
        if(!bestMoves.empty()) {
            ret.pass = false;
            ret.pt = bestMoves[randEng()%bestMoves.size()];
        }
    }
    return ret;
}

Board::Move RandomBot::getMove() {
    Board::Move ret = {isBlack,true,{0,0}};
    if(b) {
        assert(b->blackToPlay == isBlack);
        auto color = isBlack ? Board::Black : Board::White;
        std::vector<Board::Coord> moves;
        for(size_t y = 0; y < 19; ++y) {
            for(size_t x = 0; x < 19; ++x) {
                if(b->isLegal(color,x,y) &&
                   b->isSensible(color,x,y)) {
                    moves.push_back({x,y});
                }
            }
        }
        if(!moves.empty()) {
            size_t ix = randEng();
            auto move = moves[ix%moves.size()];
            ret.pass = false;
            ret.pt = move;
        }
    }
    return ret;
}

MonteCarloBot::MonteCarloBot(size_t numThreads,
            std::vector<ptr<Policy>> pols,seconds delay)
        : moveDelay(delay),policies(std::move(pols)) {
    evals.store(0);
    done.store(false);
    b.reset(new Board());
    deletion.store(false);
    if(!policies.empty()) {
        std::cerr << "With " << policies.size() << " async policies\n";
    }

    auto sym = randEng()%8;
    auto newRt = new MonteCarloNode(*b,sym);
    root.store(newRt);
    numSims.store(0);

    auto makePolicySearch = [this](Policy& policy) {
        return [this,&policy]() {
            std::cerr << "STARTING POLICY THREAD" << std::endl;
            std::random_device r;
            std::mt19937 rng(r());
            std::vector<double> vals;
            std::vector<MonteCarloNode*> path;

            while(!done.load()) {
                MonteCarloNode* node = root.load();
                auto searchStart = node;
                if(searchStart) {
                    searchStart->searches++;
                }

                /* std::cerr << "Searching" << std::endl; */

                while(node) {
                    bool expected = false;
                    if(!node->sentForPolicy.load() &&
                            node->sentForPolicy.compare_exchange_weak(expected,true))
                    {
                        policy.eval(*node);
                        evals++;
                        break;
                    }
                    if(node->moves.empty()) {
                        node = nullptr;
                    } else {
                        vals.clear();
                        vals.reserve(node->moves.size());
                        for(size_t i = 0; i<node->moves.size();++i) {
                            if(node->children[i].load()) {
                                vals.push_back(node->probabilities[i].load());
                            } else {
                                vals.push_back(0);
                            }
                        }
                        std::discrete_distribution<>
                            dist(vals.begin(),vals.end());
                        node = node->children[dist(rng)].load();
                    }
                }
                if(searchStart) {
                    searchStart->searches--;
                }

                /* std::cerr << "CHECKING DELETION" <<
                std::endl; */
                if(deletion.load()) {
                    std::unique_lock<std::mutex> l(deleteLock);
                    tryDelete.notify_all();
                }
            }
        };
    };

    auto makeRunSearch = [this]() {
        return [this]() {
            DBOUT(std::cerr << "STARTING SEARCH THREAD" << std::endl);
            std::random_device r;
            std::mt19937 rng(r());
            std::stack<MonteCarloNode*> path;

            while(!done.load()) {
                MonteCarloNode* startNode = root.load();

                /* std::cerr << "Searching" << std::endl; */

                if(startNode) {
                    startNode->runSimulation(rng);
                    numSims++;
                }

                /* std::cerr << "CHECKING DELETION" <<
                std::endl; */
                if(deletion.load()) {
                    std::unique_lock<std::mutex> l(deleteLock);
                    tryDelete.notify_all();
                }
            }
        };
    };


    searchThreads.resize(numThreads);
    for(auto& t: searchThreads) {
        t.reset(new std::thread(makeRunSearch()));
    }
    for(auto& p: policies) {
        searchThreads.emplace_back(
            new std::thread(makePolicySearch(*p)));
    }
}

MonteCarloBot::~MonteCarloBot() {
    done.store(true);
    for(auto& t: searchThreads) {
        t->join();
    }
    auto tree = root.load();
    if(tree) {
        delete tree;
    }
}

void MonteCarloBot::newBoard(Board&& newB_) {
    Board newB = std::move(newB_);
    DBOUT(std::cerr << "Updating\n");// << b.get());

    bool moveIn = true;
    /* if(b) { */
    /*     std::cerr << "\n" << *b << "\n"; */
    /* } else { */
    /*     std::cerr << " "; */
    /* } */

    /* DBOUT(std::cerr << " to " << newB.get()); */
    /* if(newB) { */
    /*     std::cerr << "\n" << *newB; */
    /* } */

    /* DBOUT(std::cerr << std::endl); */

    MonteCarloNode* newRoot = nullptr;
    if(b && newB.moveNumber == b->moveNumber &&
            b->cells == newB.cells) {
        return;
    } else if(b && newB.moveNumber == b->moveNumber+1) {
        Board::Move move = newB.lastMove;

        if(move.black == b->blackToPlay) {
            bool goodMove = false;

            if(move.pass) {
                goodMove = true;
                b->pass();
            } else {
                size_t x = move.pt.first,
                       y = move.pt.second;
                goodMove = b->playMoveIfLegal(x,y);
            }
            moveIn = false;

            if(goodMove) {
                auto oldRoot = root.load();
                if(move.pass) {
                    newRoot = oldRoot->pass.exchange(nullptr);
                } else {
                    auto i = std::find(oldRoot->moves.begin(),
                                       oldRoot->moves.end(),move.pt);
                    if(i != oldRoot->moves.end()) {
                        auto ix =
                            std::distance(oldRoot->moves.begin(),i);
                        newRoot =
                            oldRoot->children[ix].exchange(nullptr);
                    }
                }
            }

        }
    }
    if(moveIn) {
        Bot::newBoard(std::move(newB));
    }

    assert(!newRoot || newRoot->board.cells == b->cells);

    if(!newRoot) {
        auto sym = ((size_t)randEng())%8;
        newRoot = new MonteCarloNode(*b,sym);
    }

    swapInRoot_(newRoot);
}

void MonteCarloBot::swapInRoot_(MonteCarloNode* newRoot) {
    assert(newRoot);
    DBOUT(std::cerr << "Exposing deletion" << std::endl);
    deletion.store(true);
    DBOUT(std::cerr << "Swapping in new root" << std::endl);
    auto oldRoot = root.exchange(newRoot);

    if(!oldRoot) {
        DBOUT(std::cerr << "Old root is null. We are done" <<
                std::endl);
    } else {
        DBOUT(std::cerr << "Grabbing lock" << std::endl);
        {
            std::unique_lock<std::mutex> l(deleteLock);

            DBOUT(std::cerr << "Waiting for searches to leave root" <<
                    std::endl);
            tryDelete.wait(l,[oldRoot]() {
                return oldRoot->searches.load() == 0;
            });
            DBOUT(std::cerr << "No searches on old root, deleting" <<
                    std::endl);

            delete oldRoot;

            DBOUT(std::cerr << "Deleted" << std::endl);
        }
    }

    DBOUT(std::cerr << "deletion = false" << std::endl);
    deletion.store(false);
}

Board::Move MonteCarloBot::getMove() {
    static std::random_device r;
    static std::mt19937 rng(r());
    DBOUT(std::cerr << "WAITING FOR MOVE" << std::endl);
    // let threads search for a bit
    std::this_thread::sleep_for(moveDelay);
    std::cerr << "GRABBING MOVE: " << numSims.exchange(0)
              << " SIMULATIONS";
    if(!policies.empty()) {
        size_t newEvals = evals.load();
        std::cerr << ", " << newEvals-prevEvals << " POLICY EVALS";
        prevEvals = newEvals;
    }
    std::cerr << std::endl;

    const MonteCarloNode* n = root.load();
    if(n) {
        /* auto ret = n->bestOutcome(rng); */
        auto ret = n->mostVisits(rng);
        return ret;
    }
    return {isBlack,true,{0,0}};
}

NeuralNetBot::NeuralNetBot(std::string basePath,int whichOne) {
    loadNN(nn,basePath,whichOne);
    std::cerr << "NN has " << nn.depth() << " layers, " << nn.in_dim()
              << " inputs and " << nn.out_dim() << " outputs\n";
}

void NeuralNetBot::newBoard(Board&& newB) {
    if(!b) {
        b.reset(new Board(newB));
    } else {
        /* if(b->moveNumber + 1 == newB.moveNumber) { */
        /*     b->playLegalMove(newB.lastMove.pt.first, */
        /*                      newB.lastMove.pt.second); */
        /* } else { */
        *b = newB;
        /* } */
    }
    assert(b);
    std::cerr << "New board: " << *b << std::endl;
    sensibleMoves = b->blackToPlay ? &b->bSensibleMoves
                                   : &b->wSensibleMoves;
    std::array<Features,19*19> baseFeatures,oriented;
    extractFeatures(*b,baseFeatures);
    for(size_t i = 0; i < 8; ++i) {
        dihedralTranspose(baseFeatures,oriented,i);
        tiny_cnn::vec_t& planes = features[i];
        /* auto& os2 = std::cerr; */
        auto& outFeats = oriented;
        convertToTCNNInput(outFeats,planes);


        /* { */
        /*     auto sym = i; */
        /*     os2 << "Symmetry " << sym << ":\n"; */
        /*     os2 << "\nColors:\n"; */
        /*     for(size_t i = 0; i < 21; ++i) { */
        /*         os2 << '#'; */
        /*     } */
        /*     for(size_t y = 0; y < 19; ++y) { */
        /*         os2 << "\n#"; */
        /*         for(size_t x = 0; x < 19; ++x) { */
        /*             auto f = outFeats[IX({x,y})]; */
        /*             os2 << (f.color == EMPTY ? '_' : */
        /*                     f.color == MINE  ? 'o' : */
//                            /*f.color == ENEMY*/ '*');
        /*         } */
        /*         os2 << "#"; */
        /*     } */
        /*     os2 << "\n"; */
        /*     for(size_t i = 0; i < 21; ++i) { */
        /*         os2 << '#'; */
        /*     } */

        /*     os2 << "\n\nTurnsSince:\n"; */

        /*     for(size_t i = 0; i < 21; ++i) { */
        /*         os2 << '#'; */
        /*     } */
        /*     for(size_t y = 0; y < 19; ++y) { */
        /*         os2 << "\n#"; */
        /*         for(size_t x = 0; x < 19; ++x) { */
        /*             auto f = outFeats[IX({x,y})]; */
        /*             os2 << f.turnsSince; */
        /*         } */
        /*         os2 << "#"; */
        /*     } */
        /*     os2 << "\n"; */
        /*     for(size_t i = 0; i < 21; ++i) { */
        /*         os2 << '#'; */
        /*     } */

        /*     os2 << "\n\nLiberties:\n"; */

        /*     for(size_t i = 0; i < 21; ++i) { */
        /*         os2 << '#'; */
        /*     } */
        /*     for(size_t y = 0; y < 19; ++y) { */
        /*         os2 << "\n#"; */
        /*         for(size_t x = 0; x < 19; ++x) { */
        /*             auto f = outFeats[IX({x,y})]; */
        /*             os2 << f.liberties; */
        /*         } */
        /*         os2 << "#"; */
        /*     } */
        /*     os2 << "\n"; */
        /*     for(size_t i = 0; i < 21; ++i) { */
        /*         os2 << '#'; */
        /*     } */

        /*     os2 << "\n\nCaptureSize:\n"; */

        /*     for(size_t i = 0; i < 21; ++i) { */
        /*         os2 << '#'; */
        /*     } */
        /*     for(size_t y = 0; y < 19; ++y) { */
        /*         os2 << "\n#"; */
        /*         for(size_t x = 0; x < 19; ++x) { */
        /*             auto f = outFeats[IX({x,y})]; */
        /*             os2 << f.captureSize; */
        /*         } */
        /*         os2 << "#"; */
        /*     } */
        /*     os2 << "\n"; */
        /*     for(size_t i = 0; i < 21; ++i) { */
        /*         os2 << '#'; */
        /*     } */

        /*     os2 << "\n\nSelfAtariSize:\n"; */

        /*     for(size_t i = 0; i < 21; ++i) { */
        /*         os2 << '#'; */
        /*     } */
        /*     for(size_t y = 0; y < 19; ++y) { */
        /*         os2 << "\n#"; */
        /*         for(size_t x = 0; x < 19; ++x) { */
        /*             auto f = outFeats[IX({x,y})]; */
        /*             os2 << f.selfAtariSize; */
        /*         } */
        /*         os2 << "#"; */
        /*     } */
        /*     os2 << "\n"; */
        /*     for(size_t i = 0; i < 21; ++i) { */
        /*         os2 << '#'; */
        /*     } */

        /*     os2 << "\n\nLibertiesAfter:\n"; */

        /*     for(size_t i = 0; i < 21; ++i) { */
        /*         os2 << '#'; */
        /*     } */
        /*     for(size_t y = 0; y < 19; ++y) { */
        /*         os2 << "\n#"; */
        /*         for(size_t x = 0; x < 19; ++x) { */
        /*             auto f = outFeats[IX({x,y})]; */
        /*             os2 << f.libertiesAfter; */
        /*         } */
        /*         os2 << "#"; */
        /*     } */
        /*     os2 << "\n"; */
        /*     for(size_t i = 0; i < 21; ++i) { */
        /*         os2 << '#'; */
        /*     } */

        /*     os2 << "\n\nSensible:\n"; */

        /*     for(size_t i = 0; i < 21; ++i) { */
        /*         os2 << '#'; */
        /*     } */
        /*     for(size_t y = 0; y < 19; ++y) { */
        /*         os2 << "\n#"; */
        /*         for(size_t x = 0; x < 19; ++x) { */
        /*             auto f = outFeats[IX({x,y})]; */
        /*             os2 << (f.sensible ? 'o' : '!'); */
        /*         } */
        /*         os2 << "#"; */
        /*     } */
        /*     os2 << "\n"; */
        /*     for(size_t i = 0; i < 21; ++i) { */
        /*         os2 << '#'; */
        /*     } */

        /*     os2 << "\n"; */
        /*     for(size_t i=0; i < Features::NUM_CHANNELS;++i) { */
        /*         os2 << "\nPlane " << i << ":\n"; */
        /*         for(size_t i = 0; i < 21; ++i) { */
        /*             os2 << '#'; */
        /*         } */
        /*         for(size_t y = 0; y < 19; ++y) { */
        /*             os2 << "\n#"; */
        /*             for(size_t x = 0; x < 19; ++x) { */
        /*                 bool on = (planes[i*outFeats.size() + */
        /*                         IX({x,y})] >= 0.5); */
        /*                 os2 << (on ? '*' : '.'); */
        /*             } */
        /*             os2 << '#'; */
        /*         } */
        /*         os2 << '\n'; */
        /*         for(size_t i = 0; i < 21; ++i) { */
        /*             os2 << '#'; */
        /*         } */
        /*         os2 << '\n'; */
        /*     } */

        /* } */

    }
}

Board::Move NeuralNetBot::getMove() {
    if(!b) {
        Board b_;
        newBoard(std::move(b_));
    }
    assert(b);
    auto& sensible = *sensibleMoves;
    if(sensible.empty()) {
        return {isBlack,true,{0,0}}; // pass
    }
    std::vector<double> sensibleVals;
    sensibleVals.assign(sensible.size(),0);
    const tiny_cnn::vec_t* weights = nullptr;
    for(size_t i = 0; i < 8; ++i) {
        weights = &nn.fprop(features[i]);
        for(size_t j = 0; j < sensible.size(); ++j) {
            auto ix = IX(dihedral(sensible[j],i));
            /* std::cerr << " <" << sensible[j].first << "," */
            /*           << sensible[j].second << "> @ " << i << ": " */
            /*           << ix << " -> " << (*weights)[ix] << std::endl; */
            sensibleVals[j] += (*weights)[ix];
        }
    }

    if(sensibleVals.size() > 10) {
        std::array<double,10> top10;
        for(size_t i = 0; i < sensibleVals.size(); ++i) {
            if(i < 10) {
                top10[i] = i;
            } else {
                auto v = sensibleVals[i];
                size_t minInd = 0;
                size_t minVal = sensibleVals[top10[0]];
                for(size_t j = 1; j < 10; ++j) {
                    auto val = sensibleVals[top10[j]]
                    if(minVal > val) {
                        minInd = j;
                        minVal = val;
                    }
                }
                if(v > minVal) {
                    top10[minInd] = i;
                }
            }
        }
        for(size_t i = 0; i < sensibleVals.size(); ++i) {
            if(!std::count(top10.begin(),top10.end(),i)) {
                sensibleVals[i] = 0;
            }
        }
    }

    /* std::cerr << "My board: " << *b << std::endl; */

    /* std::cerr << "Sensible moves: \n"; */
    /* for(size_t i = 0; i < sensible.size(); ++i) { */
    /*     std::cerr << "   <" << sensible[i].first << "," */
    /*               << sensible[i].second << ">: " << sensibleVals[i] */
    /*               << std::endl; */
    /* } */

    std::discrete_distribution<>
        dist(sensibleVals.begin(),sensibleVals.end());

    return {isBlack,false,sensible[dist(randEng)]};
}

