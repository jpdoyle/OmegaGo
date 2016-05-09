#ifndef BOT_HPP
#define BOT_HPP

#include <random>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include "Util.hpp"
#include "Board.hpp"
#include "PolicyNet.hpp"
#include "MonteCarloTree.hpp"
#include "AsyncPolicy.hpp"

using seconds = std::chrono::duration<double>;

class Bot {
protected:
    ptr<Board> b;
    bool isBlack = false;
    static std::random_device r;
    std::default_random_engine randEng;

public:

    Bot() : randEng(r()) {}

    virtual void newBoard(Board&& b_) {
        if(!b) {
            b.reset(new Board(std::move(b_)));
        } else {
            *b = std::move(b_);
        }
    }
    virtual void setColor(bool isBlack_) {
        isBlack = isBlack_;
    }
    virtual Board::Move getMove()=0;

    void run();
};

class RandomBot : public Bot {
public:

    Board::Move getMove();
};

class OneMoveBot : public Bot {

public:

    Board::Move getMove();
};

class MonteCarloBot : public Bot {
    seconds moveDelay;
    std::atomic<bool>   done;

    std::atomic<MonteCarloNode*> root;
    std::atomic<size_t> numSims;

    std::vector<ptr<std::thread>> searchThreads;

    std::atomic<bool> deletion;
    std::mutex deleteLock;
    std::condition_variable tryDelete;

    std::vector<ptr<Policy>> policies;
    size_t prevEvals = 0;
    std::atomic<size_t> evals;

    void swapInRoot_(MonteCarloNode* newRoot);

public:

    explicit MonteCarloBot(size_t numThreads=1,
                           std::vector<ptr<Policy>> pols = {},
                           seconds delay = seconds(1));
    ~MonteCarloBot();

    MonteCarloBot(const MonteCarloBot&) = delete;
    MonteCarloBot operator=(const MonteCarloBot&) = delete;

    void newBoard(Board&&);
    Board::Move getMove();
};

class NeuralNetBot : public Bot {
    policy_net nn;

    const std::vector<Board::Coord>* sensibleMoves = nullptr;
    std::array<tiny_cnn::vec_t,8> features;

public:

    NeuralNetBot(std::string basePath,int whichOne);
    void newBoard(Board&&);

    Board::Move getMove();
};


#endif

